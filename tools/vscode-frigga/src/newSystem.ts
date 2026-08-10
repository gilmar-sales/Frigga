import * as vscode from "vscode";
import {
  FriggaProject,
  isValidCppIdentifier,
  openDocument,
  readTextFile,
  writeTextFile,
} from "./project";

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

/**
 * Gameplay system living in the plugin .so.
 * Called from GameplaySystem::Update while play mode is running.
 */
class ${className}
{
  public:
    static void Update(fr::Registry *registry, float deltaTime);
};
`;
}

function systemSource(className: string, fileBase: string): string {
  return `#include "Systems/${fileBase}.hpp"

void ${className}::Update(fr::Registry *registry, float deltaTime)
{
    if(!registry)
    {
        return;
    }

    (void)deltaTime;
    // Example:
    // registry->CreateMutation()->Each<YourComponent>(
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

  const firstInclude = source.indexOf('#include "');
  if (firstInclude >= 0) {
    const lineEnd = source.indexOf("\n", firstInclude);
    const insertAt = lineEnd >= 0 ? lineEnd + 1 : firstInclude;
    return source.slice(0, insertAt) + includeLine + "\n" + source.slice(insertAt);
  }

  return includeLine + "\n" + source;
}

function ensureSystemCall(source: string, className: string): string {
  const call = `${className}::Update(mRegistry, deltaTime);`;
  if (source.includes(`${className}::Update(`)) {
    return source;
  }

  const updateFn =
    /void\s+GameplaySystem::Update\s*\(\s*float\s+deltaTime\s*\)\s*\{/;
  const match = updateFn.exec(source);
  if (!match) {
    throw new Error("Could not find GameplaySystem::Update in GameplaySystem.cpp");
  }

  // Insert before the closing brace of Update: find matching braces from match end.
  let i = match.index + match[0].length;
  let depth = 1;
  while (i < source.length && depth > 0) {
    const ch = source[i];
    if (ch === "{") {
      depth += 1;
    } else if (ch === "}") {
      depth -= 1;
      if (depth === 0) {
        break;
      }
    }
    i += 1;
  }
  if (depth !== 0) {
    throw new Error("Unbalanced braces in GameplaySystem::Update");
  }

  return source.slice(0, i) + "\n    " + call + "\n" + source.slice(i);
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

async function pathExistsUri(uri: vscode.Uri): Promise<boolean> {
  try {
    await vscode.workspace.fs.stat(uri);
    return true;
  } catch {
    return false;
  }
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

  if (await pathExistsUri(headerUri) || await pathExistsUri(sourceUri)) {
    vscode.window.showErrorMessage(`${fileBase} already exists under src/Systems`);
    return;
  }

  const gameplaySystemUri = vscode.Uri.joinPath(
    project.root,
    "src",
    "GameplaySystem.cpp"
  );
  const cmakeUri = vscode.Uri.joinPath(project.root, "CMakeLists.txt");
  if (!(await pathExistsUri(gameplaySystemUri))) {
    vscode.window.showErrorMessage("src/GameplaySystem.cpp not found");
    return;
  }
  if (!(await pathExistsUri(cmakeUri))) {
    vscode.window.showErrorMessage("CMakeLists.txt not found");
    return;
  }

  await writeTextFile(headerUri, systemHeader(className));
  await writeTextFile(sourceUri, systemSource(className, fileBase));

  let gameplaySystem = await readTextFile(gameplaySystemUri);
  gameplaySystem = ensureInclude(
    gameplaySystem,
    `#include "Systems/${fileBase}.hpp"`
  );
  gameplaySystem = ensureSystemCall(gameplaySystem, className);
  await writeTextFile(gameplaySystemUri, gameplaySystem);

  let cmake = await readTextFile(cmakeUri);
  cmake = ensureCmakeSource(cmake, `src/Systems/${fileBase}.cpp`);
  await writeTextFile(cmakeUri, cmake);

  await openDocument(sourceUri);
  vscode.window.showInformationMessage(
    `Created ${className}. Wired into GameplaySystem + CMakeLists — rebuild & reload the plugin.`
  );
}
