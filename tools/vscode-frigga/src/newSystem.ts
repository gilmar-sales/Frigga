import * as vscode from "vscode";
import {
  FriggaProject,
  isValidCppIdentifier,
  openDocument,
  readTextFile,
  writeTextFile,
} from "./project";
import { insertFluentCall, pathExistsUri } from "./pluginModuleEdit";

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
 * Freyr system registered on the host via FRI_PLUGIN_MODULE (.System<${className}>()).
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
  return `#include "Systems/${fileBase}.hpp"

${className}::${className}(const skr::Arc<fr::Registry> &registry) : fr::System(registry) {}

void ${className}::Update(float deltaTime)
{
    (void)deltaTime;
    // Example:
    // mRegistry->CreateMutation()->Each<YourComponent>(
    //     [](fr::Entity, YourComponent &comp) { (void)comp; });
}
`;
}

function ensureInclude(source: string, includeLine: string): string {
  if (source.includes(includeLine)) {
    return source;
  }

  const systemInclude = /#include\s+"Systems\/[^"]+"\s*\n/g;
  let lastMatch: RegExpExecArray | null = null;
  let match: RegExpExecArray | null;
  while ((match = systemInclude.exec(source)) !== null) {
    lastMatch = match;
  }
  if (lastMatch) {
    const insertAt = lastMatch.index + lastMatch[0].length;
    return source.slice(0, insertAt) + includeLine + "\n" + source.slice(insertAt);
  }

  const moduleInclude = "#include <Frigga/Plugin/FriPluginModule.hpp>";
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
    /add_library\s*\(\s*\w+\s+SHARED\s*\n([\s\S]*?)\)/;
  const match = libraryBlock.exec(cmake);
  if (!match) {
    throw new Error("Could not find add_library(... SHARED ...) in CMakeLists.txt");
  }

  const bodyStart = match.index + match[0].indexOf(match[1]);
  const bodyEnd = bodyStart + match[1].length;
  const insertion = `  ${normalized}\n`;
  return cmake.slice(0, bodyEnd) + insertion + cmake.slice(bodyEnd);
}

export async function createGameplaySystem(project: FriggaProject): Promise<void> {
  const name = await vscode.window.showInputBox({
    prompt: 'System name (e.g. "Movement" or "MovementSystem")',
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

  const headerUri = vscode.Uri.joinPath(
    project.root,
    "src",
    "Systems",
    `${fileBase}.hpp`
  );
  const sourceUri = vscode.Uri.joinPath(
    project.root,
    "src",
    "Systems",
    `${fileBase}.cpp`
  );

  if ((await pathExistsUri(headerUri)) || (await pathExistsUri(sourceUri))) {
    vscode.window.showErrorMessage(`${fileBase} already exists under src/Systems`);
    return;
  }

  const pluginUri = vscode.Uri.joinPath(project.root, "src", "GameplayPlugin.cpp");
  const cmakeUri = vscode.Uri.joinPath(project.root, "CMakeLists.txt");
  if (!(await pathExistsUri(pluginUri))) {
    vscode.window.showErrorMessage("src/GameplayPlugin.cpp not found");
    return;
  }
  if (!(await pathExistsUri(cmakeUri))) {
    vscode.window.showErrorMessage("CMakeLists.txt not found");
    return;
  }

  await writeTextFile(headerUri, systemHeader(className));
  await writeTextFile(sourceUri, systemSource(className, fileBase));

  let plugin = await readTextFile(pluginUri);
  try {
    plugin = ensureInclude(plugin, `#include "Systems/${fileBase}.hpp"`);
    plugin = insertFluentCall(plugin, `.System<${className}>()`, "System");
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    vscode.window.showErrorMessage(message);
    return;
  }
  await writeTextFile(pluginUri, plugin);

  let cmake = await readTextFile(cmakeUri);
  try {
    cmake = ensureCmakeSource(cmake, `src/Systems/${fileBase}.cpp`);
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    vscode.window.showErrorMessage(message);
    return;
  }
  await writeTextFile(cmakeUri, cmake);

  await openDocument(sourceUri);
  vscode.window.showInformationMessage(
    `Created ${className}. Registered in FRI_PLUGIN_MODULE + CMakeLists — rebuild & reload the plugin.`
  );
}
