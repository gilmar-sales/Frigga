import * as vscode from "vscode";
import {
  FriggaProject,
  isValidCppIdentifier,
  openDocument,
  readTextFile,
  writeTextFile,
} from "./project";
import { insertFluentCall, pathExistsUri } from "./pluginModuleEdit";

function componentHeader(name: string, emptyTag: boolean): string {
  if (emptyTag) {
    return `#pragma once

#include <Freyr/Freyr.hpp>

struct ${name} : fr::Component
{
};
`;
  }

  return `#pragma once

#include <Freyr/Freyr.hpp>

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

  const moduleInclude = "#include <Frigga/Plugin/FriPluginModule.hpp>";
  const idx = source.indexOf(moduleInclude);
  if (idx >= 0) {
    return (
      source.slice(0, idx) + includeLine + "\n" + source.slice(idx)
    );
  }

  return includeLine + "\n" + source;
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
  try {
    plugin = ensureInclude(plugin, `#include "Components/${typeName}.hpp"`);
    plugin = insertFluentCall(plugin, `.Component<${typeName}>()`, "Component");
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    vscode.window.showErrorMessage(message);
    return;
  }
  await writeTextFile(pluginUri, plugin);

  await openDocument(headerUri);
  vscode.window.showInformationMessage(
    `Created ${typeName}. Registered in FRI_PLUGIN_MODULE — rebuild & reload the plugin.`
  );
}
