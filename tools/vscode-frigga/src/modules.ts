import * as os from "os";
import * as path from "path";
import * as vscode from "vscode";
import { FriggaProject, readTextFile } from "./project";
import {
  FriggaProjectDescriptor,
  ProjectModuleEntry,
  loadProjectDescriptor,
} from "./projectFile";
import { pathExistsUri } from "./moduleModuleEdit";

export interface ModuleManifest {
  id: string;
  name: string;
  target: string;
  libraryRelative: string;
  entry?: string;
  bundled?: boolean;
}

export interface FriggaModule {
  id: string;
  name: string;
  target: string;
  enabled: boolean;
  root: vscode.Uri;
  entryFile: vscode.Uri;
  cmakeFile: vscode.Uri;
  libraryRelative: string;
  bundled: boolean;
}

export function sanitizeModuleId(raw: string): string {
  let out = raw.toLowerCase().replace(/[^a-z0-9_-]+/g, "");
  out = out.replace(/^[-_]+/, "");
  if (!out) {
    return "module";
  }
  if (/^\d/.test(out)) {
    out = `m${out}`;
  }
  return out;
}

export function toPascalCase(raw: string): string {
  let out = "";
  let upper = true;
  for (const ch of raw) {
    if (!/[a-zA-Z0-9]/.test(ch)) {
      upper = true;
      continue;
    }
    out += upper ? ch.toUpperCase() : ch.toLowerCase();
    upper = false;
  }
  return out || "Module";
}

export function defaultLibraryRelative(target: string): string {
  switch (process.platform) {
    case "win32":
      return `build/${target}.dll`;
    case "darwin":
      return `build/lib${target}.dylib`;
    default:
      return `build/lib${target}.so`;
  }
}

export function defaultModulesLibraryDir(): string {
  return path.join(os.homedir(), "Frigga", "Modules");
}

export async function readModuleManifest(
  moduleRoot: vscode.Uri
): Promise<ModuleManifest | undefined> {
  const manifestUri = vscode.Uri.joinPath(moduleRoot, "module.json");
  if (!(await pathExistsUri(manifestUri))) {
    return undefined;
  }
  try {
    const raw = await readTextFile(manifestUri);
    const parsed = JSON.parse(raw) as Record<string, unknown>;
    const id = String(parsed.id ?? "");
    const target = String(parsed.target ?? id);
    return {
      id,
      name: String(parsed.name ?? id),
      target,
      libraryRelative: String(parsed.library ?? defaultLibraryRelative(target)),
      entry: parsed.entry ? String(parsed.entry) : undefined,
      bundled: parsed.bundled === true,
    };
  } catch {
    return undefined;
  }
}

async function resolveEntryFileUri(
  moduleRoot: vscode.Uri,
  id: string,
  displayName: string,
  entryOverride?: string
): Promise<vscode.Uri | undefined> {
  const srcDir = vscode.Uri.joinPath(moduleRoot, "src");
  if (!(await pathExistsUri(srcDir))) {
    return undefined;
  }

  if (entryOverride) {
    const candidate = vscode.Uri.joinPath(srcDir, entryOverride);
    if (await pathExistsUri(candidate)) {
      return candidate;
    }
  }

  const defaults = [
    id === "gameplay" ? "GameplayModule.cpp" : undefined,
    `${toPascalCase(displayName || id)}Module.cpp`,
  ].filter((v): v is string => Boolean(v));

  for (const fileName of defaults) {
    const candidate = vscode.Uri.joinPath(srcDir, fileName);
    if (await pathExistsUri(candidate)) {
      return candidate;
    }
  }

  const entries = await vscode.workspace.findFiles(
    new vscode.RelativePattern(moduleRoot, "src/*Module.cpp"),
    undefined,
    8
  );
  if (entries.length === 1) {
    return entries[0];
  }
  for (const entry of entries) {
    const source = await readTextFile(entry);
    if (source.includes("FRI_MODULE")) {
      return entry;
    }
  }
  return entries[0];
}

async function moduleFromEntry(
  project: FriggaProject,
  entry: ProjectModuleEntry,
  manifest?: ModuleManifest
): Promise<FriggaModule | undefined> {
  const id = entry.id || entry.target;
  if (!id) {
    return undefined;
  }
  const root = vscode.Uri.joinPath(project.root, "modules", id);
  if (!(await pathExistsUri(root))) {
    return undefined;
  }
  const name = manifest?.name ?? id;
  const entryFile = await resolveEntryFileUri(root, id, name, manifest?.entry);
  if (!entryFile) {
    return undefined;
  }
  const cmakeFile = vscode.Uri.joinPath(root, "CMakeLists.txt");
  return {
    id,
    name,
    target: entry.target || id,
    enabled: entry.enabled,
    root,
    entryFile,
    cmakeFile,
    libraryRelative:
      entry.libraryRelative ||
      manifest?.libraryRelative ||
      defaultLibraryRelative(entry.target || id),
    bundled: manifest?.bundled ?? false,
  };
}

export async function listProjectModules(
  project: FriggaProject
): Promise<FriggaModule[]> {
  const desc = await loadProjectDescriptor(project.projectFile);
  if (!desc) {
    return [];
  }

  const modules: FriggaModule[] = [];
  for (const entry of desc.modules) {
    const manifest = await readModuleManifest(
      vscode.Uri.joinPath(project.root, "modules", entry.id || entry.target)
    );
    const mod = await moduleFromEntry(project, entry, manifest);
    if (mod) {
      modules.push(mod);
    }
  }
  return modules;
}

export async function listDiscoveredModules(
  searchRoot: vscode.Uri,
  bundled = false
): Promise<FriggaModule[]> {
  if (!(await pathExistsUri(searchRoot))) {
    return [];
  }
  const found = await vscode.workspace.findFiles(
    new vscode.RelativePattern(searchRoot, "*/module.json"),
    undefined,
    64
  );

  const modules: FriggaModule[] = [];
  for (const manifestUri of found) {
    const moduleRoot = vscode.Uri.file(path.dirname(manifestUri.fsPath));
    const manifest = await readModuleManifest(moduleRoot);
    if (!manifest?.id) {
      continue;
    }
    const entryFile = await resolveEntryFileUri(
      moduleRoot,
      manifest.id,
      manifest.name,
      manifest.entry
    );
    if (!entryFile) {
      continue;
    }
    modules.push({
      id: manifest.id,
      name: manifest.name,
      target: manifest.target || manifest.id,
      enabled: true,
      root: moduleRoot,
      entryFile,
      cmakeFile: vscode.Uri.joinPath(moduleRoot, "CMakeLists.txt"),
      libraryRelative: manifest.libraryRelative,
      bundled,
    });
  }
  return modules;
}

export async function listUserLibraryModules(): Promise<FriggaModule[]> {
  return listDiscoveredModules(vscode.Uri.file(defaultModulesLibraryDir()), false);
}

export async function listBundledModules(
  desc: FriggaProjectDescriptor
): Promise<FriggaModule[]> {
  const dirs = [
    desc.engine.friggaSdk ? path.join(desc.engine.friggaSdk, "modules") : "",
    desc.engine.friggaRoot
      ? path.join(desc.engine.friggaRoot, "src", "Editor", "Resources", "modules")
      : "",
  ].filter(Boolean);

  const seen = new Set<string>();
  const result: FriggaModule[] = [];
  for (const dir of dirs) {
    for (const mod of await listDiscoveredModules(vscode.Uri.file(dir), true)) {
      if (seen.has(mod.id)) {
        continue;
      }
      seen.add(mod.id);
      result.push(mod);
    }
  }
  return result;
}

export function inferModuleIdFromUri(project: FriggaProject, uri: vscode.Uri): string | undefined {
  const rel = path.relative(project.root.fsPath, uri.fsPath).replace(/\\/g, "/");
  const match = /^modules\/([^/]+)\//.exec(rel);
  return match?.[1];
}

export async function resolveModule(
  project: FriggaProject,
  contextUri?: vscode.Uri,
  preferredId?: string
): Promise<FriggaModule | undefined> {
  const modules = await listProjectModules(project);
  if (modules.length === 0) {
    vscode.window.showErrorMessage("No modules found under modules/ in this project.");
    return undefined;
  }

  if (preferredId) {
    const hit = modules.find((m) => m.id === preferredId);
    if (hit) {
      return hit;
    }
  }

  if (contextUri) {
    const inferred = inferModuleIdFromUri(project, contextUri);
    if (inferred) {
      const hit = modules.find((m) => m.id === inferred);
      if (hit) {
        return hit;
      }
    }
  }

  const enabled = modules.filter((m) => m.enabled);
  if (enabled.length === 1) {
    return enabled[0];
  }

  const pick = await vscode.window.showQuickPick(
    modules.map((m) => ({
      label: m.name,
      description: m.id,
      detail: m.enabled ? "enabled" : "disabled",
      module: m,
    })),
    { placeHolder: "Select gameplay module" }
  );
  return pick?.module;
}
