import * as path from "path";
import * as vscode from "vscode";
import { FriggaProject, readTextFile, writeTextFile } from "./project";
import {
  FriggaProjectDescriptor,
  loadProjectDescriptor,
  saveProjectDescriptor,
} from "./projectFile";
import {
  FriggaModule,
  defaultLibraryRelative,
  defaultModulesLibraryDir,
  sanitizeModuleId,
  toPascalCase,
} from "./modules";
import { pathExistsUri } from "./moduleModuleEdit";

const MANAGED_BEGIN = "# FRIGGA_MANAGED_MODULE_SUBDIRS_BEGIN";
const MANAGED_END = "# FRIGGA_MANAGED_MODULE_SUBDIRS_END";

function makeExtraModuleCpp(): string {
  return `#include <Frigga/Module/FriModule.hpp>

FRI_MODULE(module)
{
}
`;
}

function makeExtraModuleCMake(target: string, sourceFile: string): string {
  return `frigga_add_module(${target}\n  src/${sourceFile}\n)\n`;
}

function makeModuleJson(id: string, name: string, target: string): string {
  const library = defaultLibraryRelative(target);
  return `{
  "id": "${id}",
  "name": "${name.replace(/"/g, '\\"')}",
  "target": "${target}",
  "library": "${library}"
}
`;
}

function makeManagedModuleSubdirsBlock(desc: FriggaProjectDescriptor): string {
  const lines = [MANAGED_BEGIN];
  for (const entry of desc.modules) {
    const folder = entry.id || entry.target;
    lines.push(
      `if(EXISTS "\${CMAKE_CURRENT_SOURCE_DIR}/Modules/${folder}/CMakeLists.txt")`,
      `  add_subdirectory(Modules/${folder})`,
      "endif()"
    );
  }
  lines.push(MANAGED_END);
  return lines.join("\n") + "\n";
}

async function syncManagedModuleSubdirs(
  project: FriggaProject,
  desc: FriggaProjectDescriptor
): Promise<void> {
  const cmakeUri = vscode.Uri.joinPath(project.root, "CMakeLists.txt");
  if (!(await pathExistsUri(cmakeUri))) {
    throw new Error("CMakeLists.txt not found at project root");
  }
  let text = await readTextFile(cmakeUri);
  const block = makeManagedModuleSubdirsBlock(desc);
  const begin = text.indexOf(MANAGED_BEGIN);
  const end = text.indexOf(MANAGED_END);
  if (begin >= 0 && end > begin) {
    const endLine = text.indexOf("\n", end);
    const replaceUntil = endLine >= 0 ? endLine + 1 : text.length;
    text = text.slice(0, begin) + block + text.slice(replaceUntil);
  } else {
    if (text.length > 0 && !text.endsWith("\n")) {
      text += "\n";
    }
    text += `\n${block}`;
  }
  await writeTextFile(cmakeUri, text);
}

function registerModuleEntry(
  desc: FriggaProjectDescriptor,
  entry: FriggaProjectDescriptor["modules"][number]
): void {
  const idx = desc.modules.findIndex(
    (m) => m.id === entry.id || m.target === entry.target
  );
  if (idx >= 0) {
    desc.modules[idx] = entry;
  } else {
    desc.modules.push(entry);
  }
}

export async function createModule(project: FriggaProject): Promise<void> {
  const name = await vscode.window.showInputBox({
    prompt: "Module name (e.g. Character Movement)",
    placeHolder: "Character Movement",
    validateInput: (value) => (value.trim() ? undefined : "Name is required"),
  });
  if (!name) {
    return;
  }

  const id = sanitizeModuleId(name);
  const moduleRoot = vscode.Uri.joinPath(project.root, "Modules", id);
  if (await pathExistsUri(moduleRoot)) {
    vscode.window.showErrorMessage(`Module already exists: ${id}`);
    return;
  }

  const sourceFile = `${toPascalCase(name)}Module.cpp`;
  const target = id;

  await writeTextFile(
    vscode.Uri.joinPath(moduleRoot, "src", sourceFile),
    makeExtraModuleCpp()
  );
  await writeTextFile(
    vscode.Uri.joinPath(moduleRoot, "CMakeLists.txt"),
    makeExtraModuleCMake(target, sourceFile)
  );
  await writeTextFile(
    vscode.Uri.joinPath(moduleRoot, "module.json"),
    makeModuleJson(id, name.trim(), target)
  );
  await vscode.workspace.fs.createDirectory(vscode.Uri.joinPath(moduleRoot, "src", "components"));
  await vscode.workspace.fs.createDirectory(vscode.Uri.joinPath(moduleRoot, "src", "systems"));

  const desc = (await loadProjectDescriptor(project.projectFile))!;
  registerModuleEntry(desc, {
    id,
    target,
    libraryRelative: defaultLibraryRelative(target),
    enabled: true,
    source: "project",
  });
  await syncManagedModuleSubdirs(project, desc);
  await saveProjectDescriptor(project.projectFile, desc);

  const entryUri = vscode.Uri.joinPath(moduleRoot, "src", sourceFile);
  await vscode.workspace.openTextDocument(entryUri).then((doc) => vscode.window.showTextDocument(doc));
  vscode.window.showInformationMessage(
    `Created module "${name}" (${id}). Build modules and reload in the Editor (Ctrl+R).`
  );
}

async function copyDirectory(from: vscode.Uri, to: vscode.Uri): Promise<void> {
  await vscode.workspace.fs.createDirectory(to);
  const entries = await vscode.workspace.fs.readDirectory(from);
  for (const [name, type] of entries) {
    const src = vscode.Uri.joinPath(from, name);
    const dst = vscode.Uri.joinPath(to, name);
    if (type === vscode.FileType.Directory) {
      await copyDirectory(src, dst);
    } else {
      const bytes = await vscode.workspace.fs.readFile(src);
      await vscode.workspace.fs.writeFile(dst, bytes);
    }
  }
}

export async function installModule(
  project: FriggaProject,
  source: FriggaModule
): Promise<void> {
  const desc = (await loadProjectDescriptor(project.projectFile))!;
  const destRoot = vscode.Uri.joinPath(project.root, "Modules", source.id);
  if (await pathExistsUri(destRoot)) {
    const overwrite = await vscode.window.showWarningMessage(
      `Module "${source.id}" already exists in project. Overwrite?`,
      "Overwrite"
    );
    if (overwrite !== "Overwrite") {
      return;
    }
    await vscode.workspace.fs.delete(destRoot, { recursive: true });
  }

  await copyDirectory(source.root, destRoot);

  if (!(await pathExistsUri(vscode.Uri.joinPath(destRoot, "CMakeLists.txt")))) {
    const sourceFile =
      source.entryFile.path.split("/").pop()?.split("\\").pop() ??
      `${toPascalCase(source.name)}Module.cpp`;
    await writeTextFile(
      vscode.Uri.joinPath(destRoot, "CMakeLists.txt"),
      makeExtraModuleCMake(source.target, sourceFile)
    );
  }

  registerModuleEntry(desc, {
    id: source.id,
    target: source.target,
    libraryRelative: defaultLibraryRelative(source.target),
    enabled: true,
    source: "user",
  });
  await writeTextFile(
    vscode.Uri.joinPath(destRoot, "module.json"),
    makeModuleJson(source.id, source.name, source.target)
  );
  await syncManagedModuleSubdirs(project, desc);
  await saveProjectDescriptor(project.projectFile, desc);

  vscode.window.showInformationMessage(
    `Installed module "${source.name}". Build modules (Frigga: Build All Modules), then Reload in the Editor (Ctrl+R).`
  );
}

export async function exportModule(
  project: FriggaProject,
  mod: FriggaModule
): Promise<void> {
  if (mod.id === "gameplay" || mod.target === "gameplay") {
    vscode.window.showErrorMessage("The gameplay module cannot be exported.");
    return;
  }

  const destRoot = vscode.Uri.file(path.join(defaultModulesLibraryDir(), mod.id));
  if (await pathExistsUri(destRoot)) {
    await vscode.workspace.fs.delete(destRoot, { recursive: true });
  }
  await copyDirectory(mod.root, destRoot);
  vscode.window.showInformationMessage(`Exported module to ${destRoot.fsPath}`);
}

export async function setModuleEnabled(
  project: FriggaProject,
  mod: FriggaModule,
  enabled: boolean
): Promise<void> {
  const desc = (await loadProjectDescriptor(project.projectFile))!;
  const entry = desc.modules.find((m) => m.id === mod.id);
  if (!entry) {
    return;
  }
  entry.enabled = enabled;
  await saveProjectDescriptor(project.projectFile, desc);
  vscode.window.showInformationMessage(
    `Module "${mod.name}" ${enabled ? "enabled" : "disabled"}. Reload in the Editor (Ctrl+R).`
  );
}
