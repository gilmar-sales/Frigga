import * as vscode from "vscode";
import {
  FriggaProject,
  isValidCppIdentifier,
  openDocument,
  readTextFile,
  writeTextFile,
} from "./project";

function componentHeader(name: string, emptyTag: boolean): string {
  if (emptyTag) {
    return `#pragma once

#include <Freyr/Freyr.hpp>

/// Tag component. Register in GameplayPlugin on_attach.
struct ${name} : fr::Component
{
};
`;
  }

  return `#pragma once

#include <Freyr/Freyr.hpp>

/// Project gameplay component. Register in GameplayPlugin on_attach.
struct ${name} : fr::Component
{
    float value = 0.0f;
};
`;
}

function ensureInclude(source: string, includeLine: string): string {
  if (source.includes(includeLine)) {
    return source;
  }

  const componentInclude = /#include\s+"Components\/[^"]+"\s*\n/g;
  let lastMatch: RegExpExecArray | null = null;
  let match: RegExpExecArray | null;
  while ((match = componentInclude.exec(source)) !== null) {
    lastMatch = match;
  }
  if (lastMatch) {
    const insertAt = lastMatch.index + lastMatch[0].length;
    return source.slice(0, insertAt) + includeLine + "\n" + source.slice(insertAt);
  }

  const userComponents = '#include <frigga_user_components.hpp>';
  const idx = source.indexOf(userComponents);
  if (idx >= 0) {
    const insertAt = idx + userComponents.length;
    return (
      source.slice(0, insertAt) +
      "\n\n" +
      includeLine +
      source.slice(insertAt)
    );
  }

  return includeLine + "\n" + source;
}

function ensureRegistration(source: string, typeName: string): string {
  const call = `FriRegisterUserComponent<${typeName}>(*registry, *userComponents, "${typeName}");`;
  if (source.includes(`FriRegisterUserComponent<${typeName}>`)) {
    return source;
  }

  const registerRe = /FriRegisterUserComponent<[^>]+>\(\*registry,\s*\*userComponents,\s*"[^"]*"\);/g;
  let lastMatch: RegExpExecArray | null = null;
  let match: RegExpExecArray | null;
  while ((match = registerRe.exec(source)) !== null) {
    lastMatch = match;
  }
  if (lastMatch) {
    const insertAt = lastMatch.index + lastMatch[0].length;
    return source.slice(0, insertAt) + "\n        " + call + source.slice(insertAt);
  }

  const systemCreate = /plugin->system\s*=\s*std::make_unique<GameplaySystem>\(registry\);/;
  const createMatch = systemCreate.exec(source);
  if (createMatch) {
    return (
      source.slice(0, createMatch.index) +
      call +
      "\n        " +
      source.slice(createMatch.index)
    );
  }

  throw new Error(
    "Could not find FriRegisterUserComponent / GameplaySystem creation in GameplayPlugin.cpp"
  );
}

export async function createGameplayComponent(
  project: FriggaProject
): Promise<void> {
  const name = await vscode.window.showInputBox({
    prompt: "Component type name (C++ identifier)",
    placeHolder: "Player",
    validateInput: (value) => {
      if (!value.trim()) {
        return "Name is required";
      }
      if (!isValidCppIdentifier(value.trim())) {
        return "Must be a valid C++ identifier";
      }
      return undefined;
    },
  });
  if (!name) {
    return;
  }
  const typeName = name.trim();

  const kind = await vscode.window.showQuickPick(
    [
      { label: "With fields", description: "struct with a sample float value", empty: false },
      { label: "Empty tag", description: "marker component with no fields", empty: true },
    ],
    { placeHolder: "Component kind" }
  );
  if (!kind) {
    return;
  }

  const headerUri = vscode.Uri.joinPath(
    project.root,
    "src",
    "Components",
    `${typeName}.hpp`
  );
  try {
    await vscode.workspace.fs.stat(headerUri);
    vscode.window.showErrorMessage(`${typeName}.hpp already exists`);
    return;
  } catch {
    // ok
  }

  const pluginUri = vscode.Uri.joinPath(project.root, "src", "GameplayPlugin.cpp");
  if (!(await pathExistsUri(pluginUri))) {
    vscode.window.showErrorMessage("src/GameplayPlugin.cpp not found in project");
    return;
  }

  await writeTextFile(headerUri, componentHeader(typeName, kind.empty));

  let plugin = await readTextFile(pluginUri);
  plugin = ensureInclude(plugin, `#include "Components/${typeName}.hpp"`);
  plugin = ensureRegistration(plugin, typeName);
  await writeTextFile(pluginUri, plugin);

  await openDocument(headerUri);
  vscode.window.showInformationMessage(
    `Created ${typeName}. Registered in GameplayPlugin.cpp — rebuild & reload the plugin.`
  );
}

async function pathExistsUri(uri: vscode.Uri): Promise<boolean> {
  try {
    await vscode.workspace.fs.stat(uri);
    return true;
  } catch {
    return false;
  }
}
