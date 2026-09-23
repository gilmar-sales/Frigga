#!/usr/bin/env python3
"""MCP stdio server exposing Freyr, Freya and Skirnir markdown documentation.

Documentation is read straight from GitHub (one tree listing per library plus
raw blob fetches) so the server works for end users who never cloned the engine
nor configured a build. Responses are cached on disk keyed by blob SHA, which
makes a refresh cost a single API call per library: raw fetches are served by
the CDN and do not count against the 60/h unauthenticated API quota.

Version pins are resolved per library from, in order: an explicit ``--ref``,
``FRIGGA_SDK_DEPS`` in the published SDK config, the ``GIT_TAG`` declared in
the engine ``CMakeLists.txt``, and finally ``main``.
"""

from __future__ import annotations

import argparse
from collections import Counter
import json
import os
from pathlib import Path
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from typing import Any, Callable

# The stdio transport is shared with the Editor bridge; keep a single copy.
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "frigga-mcp"))
from transports import StdioMcpTransport  # noqa: E402


DEFAULT_LIBRARIES = {
    "freyr": "gilmar-sales/Freyr",
    "freya": "gilmar-sales/Freya",
    "skirnir": "gilmar-sales/Skirnir",
}
API_ROOT = "https://api.github.com"
RAW_ROOT = "https://raw.githubusercontent.com"
USER_AGENT = "frigga-docs-mcp"
DEFAULT_TTL = 300.0
DEFAULT_TIMEOUT = 15.0
DEFAULT_LIMIT = 200
MAX_LIMIT = 1000

Fetcher = Callable[[str], "tuple[int, bytes]"]


class DocsError(Exception):
    """A failure the tool layer should report as an error payload."""

    def __init__(self, code: str, message: str) -> None:
        super().__init__(message)
        self.code = code
        self.message = message


def http_fetch(url: str, timeout: float = DEFAULT_TIMEOUT) -> tuple[int, bytes]:
    request = urllib.request.Request(
        url,
        headers={
            "User-Agent": USER_AGENT,
            "Accept": "application/vnd.github+json, application/json, text/plain, */*",
        },
    )
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:  # noqa: S310
            return response.status, response.read()
    except urllib.error.HTTPError as error:
        return error.code, error.read()
    except (urllib.error.URLError, TimeoutError, OSError) as error:
        raise DocsError("network", f"Request to {url} failed: {error}") from error


def default_cache_dir() -> Path:
    if os.name == "nt":
        base = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData" / "Local"))
    else:
        base = Path(os.environ.get("XDG_CACHE_HOME", Path.home() / ".cache"))
    return base / "frigga-docs"


HEADING_RE = re.compile(r"^(#{1,6})\s+(.*?)\s*#*\s*$")
FENCE_RE = re.compile(r"^\s*(```|~~~)")
TOKEN_RE = re.compile(r"[a-z0-9_]+")
CMAKE_TAG_RE = re.compile(
    r"FetchContent_Declare\(\s*([A-Za-z0-9_]+)(.*?)\)", re.DOTALL
)
CMAKE_SET_RE = re.compile(r"set\(\s*([A-Za-z0-9_]+)\s+\"([^\"]*)\"\s*\)")
CMAKE_VARIABLE_RE = re.compile(r"^\$\{([A-Za-z0-9_]+)\}$")
SDK_DEPS_RE = re.compile(r"set\(\s*FRIGGA_SDK_DEPS\s+\"([^\"]*)\"\s*\)")


def expand_cmake_value(value: str, variables: dict[str, str]) -> str | None:
    """Resolve a plain ``${VAR}`` reference declared in the same CMake file.

    Pins are often factored into ``set(FRIGGA_FREYR_TAG "v0.39.6")``; failing
    to expand them would make the server ask GitHub for a ref like
    ``${FRIGGA_FREYR_TAG}``. Unresolvable references return ``None`` so the
    caller can fall back to the next pin source instead of using garbage.
    """
    current = value.strip()
    for _ in range(8):
        match = CMAKE_VARIABLE_RE.match(current)
        if match is None:
            return current or None
        if match.group(1) not in variables:
            return None
        current = variables[match.group(1)].strip()
    return None


def slugify(title: str) -> str:
    slug = re.sub(r"[^a-z0-9\s-]", "", title.lower())
    return re.sub(r"[\s]+", "-", slug.strip())


def split_sections(text: str) -> list[dict[str, Any]]:
    """Split markdown into heading-delimited sections, ignoring fenced code."""
    sections: list[dict[str, Any]] = []
    current: dict[str, Any] = {"title": "", "level": 0, "line": 1, "body": []}
    fence: str | None = None
    for number, line in enumerate(text.splitlines(), start=1):
        fence_match = FENCE_RE.match(line)
        if fence_match:
            token = fence_match.group(1)
            if fence is None:
                fence = token
            elif token == fence:
                fence = None
            current["body"].append(line)
            continue
        heading = None if fence else HEADING_RE.match(line)
        if heading:
            sections.append(current)
            current = {
                "title": heading.group(2),
                "level": len(heading.group(1)),
                "line": number,
                "body": [],
            }
            continue
        current["body"].append(line)
    sections.append(current)
    for section in sections:
        section["body"] = "\n".join(section["body"]).strip("\n")
        section["slug"] = slugify(section["title"]) if section["title"] else ""
    return sections


def tokenize(text: str) -> list[str]:
    return TOKEN_RE.findall(text.lower())


class GitDocs:
    """Reads markdown documentation from GitHub with a disk-backed cache."""

    def __init__(
        self,
        libraries: dict[str, str],
        fetcher: Fetcher,
        cache_dir: Path,
        root: Path | None = None,
        ttl: float = DEFAULT_TTL,
        refs: dict[str, str] | None = None,
        clock: Callable[[], float] = time.time,
    ) -> None:
        self.libraries = {name.lower(): repo for name, repo in libraries.items()}
        self.fetcher = fetcher
        self.cache_dir = cache_dir
        self.root = root
        self.ttl = ttl
        self.overrides = {name.lower(): ref for name, ref in (refs or {}).items()}
        self.clock = clock
        self._ref_cache: dict[str, tuple[str, str]] = {}
        self._index: dict[tuple[str, str, str], list[dict[str, Any]]] = {}
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        (self.cache_dir / "blobs").mkdir(parents=True, exist_ok=True)

    # -- configuration -----------------------------------------------------
    def library_repo(self, name: str) -> str:
        key = name.lower()
        if key not in self.libraries:
            known = ", ".join(sorted(self.libraries))
            raise DocsError("unknown_library", f"Unknown library '{name}'. Known: {known}.")
        return self.libraries[key]

    def ref(self, name: str) -> tuple[str, str]:
        """Return (ref, source) for a library, resolving pins in priority order."""
        key = name.lower()
        self.library_repo(key)
        if key in self._ref_cache:
            return self._ref_cache[key]
        if key in self.overrides:
            resolved = (self.overrides[key], "argument")
        else:
            resolved = self._sdk_ref(key) or self._cmake_ref(key) or ("main", "default")
        self._ref_cache[key] = resolved
        return resolved

    def _read(self, path: Path) -> str | None:
        try:
            return path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            return None

    def _sdk_ref(self, name: str) -> tuple[str, str] | None:
        if self.root is None:
            return None
        candidates = [
            self.root / "Sdk" / "FriggaSdkConfig.cmake",
            self.root / "build" / "Sdk" / "FriggaSdkConfig.cmake",
        ]
        for candidate in candidates:
            content = self._read(candidate)
            if not content:
                continue
            match = SDK_DEPS_RE.search(content)
            if not match:
                continue
            for entry in match.group(1).split(";"):
                if "=" not in entry:
                    continue
                dep, ref = entry.split("=", 1)
                if dep.strip().lower() == name and ref.strip():
                    return (ref.strip(), "sdk")
        return None

    def _cmake_ref(self, name: str) -> tuple[str, str] | None:
        if self.root is None:
            return None
        content = self._read(self.root / "CMakeLists.txt")
        if not content:
            return None
        variables = {
            match.group(1): match.group(2)
            for match in CMAKE_SET_RE.finditer(content)
        }
        for match in CMAKE_TAG_RE.finditer(content):
            dep = match.group(1)
            tag = re.search(r"GIT_TAG\s+\"([^\"]+)\"", match.group(2))
            if not tag or dep.lower() != name:
                continue
            value = expand_cmake_value(tag.group(1), variables)
            if value:
                return (value, "cmake")
        return None

    # -- remote access -----------------------------------------------------
    def _repo_path(self, name: str) -> str:
        return self.library_repo(name)

    def _slugs(self, name: str) -> tuple[str, str]:
        repo = self._repo_path(name)
        ref = self.ref(name)[0]
        repo_slug = repo.replace("/", "__")
        ref_slug = urllib.parse.quote(ref, safe="")
        return repo_slug, ref_slug

    def _tree_file(self, name: str) -> Path:
        repo_slug, ref_slug = self._slugs(name)
        directory = self.cache_dir / repo_slug / ref_slug
        directory.mkdir(parents=True, exist_ok=True)
        return directory / "tree.json"

    def _load_tree(self, name: str) -> dict[str, Any] | None:
        raw = None
        try:
            raw = self._tree_file(name).read_text(encoding="utf-8")
        except OSError:
            return None
        try:
            payload = json.loads(raw)
        except ValueError:
            return None
        if not isinstance(payload, dict) or "entries" not in payload:
            return None
        return payload

    def _fetch_tree(self, name: str) -> dict[str, Any]:
        repo = self._repo_path(name)
        ref = self.ref(name)[0]
        url = f"{API_ROOT}/repos/{repo}/git/trees/{urllib.parse.quote(ref, safe='')}?recursive=1"
        status, body = self.fetcher(url)
        if status in (403, 429):
            cached = self._load_tree(name)
            if cached is not None:
                cached["stale"] = True
                return cached
            raise DocsError(
                "rate_limited",
                f"GitHub API rate limit hit while listing {repo}@{ref} "
                "(60 requests/hour without a token) and no cached copy exists.",
            )
        if status == 404:
            raise DocsError(
                "not_found",
                f"Repository {repo} or ref {ref} was not found on GitHub.",
            )
        if status != 200:
            raise DocsError("github", f"Listing {repo}@{ref} failed with HTTP {status}.")
        try:
            payload = json.loads(body.decode("utf-8"))
        except (ValueError, UnicodeDecodeError) as error:
            raise DocsError("github", f"Malformed tree response for {repo}: {error}") from error
        if payload.get("truncated"):
            raise DocsError(
                "github",
                f"Tree listing for {repo}@{ref} was truncated; cannot index all docs.",
            )
        entries = {}
        for entry in payload.get("tree", []):
            path = entry.get("path", "")
            if not path.startswith("docs/") or not path.endswith(".md"):
                continue
            if entry.get("type") != "blob":
                continue
            entries[path] = {"sha": entry.get("sha"), "size": entry.get("size", 0)}
        if not entries:
            raise DocsError("no_docs", f"{repo}@{ref} has no markdown under docs/.")
        tree = {
            "ref": ref,
            "source": self.ref(name)[1],
            "sha": payload.get("sha", ""),
            "fetched_at": self.clock(),
            "entries": entries,
            "stale": False,
        }
        try:
            self._tree_file(name).write_text(json.dumps(tree), encoding="utf-8")
        except OSError:
            pass  # An unwritable cache must not break reads.
        return tree

    def tree(self, name: str, force: bool = False) -> dict[str, Any]:
        cached = self._load_tree(name)
        if cached is None:
            return self._fetch_tree(name)
        if not force and self.clock() - float(cached.get("fetched_at", 0)) < self.ttl:
            return cached
        try:
            return self._fetch_tree(name)
        except DocsError:
            cached["stale"] = True
            return cached

    def read(self, name: str, path: str) -> str:
        tree = self.tree(name)
        entry = tree["entries"].get(path)
        if entry is None:
            raise DocsError("not_found", f"{path} is not part of {self._repo_path(name)}.")
        sha = entry.get("sha") or ""
        blob = self.cache_dir / "blobs" / sha
        try:
            return blob.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            pass
        repo = self._repo_path(name)
        ref = self.ref(name)[0]
        url = f"{RAW_ROOT}/{repo}/{ref}/{path}"
        status, body = self.fetcher(url)
        if status != 200:
            raise DocsError(
                "not_found",
                f"Fetching {url} returned HTTP {status}.",
            )
        text = body.decode("utf-8")
        try:
            blob.write_bytes(body)
        except OSError:
            pass
        return text

    # -- in-memory index ---------------------------------------------------
    def index(self, name: str) -> list[dict[str, Any]]:
        tree = self.tree(name)
        key = (name.lower(), tree["ref"], tree.get("sha", ""))
        if key in self._index:
            return self._index[key]
        pages = []
        for path in sorted(tree["entries"]):
            text = self.read(name, path)
            sections = split_sections(text)
            title = next(
                (s["title"] for s in sections if s["level"] == 1),
                Path(path).stem.replace("-", " ").title(),
            )
            pages.append(
                {
                    "path": path,
                    "title": title,
                    "sections": sections,
                }
            )
        self._index[key] = pages
        return pages

    def forget_index(self, name: str, ref: str) -> None:
        prefix = name.lower()
        for key in [k for k in self._index if k[0] == prefix and k[1] == ref]:
            self._index.pop(key, None)

    def libraries_ready(self, name: str | None = None) -> list[str]:
        if name:
            self.library_repo(name)
            return [name.lower()]
        return sorted(self.libraries)


def resolve_page_path(docs: GitDocs, spec: str, library: str | None) -> tuple[str, str]:
    """Resolve a page reference into (library, docs-relative path).

    Accepted shapes: ``freyr/docs/api/query.md``, ``freyr:docs/api/query.md``,
    ``docs/api/query.md`` and any unique suffix such as ``api/query.md``. A
    reference without a library prefix only resolves when exactly one library
    owns it, otherwise the ambiguity is reported back to the caller.
    """
    spec = spec.replace("\\", "/").strip()
    chosen = library
    if ":" in spec:
        head, spec = spec.split(":", 1)
        chosen = head
    parts = [p for p in spec.split("/") if p and p != "."]
    if not parts or any(p == ".." for p in parts):
        raise DocsError("bad_path", f"Invalid documentation path: {spec!r}")
    if parts[0].lower() in docs.libraries:
        chosen = parts[0]
        parts = parts[1:]
    relative = "/".join(parts)
    if not relative.startswith("docs/"):
        relative = f"docs/{relative}"

    if chosen is not None:
        lib = chosen.lower()
        docs.library_repo(lib)
        return lib, _match_in(docs, lib, relative, spec)

    candidates = [
        (lib, _match_in_optional(docs, lib, relative))
        for lib in sorted(docs.libraries)
    ]
    hits = [(lib, path) for lib, path in candidates if path is not None]
    if len(hits) == 1:
        return hits[0]
    if len(hits) > 1:
        listing = ", ".join(f"{lib}:{path}" for lib, path in hits)
        raise DocsError(
            "ambiguous_path",
            f"'{spec}' matches several libraries: {listing}. "
            "Prefix it with the library, e.g. 'freyr/" + spec + "'.",
        )
    raise DocsError(
        "not_found",
        f"'{spec}' not found in {', '.join(sorted(docs.libraries))}.",
    )


def _match_in_optional(docs: GitDocs, lib: str, relative: str) -> str | None:
    entries = docs.tree(lib)["entries"]
    if relative in entries:
        return relative
    suffix = [p for p in entries if p.endswith("/" + relative)]
    return suffix[0] if len(suffix) == 1 else None


def _match_in(docs: GitDocs, lib: str, relative: str, spec: str) -> str:
    path = _match_in_optional(docs, lib, relative)
    if path is not None:
        return path
    entries = docs.tree(lib)["entries"]
    suffix = [p for p in entries if p.endswith("/" + relative)]
    if len(suffix) > 1:
        raise DocsError(
            "ambiguous_path",
            f"'{spec}' matches several pages in {lib}: {', '.join(sorted(suffix)[:8])}.",
        )
    available = ", ".join(sorted(entries)[:12])
    raise DocsError(
        "not_found",
        f"'{spec}' not found in {lib} ({docs.ref(lib)[0]}). Pages: {available}.",
    )


def select_section(
    sections: list[dict[str, Any]], wanted: str
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    """Return (section, span lines source list indexes) for a slug or title."""
    wanted_norm = wanted.strip().lower()
    wanted_slug = slugify(wanted_norm)
    chosen_index = None
    for index, section in enumerate(sections):
        if section["slug"] and (section["slug"] == wanted_slug):
            chosen_index = index
            break
        if section["title"].strip().lower() == wanted_norm:
            chosen_index = index
            break
    if chosen_index is None:
        available = ", ".join(s["slug"] for s in sections if s["slug"])
        raise DocsError(
            "not_found",
            f"Section '{wanted}' not found. Available: {available}.",
        )
    section = sections[chosen_index]
    level = section["level"]
    end = len(sections)
    for index in range(chosen_index + 1, len(sections)):
        if sections[index]["level"] and sections[index]["level"] <= level:
            end = index
            break
    return section, sections[chosen_index:end]


def score_sections(
    pages: list[dict[str, Any]], query: str, limit: int
) -> list[dict[str, Any]]:
    """BM25-style ranking of sections, with a boost for title matches.

    Term frequencies are token counts rather than substring counts so that
    ``fr::Query`` and friends are scored on their identifiers, and so that a
    query of a few thousand characters stays linear in the corpus size.
    """
    terms = sorted(set(tokenize(query)))
    if not terms:
        raise DocsError("bad_query", "Search query has no usable terms.")
    units = []
    for page in pages:
        for section in page["sections"]:
            if not section["title"] and not section["body"]:
                continue
            counts: Counter[str] = Counter(tokenize(f"{section['title']}\n{section['body']}"))
            if not counts:
                continue
            title_tokens = set(tokenize(section["title"]))
            units.append((page, section, counts, title_tokens))
    if not units:
        return []
    lengths = [sum(counts.values()) for _page, _s, counts, _t in units]
    average = sum(lengths) / len(lengths)
    k1, b = 1.2, 0.75
    document_frequency = {
        term: sum(1 for _p, _s, counts, _t in units if counts.get(term)) for term in terms
    }
    hits = []
    for (page, section, counts, title_tokens), length in zip(units, lengths):
        score = 0.0
        matched = 0
        for term in terms:
            tf = counts.get(term)
            if not tf:
                continue
            matched += 1
            df = document_frequency[term]
            idf = 1.0 + (len(units) - df + 0.5) / (df + 0.5)
            score += idf * (tf * (k1 + 1)) / (tf + k1 * (1 - b + b * length / average))
            if term in title_tokens:
                score += 3.0
        if not matched:
            continue
        hits.append(
            {
                "library": None,
                "path": page["path"],
                "page_title": page["title"],
                "section": section["title"],
                "line": section["line"],
                "score": round(score, 4),
                "snippet": make_snippet(section["body"], terms),
            }
        )
    hits.sort(key=lambda hit: (-hit["score"], hit["path"], hit["line"]))
    return hits[:limit]


def make_snippet(body: str, terms: list[str]) -> str:
    lowered = body.lower()
    positions = [lowered.find(term) for term in terms]
    positions = [p for p in positions if p >= 0]
    start = max((min(positions) if positions else 0) - 120, 0)
    snippet = " ".join(body[start:start + 320].split())
    if start > 0:
        snippet = "…" + snippet
    if start + 320 < len(body):
        snippet = snippet + "…"
    return snippet


TOOLS: list[dict[str, Any]] = [
    {
        "name": "docs.catalog",
        "description": (
            "List Freyr/Freya/Skirnir documentation pages and their sections, "
            "with the exact version (ref) being served."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "library": {"type": "string", "description": "freyr | freya | skirnir"},
                "sections": {
                    "type": "boolean",
                    "description": "Include heading slugs and line numbers per page.",
                },
            },
        },
    },
    {
        "name": "docs.search",
        "description": (
            "Full-text search across the documentation; returns page, section, "
            "line number and a snippet for each hit."
        ),
        "inputSchema": {
            "type": "object",
            "required": ["query"],
            "properties": {
                "query": {"type": "string"},
                "library": {"type": "string"},
                "limit": {"type": "integer", "minimum": 1, "maximum": 50},
            },
        },
    },
    {
        "name": "docs.read",
        "description": (
            "Read a documentation page (or one section of it) as markdown. "
            "Paths accept 'freyr/docs/api/query.md', 'freyr:docs/api/query.md', "
            "or a unique suffix such as 'api/query.md'."
        ),
        "inputSchema": {
            "type": "object",
            "required": ["path"],
            "properties": {
                "path": {"type": "string"},
                "library": {"type": "string"},
                "section": {
                    "type": "string",
                    "description": "Heading slug or title to restrict the read to.",
                },
                "offset": {"type": "integer", "minimum": 0},
                "limit": {"type": "integer", "minimum": 1, "maximum": 1000},
            },
        },
    },
    {
        "name": "docs.refresh",
        "description": (
            "Re-list a library's documentation on GitHub (one API call) and drop "
            "the staleness of the cached tree; blob cache stays keyed by SHA."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {"library": {"type": "string"}},
        },
    },
]

# Cursor normalizes dotted MCP tool names in some surfaces; normalize back.
def wire_name(name: Any) -> str:
    return str(name).replace("_", ".")


class DocsService:
    """Tool implementations returning ``{ok, data}`` payloads."""

    def __init__(self, docs: GitDocs) -> None:
        self.docs = docs

    def catalog(self, arguments: dict[str, Any]) -> dict[str, Any]:
        library = arguments.get("library")
        include_sections = bool(arguments.get("sections"))
        libraries = []
        for name in self.docs.libraries_ready(library):
            tree = self.docs.tree(name)
            pages = []
            for page in self.docs.index(name):
                entry = {
                    "path": page["path"],
                    "title": page["title"],
                    "sections": len(page["sections"]),
                }
                if include_sections:
                    entry["headings"] = [
                        {
                            "title": s["title"],
                            "level": s["level"],
                            "line": s["line"],
                            "slug": s["slug"],
                        }
                        for s in page["sections"]
                        if s["title"]
                    ]
                pages.append(entry)
            libraries.append(
                {
                    "library": name,
                    "repo": self.docs.library_repo(name),
                    "ref": tree["ref"],
                    "ref_source": tree.get("source", "default"),
                    "sha": tree.get("sha", ""),
                    "stale": bool(tree.get("stale")),
                    "pages": pages,
                }
            )
        return {"ok": True, "data": {"libraries": libraries}}

    def search(self, arguments: dict[str, Any]) -> dict[str, Any]:
        query = str(arguments.get("query", "")).strip()
        if not query:
            raise DocsError("bad_query", "Search query is empty.")
        limit = int(arguments.get("limit", 10))
        limit = max(1, min(limit, 50))
        library = arguments.get("library")
        hits = []
        for name in self.docs.libraries_ready(library):
            page_hits = score_sections(self.docs.index(name), query, limit)
            for hit in page_hits:
                hit["library"] = name
            hits.extend(page_hits)
        hits.sort(key=lambda hit: (-hit["score"], hit["library"], hit["path"]))
        for hit in hits:
            hit["ref"] = self.docs.ref(hit["library"])[0]
            hit["cite"] = (
                f"{hit['library']}@{hit['ref']}:{hit['path']}:{hit['line']}"
            )
        return {
            "ok": True,
            "data": {"query": query, "count": len(hits), "hits": hits[:limit]},
        }

    def read(self, arguments: dict[str, Any]) -> dict[str, Any]:
        spec = str(arguments.get("path", "")).strip()
        if not spec:
            raise DocsError("bad_path", "path is required.")
        library, path = resolve_page_path(self.docs, spec, arguments.get("library"))
        text = self.docs.read(library, path)
        lines = text.splitlines()
        total = len(lines)
        offset = int(arguments.get("offset", 0) or 0)
        limit = int(arguments.get("limit", DEFAULT_LIMIT) or DEFAULT_LIMIT)
        limit = max(1, min(limit, MAX_LIMIT))
        ref, source = self.docs.ref(library)
        payload = {
            "library": library,
            "repo": self.docs.library_repo(library),
            "ref": ref,
            "ref_source": source,
            "path": path,
            "total_lines": total,
            "line_start": offset + 1,
            "stale": bool(self.docs.tree(library).get("stale")),
        }
        section = arguments.get("section")
        if section:
            sections = split_sections(text)
            chosen, span = select_section(sections, str(section))
            span_lines: list[str] = []
            for item in span:
                heading = f"{'#' * item['level']} {item['title']}" if item["title"] else ""
                body_lines = item["body"].splitlines()
                if heading:
                    span_lines.append(heading)
                span_lines.extend(body_lines)
            payload["section"] = chosen["title"]
            payload["line_start"] = chosen["line"]
            payload["total_lines"] = len(span_lines)
            payload["cite"] = f"{library}@{ref}:{path}:{chosen['line']}"
            lines = span_lines
            offset = 0
        end = offset + limit
        payload["offset"] = offset
        payload["limit"] = limit
        payload["truncated"] = end < len(lines)
        payload["next_offset"] = end if end < len(lines) else None
        payload["text"] = "\n".join(lines[offset:end])
        return {"ok": True, "data": payload}

    def refresh(self, arguments: dict[str, Any]) -> dict[str, Any]:
        library = arguments.get("library")
        refreshed = []
        for name in self.docs.libraries_ready(library):
            tree = self.docs.tree(name, force=True)
            self.docs.forget_index(name, tree["ref"])
            refreshed.append(
                {
                    "library": name,
                    "ref": tree["ref"],
                    "sha": tree.get("sha", ""),
                    "pages": len(tree["entries"]),
                    "stale": bool(tree.get("stale")),
                }
            )
        return {"ok": True, "data": {"refreshed": refreshed}}


class McpServer:
    def __init__(self, service: DocsService) -> None:
        self.service = service

    def handle(self, request: dict[str, Any]) -> dict[str, Any] | None:
        method = request.get("method")
        request_id = request.get("id")
        if method == "notifications/initialized":
            return None
        if method == "initialize":
            return {
                "jsonrpc": "2.0",
                "id": request_id,
                "result": {
                    "protocolVersion": request.get("params", {}).get(
                        "protocolVersion", "2024-11-05"
                    ),
                    "capabilities": {"tools": {}},
                    "serverInfo": {"name": "frigga-docs", "version": "0.1.0"},
                },
            }
        if method == "tools/list":
            return {"jsonrpc": "2.0", "id": request_id, "result": {"tools": TOOLS}}
        if method == "tools/call":
            params = request.get("params", {})
            name = params.get("name")
            arguments = params.get("arguments", {}) or {}
            handlers = {
                "docs.catalog": self.service.catalog,
                "docs.search": self.service.search,
                "docs.read": self.service.read,
                "docs.refresh": self.service.refresh,
            }
            handler = handlers.get(wire_name(name))
            if handler is None:
                return {
                    "jsonrpc": "2.0",
                    "id": request_id,
                    "error": {"code": -32602, "message": f"Unknown tool: {name}"},
                }
            try:
                payload = handler(arguments)
            except DocsError as error:
                payload = {"ok": False, "error": {"code": error.code,
                                                  "message": error.message}}
            except (TypeError, ValueError) as error:
                payload = {"ok": False, "error": {"code": "bad_arguments",
                                                  "message": str(error)}}
            return {
                "jsonrpc": "2.0",
                "id": request_id,
                "result": {
                    "isError": not payload.get("ok", False),
                    "content": [{"type": "text", "text": json.dumps(payload)}],
                },
            }
        return {
            "jsonrpc": "2.0",
            "id": request_id,
            "error": {"code": -32601, "message": f"Method not found: {method}"},
        }


def parse_refs(values: list[str]) -> dict[str, str]:
    refs: dict[str, str] = {}
    for value in values:
        if "=" not in value:
            raise DocsError("bad_arguments", f"--ref expects name=ref, got '{value}'")
        name, ref = value.split("=", 1)
        refs[name.strip().lower()] = ref.strip()
    return refs


def parse_repos(values: list[str], libraries: dict[str, str]) -> dict[str, str]:
    for value in values:
        if "=" not in value:
            raise DocsError("bad_arguments", f"--repo expects name=owner/repo, got '{value}'")
        name, repo = value.split("=", 1)
        libraries[name.strip().lower()] = repo.strip()
    return libraries


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache-dir", type=Path, default=default_cache_dir())
    parser.add_argument("--root", type=Path, default=Path.cwd(),
                        help="Directory holding CMakeLists.txt and Sdk/ used to "
                             "resolve version pins.")
    parser.add_argument("--ref", action="append", default=[],
                        help="Force a ref per library, e.g. --ref freyr=v0.39.6")
    parser.add_argument("--repo", action="append", default=[],
                        help="Override a library repository, e.g. --repo freyr=me/Freyr")
    parser.add_argument("--ttl", type=float, default=DEFAULT_TTL,
                        help="Seconds before a cached tree is revalidated.")
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT)
    parser.add_argument("--endpoint", type=Path, default=None,
                        help="Unused; accepted for symmetry with the Editor bridge.")
    args = parser.parse_args()

    libraries = dict(DEFAULT_LIBRARIES)
    parse_repos(args.repo, libraries)
    fetcher = lambda url: http_fetch(url, args.timeout)  # noqa: E731
    docs = GitDocs(
        libraries=libraries,
        fetcher=fetcher,
        cache_dir=args.cache_dir,
        root=args.root,
        ttl=args.ttl,
        refs=parse_refs(args.ref),
    )
    try:
        StdioMcpTransport().serve(McpServer(DocsService(docs)).handle)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
