import * as vscode from "vscode";
import {
  FriggaProject,
  isValidCppIdentifier,
  openDocument,
  readTextFile,
  writeTextFile,
} from "./project";
import { FriggaModule, resolveModule } from "./modules";
import { entryFileName, insertFluentCall, pathExistsUri } from "./moduleModuleEdit";

function systemBaseName(raw: string): string {
  const trimmed = raw.trim();
  if (trimmed.endsWith("System")) {
    return trimmed.slice(0, -"System".length) || trimmed;
  }
  return trimmed;
}

function systemClassName(base: string): string {
  return base.endsWith("System") ? base : `${base}System`;
}

function systemHeader(className: string): string {
  return `#pragma once

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

/**
 * Freyr system registered on the host via FRI_MODULE (.System<${className}>()).
 * Runs on the Simulation pipeline (Play mode).
 */
class ${className}: public fr::System
{
  public:
    explicit ${className}(const skr::Arc<fr::Registry> &registry);

    void Update(float deltaTime) override;
};
`;
}

function systemSource(className: string, fileBase: string): string {
  return `#include "systems/${fileBase}.hpp"

${className}::${className}(const skr::Arc<fr::Registry> &registry) : fr::System(registry) {}

void ${className}::Update(float deltaTime)
{
    (void)deltaTime;
}
`;
}

function ensureInclude(source: string, includeLine: string): string {
  if (source.includes(includeLine)) {
    return source;
  }

  const systemInclude = /#include\s+"systems\/[^"]+"\s*\n/g;
  let lastMatch: RegExpExecArray | null = null;
  let match: RegExpExecArray | null;
  while ((match = systemInclude.exec(source)) !== null) {
    lastMatch = match;
  }
  if (lastMatch) {
    const insertAt = lastMatch.index + lastMatch[0].length;
    return source.slice(0, insertAt) + includeLine + "\n" + source.slice(insertAt);
  }

  const moduleInclude = "#include <Frigga/Module/FriModule.hpp>";
  const idx = source.indexOf(moduleInclude);
  if (idx >= 0) {
    return source.slice(0, idx) + includeLine + "\n" + source.slice(idx);
  }

  const firstInclude = source.indexOf('#include "');
  if (firstInclude >= 0) {
    const lineEnd = source.indexOf("\n", firstInclude);
    const insertAt = lineEnd >= 0 ? lineEnd + 1 : firstInclude;
    return source.slice(0, insertAt) + includeLine + "\n" + source.slice(insertAt);
  }

  return includeLine + "\n" + source;
}

function ensureCmakeSource(cmake: string, relativeCpp: string): string {
  const normalized = relativeCpp.replace(/\\/g, "/");
  if (cmake.includes(normalized)) {
    return cmake;
  }

  const libraryBlock =
    /(?:add_library\s*\(\s*\w+\s+SHARED|frigga_add_module\s*\(\s*\w+)\s*\n([\s\S]*?)\)/;
  const match = libraryBlock.exec(cmake);
  if (!match) {
    throw new Error("Could not find frigga_add_module(...) in CMakeLists.txt");
  }

  const bodyStart = match.index + match[0].indexOf(match[1]);
  const bodyEnd = bodyStart + match[1].length;
  const insertion = `  ${normalized}\n`;
  return cmake.slice(0, bodyEnd) + insertion + cmake.slice(bodyEnd);
}

export async function createSystem(
  project: FriggaProject,
  contextUri?: vscode.Uri,
  preferredModule?: FriggaModule
): Promise<void> {
  const mod =
    preferredModule ?? (await resolveModule(project, contextUri));
  if (!mod) {
    return;
  }

  const name = await vscode.window.showInputBox({
    prompt: `System name for module "${mod.name}" (e.g. Movement or MovementSystem)`,
    placeHolder: "Movement",
    validateInput: (value) => {
      if (!value.trim()) {
        return "Name is required";
      }
      const base = systemBaseName(value);
      if (!isValidCppIdentifier(base) && !isValidCppIdentifier(value.trim())) {
        return "Must be a valid C++ identifier";
      }
      return undefined;
    },
  });
  if (!name) {
    return;
  }

  const className = systemClassName(name.trim());
  const fileBase = className;

  const headerUri = vscode.Uri.joinPath(mod.root, "src", "systems", `${fileBase}.hpp`);
  const sourceUri = vscode.Uri.joinPath(mod.root, "src", "systems", `${fileBase}.cpp`);

  if ((await pathExistsUri(headerUri)) || (await pathExistsUri(sourceUri))) {
    vscode.window.showErrorMessage(`${fileBase} already exists under ${mod.id}/src/systems`);
    return;
  }
  if (!(await pathExistsUri(mod.entryFile))) {
    vscode.window.showErrorMessage(`Module entry file not found: ${mod.entryFile.fsPath}`);
    return;
  }
  if (!(await pathExistsUri(mod.cmakeFile))) {
    vscode.window.showErrorMessage(`CMakeLists.txt not found for module ${mod.id}`);
    return;
  }

  await writeTextFile(headerUri, systemHeader(className));
  await writeTextFile(sourceUri, systemSource(className, fileBase));

  let moduleSource = await readTextFile(mod.entryFile);
  try {
    moduleSource = ensureInclude(moduleSource, `#include "systems/${fileBase}.hpp"`);
    moduleSource = insertFluentCall(
      moduleSource,
      `.System<${className}>()`,
      "System",
      entryFileName(mod.entryFile)
    );
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    vscode.window.showErrorMessage(message);
    return;
  }
  await writeTextFile(mod.entryFile, moduleSource);

  let cmake = await readTextFile(mod.cmakeFile);
  try {
    cmake = ensureCmakeSource(cmake, `src/systems/${fileBase}.cpp`);
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    vscode.window.showErrorMessage(message);
    return;
  }
  await writeTextFile(mod.cmakeFile, cmake);

  await openDocument(sourceUri);
  vscode.window.showInformationMessage(
    `Created ${className} in ${mod.name}. Registered in FRI_MODULE + CMakeLists — rebuild & reload the module.`
  );
}
