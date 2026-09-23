#!/usr/bin/env python3
"""Unit tests for the Frigga documentation MCP server (no network required)."""

from __future__ import annotations

import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from server import (
    DEFAULT_LIBRARIES,
    DocsError,
    DocsService,
    GitDocs,
    McpServer,
    split_sections,
)


QUERY_MD = """# Query

`fr::Query` filters entities by component composition.

## Filter by component

```cpp
auto q = registry.Query<Position>();
```

## Notes

```bash
# not a heading
echo hi
```

The `Query` builder chains filters.
"""

INDEX_MD = """# Freyr

## The big picture

Archetype chunks feed a thread pool.
"""

FREYA_CORE = """# Core

## Frame stages

Renderer::EndScene composes the frame.
"""

SKIRNIR_INJECTION = """# Injection

## Constructor injection

Skirnir resolves constructor parameters with reflection.
"""


def _blob_sha(content: str) -> str:
    return hashlib.sha1(content.encode("utf-8")).hexdigest()


def make_tree(files: dict[str, str], sha: str = "TREE-SHA") -> dict:
    return {
        "sha": sha,
        "truncated": False,
        "tree": [
            {
                "path": path,
                "type": "blob",
                "sha": _blob_sha(content),
                "size": len(content),
            }
            for path, content in sorted(files.items())
        ],
    }


DEFAULT_FILES = {
    "freyr": (
        "gilmar-sales/Freyr",
        "v0.39.6",
        {"docs/index.md": INDEX_MD, "docs/api/query.md": QUERY_MD},
    ),
    "freya": (
        "gilmar-sales/Freya",
        "v0.58.0",
        {"docs/index.md": "# Freya\n\nRender features.\n", "docs/core.md": FREYA_CORE},
    ),
    "skirnir": (
        "gilmar-sales/Skirnir",
        "v0.23.2",
        {
            "docs/index.md": "# Skirnir\n\nIoC container.\n",
            "docs/usage/injection.md": SKIRNIR_INJECTION,
        },
    ),
}


class FakeFetcher:
    """Serves canned GitHub tree listings and raw blobs; records every call."""

    def __init__(self) -> None:
        self.calls: list[str] = []
        self.rate_limited = False
        self.trees: dict[tuple[str, str], dict] = {}
        self.blobs: dict[tuple[str, str, str], str] = {}

    def seed(self, name: str) -> None:
        repo, ref, files = DEFAULT_FILES[name]
        self.trees[(repo, ref)] = make_tree(files)
        for path, content in files.items():
            self.blobs[(repo, ref, path)] = content

    def __call__(self, url: str) -> tuple[int, bytes]:
        self.calls.append(url)
        if url.startswith("https://api.github.com/"):
            if self.rate_limited:
                return 403, b'{"message":"rate limited"}'
            rest = url.split("/git/trees/", 1)
            if len(rest) != 2:
                return 404, b"{}"
            head = rest[0].split("/repos/", 1)[1]
            ref = rest[1].split("?", 1)[0]
            tree = self.trees.get((head, ref))
            if tree is None:
                return 404, b"{}"
            return 200, json.dumps(tree).encode("utf-8")
        if url.startswith("https://raw.githubusercontent.com/"):
            owner, repo, ref, path = (
                url[len("https://raw.githubusercontent.com/"):].split("/", 3)
            )
            content = self.blobs.get((f"{owner}/{repo}", ref, path))
            if content is None:
                return 404, b"not found"
            return 200, content.encode("utf-8")
        return 404, b""

    @property
    def api_calls(self) -> list[str]:
        return [c for c in self.calls if c.startswith("https://api.github.com/")]

    @property
    def raw_calls(self) -> list[str]:
        return [c for c in self.calls if c.startswith("https://raw.githubusercontent.com/")]


class DocsFixture(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.cache = self.root / "cache"
        self.fetcher = FakeFetcher()
        for name in DEFAULT_LIBRARIES:
            self.fetcher.seed(name)

    def make_docs(self, root: Path | None = None, refs: dict | None = None,
                  ttl: float = 300.0) -> GitDocs:
        return GitDocs(
            libraries=dict(DEFAULT_LIBRARIES),
            fetcher=self.fetcher,
            cache_dir=self.cache,
            root=self.root if root is None else root,
            ttl=ttl,
            refs=refs,
        )

    @staticmethod
    def call(server: McpServer, tool: str, arguments: dict | None = None) -> dict:
        response = server.handle(
            {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "tools/call",
                "params": {"name": tool, "arguments": arguments or {}},
            }
        )
        return json.loads(response["result"]["content"][0]["text"])


class ProtocolTests(DocsFixture):
    def setUp(self) -> None:
        super().setUp()
        self.server = McpServer(DocsService(self.make_docs(refs={
            name: data[1] for name, data in DEFAULT_FILES.items()
        })))

    def test_initialize(self) -> None:
        response = self.server.handle(
            {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "initialize",
                "params": {"protocolVersion": "2024-11-05"},
            }
        )
        self.assertEqual(response["result"]["serverInfo"]["name"], "frigga-docs")
        self.assertIn("tools", response["result"]["capabilities"])

    def test_tools_list(self) -> None:
        response = self.server.handle({"jsonrpc": "2.0", "id": 2, "method": "tools/list"})
        names = {tool["name"] for tool in response["result"]["tools"]}
        self.assertEqual(
            names,
            {"docs.catalog", "docs.search", "docs.read", "docs.refresh"},
        )

    def test_underscores_are_normalized_to_dots(self) -> None:
        payload = self.call(self.server, "docs_catalog", {"library": "freyr"})
        self.assertTrue(payload["ok"], payload)

    def test_unknown_tool(self) -> None:
        response = self.server.handle(
            {
                "jsonrpc": "2.0",
                "id": 3,
                "method": "tools/call",
                "params": {"name": "docs.nope"},
            }
        )
        self.assertIn("error", response)


class RefResolutionTests(DocsFixture):
    def test_default_is_main(self) -> None:
        self.assertEqual(self.make_docs().ref("freyr"), ("main", "default"))

    def test_cmake_git_tag(self) -> None:
        (self.root / "CMakeLists.txt").write_text(
            "FetchContent_Declare(\n"
            "        freyr\n"
            "        GIT_REPOSITORY \"https://github.com/gilmar-sales/Freyr.git\"\n"
            "        GIT_TAG \"v0.39.6\"\n"
            "        GIT_SHALLOW TRUE\n"
            ")\n"
            "FetchContent_MakeAvailable(freyr)\n",
            encoding="utf-8",
        )
        self.assertEqual(self.make_docs().ref("freyr"), ("v0.39.6", "cmake"))
        self.assertEqual(self.make_docs().ref("freya"), ("main", "default"))

    def test_cmake_tag_variable_is_expanded(self) -> None:
        (self.root / "CMakeLists.txt").write_text(
            "set(FRIGGA_FREYR_TAG \"v0.39.6\")\n"
            "FetchContent_Declare(\n"
            "        freyr\n"
            "        GIT_REPOSITORY \"https://github.com/gilmar-sales/Freyr.git\"\n"
            "        GIT_TAG \"${FRIGGA_FREYR_TAG}\"\n"
            "        GIT_SHALLOW TRUE\n"
            ")\n",
            encoding="utf-8",
        )
        self.assertEqual(self.make_docs().ref("freyr"), ("v0.39.6", "cmake"))

    def test_unresolvable_tag_variable_falls_back(self) -> None:
        (self.root / "CMakeLists.txt").write_text(
            "FetchContent_Declare(freyr GIT_TAG \"${MISSING_TAG}\")\n",
            encoding="utf-8",
        )
        self.assertEqual(self.make_docs().ref("freyr"), ("main", "default"))

    def test_sdk_config_wins_over_cmake(self) -> None:
        (self.root / "CMakeLists.txt").write_text(
            "FetchContent_Declare(freyr GIT_TAG \"v0.1.0\")\n", encoding="utf-8"
        )
        sdk = self.root / "Sdk"
        sdk.mkdir()
        (sdk / "FriggaSdkConfig.cmake").write_text(
            "set(FRIGGA_SDK_VERSION \"0.10.0\")\n"
            "set(FRIGGA_SDK_DEPS \"Skirnir=v0.23.2;Freyr=v0.39.6;Freya=v0.58.0\")\n",
            encoding="utf-8",
        )
        docs = self.make_docs()
        self.assertEqual(docs.ref("freyr"), ("v0.39.6", "sdk"))
        self.assertEqual(docs.ref("skirnir"), ("v0.23.2", "sdk"))

    def test_sdk_config_under_build_dir(self) -> None:
        """The engine repository keeps the generated config in build/Sdk."""
        build_sdk = self.root / "build" / "Sdk"
        build_sdk.mkdir(parents=True)
        (build_sdk / "FriggaSdkConfig.cmake").write_text(
            "set(FRIGGA_SDK_DEPS \"Freyr=v0.39.6\")\n", encoding="utf-8"
        )
        self.assertEqual(self.make_docs().ref("freyr"), ("v0.39.6", "sdk"))

    def test_explicit_ref_wins_everywhere(self) -> None:
        sdk = self.root / "Sdk"
        sdk.mkdir()
        (sdk / "FriggaSdkConfig.cmake").write_text(
            "set(FRIGGA_SDK_DEPS \"Freyr=v0.39.6\")\n", encoding="utf-8"
        )
        docs = self.make_docs(refs={"freyr": "v9.9.9"})
        self.assertEqual(docs.ref("freyr"), ("v9.9.9", "argument"))

    def test_ref_reaches_the_tree_request(self) -> None:
        docs = self.make_docs(refs={"freyr": "v0.39.6"})
        docs.tree("freyr")
        self.assertTrue(
            any("/gilmar-sales/Freyr/git/trees/v0.39.6" in c
                for c in self.fetcher.api_calls)
        )


class CatalogTests(DocsFixture):
    def setUp(self) -> None:
        super().setUp()
        self.server = McpServer(DocsService(self.make_docs(refs={
            name: data[1] for name, data in DEFAULT_FILES.items()
        })))

    def test_lists_every_library_and_page(self) -> None:
        payload = self.call(self.server, "docs.catalog")
        self.assertTrue(payload["ok"], payload)
        libraries = {entry["library"]: entry for entry in payload["data"]["libraries"]}
        self.assertEqual(set(libraries), {"freyr", "freya", "skirnir"})
        freyr = libraries["freyr"]
        self.assertEqual(freyr["ref"], "v0.39.6")
        self.assertEqual(freyr["ref_source"], "argument")
        self.assertFalse(freyr["stale"])
        self.assertEqual(
            [page["path"] for page in freyr["pages"]],
            ["docs/api/query.md", "docs/index.md"],
        )
        self.assertEqual(freyr["pages"][0]["title"], "Query")

    def test_sections_flag_exposes_headings(self) -> None:
        payload = self.call(self.server, "docs.catalog",
                            {"library": "freyr", "sections": True})
        page = payload["data"]["libraries"][0]["pages"][0]
        slugs = [heading["slug"] for heading in page["headings"]]
        self.assertIn("query", slugs)
        self.assertIn("filter-by-component", slugs)
        # A markdown heading inside a fenced block must not become a section.
        self.assertNotIn("not-a-heading", slugs)

    def test_unknown_library(self) -> None:
        payload = self.call(self.server, "docs.catalog", {"library": "mimir"})
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"]["code"], "unknown_library")


class SplitSectionsTests(unittest.TestCase):
    def test_heading_inside_fence_is_ignored(self) -> None:
        sections = split_sections(QUERY_MD)
        titles = [section["title"] for section in sections]
        self.assertIn("Query", titles)
        self.assertIn("Filter by component", titles)
        self.assertNotIn("not a heading", titles)

    def test_line_numbers_point_at_headings(self) -> None:
        sections = split_sections(QUERY_MD)
        by_title = {s["title"]: s["line"] for s in sections if s["title"]}
        lines = QUERY_MD.splitlines()
        self.assertEqual(lines[by_title["Query"] - 1], "# Query")
        self.assertEqual(
            lines[by_title["Filter by component"] - 1], "## Filter by component"
        )

    def test_section_span_stops_at_peer_heading(self) -> None:
        sections = split_sections(QUERY_MD)
        target = next(s for s in sections if s["title"] == "Filter by component")
        peers = [s for s in sections
                 if s["level"] and s["level"] <= target["level"]]
        following = peers[peers.index(target) + 1] if len(peers) > 1 else None
        self.assertEqual(following["title"], "Notes")


class SearchTests(DocsFixture):
    def setUp(self) -> None:
        super().setUp()
        self.server = McpServer(DocsService(self.make_docs(refs={
            name: data[1] for name, data in DEFAULT_FILES.items()
        })))

    def test_hit_carries_citation(self) -> None:
        payload = self.call(self.server, "docs.search", {"query": "fr::Query components"})
        self.assertTrue(payload["ok"], payload)
        hit = next(h for h in payload["data"]["hits"]
                   if h["section"] == "Filter by component")
        self.assertEqual(hit["library"], "freyr")
        self.assertEqual(hit["path"], "docs/api/query.md")
        self.assertEqual(hit["ref"], "v0.39.6")
        self.assertEqual(hit["cite"], "freyr@v0.39.6:docs/api/query.md:5")
        self.assertIn("line", hit)
        self.assertTrue(hit["snippet"])

    def test_hit_pages_group_together(self) -> None:
        payload = self.call(self.server, "docs.search",
                            {"query": "fr::Query components"})
        hits = payload["data"]["hits"]
        self.assertTrue(hits, payload)
        self.assertEqual(hits[0]["path"], "docs/api/query.md")
        self.assertEqual(hits[0]["library"], "freyr")
        target = next(h for h in hits if h["section"] == "Filter by component")
        self.assertEqual(target["line"], 5)

    def test_title_match_ranks_first(self) -> None:
        payload = self.call(self.server, "docs.search", {"query": "injection"})
        hit = payload["data"]["hits"][0]
        self.assertEqual(hit["library"], "skirnir")
        self.assertEqual(hit["path"], "docs/usage/injection.md")

    def test_library_filter(self) -> None:
        payload = self.call(self.server, "docs.search",
                            {"query": "render frame stages", "library": "freya"})
        self.assertTrue(payload["ok"], payload)
        self.assertTrue(payload["data"]["hits"], payload)
        for hit in payload["data"]["hits"]:
            self.assertEqual(hit["library"], "freya")

    def test_empty_query(self) -> None:
        payload = self.call(self.server, "docs.search", {"query": "  "})
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"]["code"], "bad_query")


class ReadTests(DocsFixture):
    def setUp(self) -> None:
        super().setUp()
        self.server = McpServer(DocsService(self.make_docs(refs={
            name: data[1] for name, data in DEFAULT_FILES.items()
        })))

    def test_read_full_page(self) -> None:
        payload = self.call(self.server, "docs.read", {"path": "freyr/docs/api/query.md"})
        self.assertTrue(payload["ok"], payload)
        data = payload["data"]
        self.assertEqual(data["library"], "freyr")
        self.assertEqual(data["path"], "docs/api/query.md")
        self.assertEqual(data["ref"], "v0.39.6")
        self.assertEqual(data["line_start"], 1)
        self.assertTrue(data["text"].startswith("# Query"))
        self.assertFalse(data["truncated"])
        self.assertIsNone(data["next_offset"])

    def test_read_by_colon_notation_and_suffix(self) -> None:
        colon = self.call(self.server, "docs.read", {"path": "freyr:docs/api/query.md"})
        suffix = self.call(self.server, "docs.read", {"path": "api/query.md"})
        explicit = self.call(self.server, "docs.read",
                             {"path": "docs/api/query.md", "library": "freyr"})
        for payload in (colon, suffix, explicit):
            self.assertTrue(payload["ok"], payload)
            self.assertEqual(payload["data"]["path"], "docs/api/query.md")
            self.assertEqual(payload["data"]["library"], "freyr")

    def test_unique_suffix_across_libraries(self) -> None:
        payload = self.call(self.server, "docs.read", {"path": "core.md"})
        self.assertTrue(payload["ok"], payload)
        self.assertEqual(payload["data"]["library"], "freya")

    def test_ambiguous_path(self) -> None:
        payload = self.call(self.server, "docs.read", {"path": "index.md"})
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"]["code"], "ambiguous_path")

    def test_read_section_with_citation(self) -> None:
        payload = self.call(self.server, "docs.read",
                            {"path": "freyr/docs/api/query.md", "section": "filter-by-component"})
        self.assertTrue(payload["ok"], payload)
        data = payload["data"]
        self.assertEqual(data["section"], "Filter by component")
        self.assertEqual(data["line_start"], 5)
        self.assertTrue(data["text"].startswith("## Filter by component"))
        self.assertEqual(data["cite"], "freyr@v0.39.6:docs/api/query.md:5")
        # The span stops at the peer heading, so "Notes" is not included.
        self.assertNotIn("## Notes", data["text"])

    def test_read_section_by_title(self) -> None:
        payload = self.call(self.server, "docs.read",
                            {"path": "freyr/docs/api/query.md", "section": "Notes"})
        self.assertTrue(payload["ok"], payload)
        self.assertEqual(payload["data"]["section"], "Notes")

    def test_missing_section_lists_available(self) -> None:
        payload = self.call(self.server, "docs.read",
                            {"path": "freyr/docs/api/query.md", "section": "mimir"})
        self.assertFalse(payload["ok"])
        self.assertIn("filter-by-component", payload["error"]["message"])

    def test_pagination(self) -> None:
        first = self.call(self.server, "docs.read",
                          {"path": "freyr/docs/api/query.md", "limit": 3})
        self.assertTrue(first["data"]["truncated"])
        second = self.call(self.server, "docs.read",
                           {"path": "freyr/docs/api/query.md", "limit": 3,
                            "offset": first["data"]["next_offset"]})
        self.assertEqual(second["data"]["line_start"], 4)
        self.assertNotEqual(first["data"]["text"], second["data"]["text"])

    def test_rejects_path_escape(self) -> None:
        for spec in ("../../etc/passwd", "/etc/passwd", ".."):
            payload = self.call(self.server, "docs.read", {"path": spec})
            self.assertFalse(payload["ok"], spec)
            self.assertIn(payload["error"]["code"], ("bad_path", "not_found",
                                                     "ambiguous_path"))

    def test_missing_page(self) -> None:
        payload = self.call(self.server, "docs.read",
                            {"path": "freyr/docs/api/nope.md"})
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"]["code"], "not_found")


class CachingTests(DocsFixture):
    def setUp(self) -> None:
        super().setUp()
        self.refs = {name: data[1] for name, data in DEFAULT_FILES.items()}

    def test_blob_is_fetched_once(self) -> None:
        docs = self.make_docs(refs=self.refs)
        service = DocsService(docs)
        first = service.read({"path": "freyr/docs/api/query.md"})
        before = len(self.fetcher.raw_calls)
        second = service.read({"path": "freyr/docs/api/query.md"})
        self.assertEqual(len(self.fetcher.raw_calls), before)
        self.assertEqual(first["data"]["text"], second["data"]["text"])

    def test_second_tree_call_is_served_from_cache(self) -> None:
        docs = self.make_docs(refs=self.refs)
        docs.tree("freyr")
        calls = len(self.fetcher.api_calls)
        docs.tree("freyr")
        self.assertEqual(len(self.fetcher.api_calls), calls)

    def test_expired_ttl_revalidates_with_one_api_call(self) -> None:
        docs = self.make_docs(refs=self.refs, ttl=0.0)
        docs.tree("freyr")
        calls = len(self.fetcher.api_calls)
        docs.tree("freyr")
        self.assertEqual(len(self.fetcher.api_calls), calls + 1)

    def test_rate_limit_serves_stale_tree(self) -> None:
        docs = self.make_docs(refs=self.refs)
        docs.tree("freyr")
        self.fetcher.rate_limited = True
        stale = docs.tree("freyr", force=True)
        self.assertTrue(stale["stale"])
        self.assertIn("docs/api/query.md", stale["entries"])

    def test_rate_limit_without_cache_is_an_error(self) -> None:
        self.fetcher.rate_limited = True
        server = McpServer(DocsService(self.make_docs(refs=self.refs)))
        payload = self.call(server, "docs.catalog", {"library": "freyr"})
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"]["code"], "rate_limited")
        self.assertIn("rate limit", payload["error"]["message"])

    def test_missing_ref_is_reported(self) -> None:
        server = McpServer(DocsService(self.make_docs(refs={"freyr": "v9.9.9"})))
        payload = self.call(server, "docs.catalog", {"library": "freyr"})
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"]["code"], "not_found")

    def test_refresh_reports_new_listing(self) -> None:
        docs = self.make_docs(refs=self.refs, ttl=300.0)
        server = McpServer(DocsService(docs))
        payload = self.call(server, "docs.refresh", {"library": "skirnir"})
        self.assertTrue(payload["ok"], payload)
        entry = payload["data"]["refreshed"][0]
        self.assertEqual(entry["library"], "skirnir")
        self.assertEqual(entry["ref"], "v0.23.2")
        self.assertEqual(entry["pages"], 2)

    def test_truncated_tree_is_rejected(self) -> None:
        repo, ref, _files = DEFAULT_FILES["freyr"]
        self.fetcher.trees[(repo, ref)] = {"sha": "S", "truncated": True, "tree": []}
        server = McpServer(DocsService(self.make_docs(refs={"freyr": ref})))
        payload = self.call(server, "docs.catalog", {"library": "freyr"})
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["error"]["code"], "github")
        self.assertIn("truncated", payload["error"]["message"])


class HelperTests(unittest.TestCase):
    def test_docs_error_carries_code(self) -> None:
        error = DocsError("bad_path", "nope")
        self.assertEqual(error.code, "bad_path")
        self.assertEqual(str(error), "nope")


if __name__ == "__main__":
    unittest.main(verbosity=2)
