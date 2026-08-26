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

  const componentInclude = /#include\s+"components\/[^"]+"\s*\n/g;
  let lastMatch: RegExpExecArray | null = null;
  let match: RegExpExecArray | null;
  while ((match = componentInclude.exec(source)) !== null) {
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

  return includeLine + "\n" + source;
}

export async function createComponent(
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
    prompt: `Component type name for module "${mod.name}"`,
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
    mod.root,
    "src",
    "components",
    `${typeName}.hpp`
  );
  if (await pathExistsUri(headerUri)) {
    vscode.window.showErrorMessage(`${typeName}.hpp already exists`);
    return;
  }
  if (!(await pathExistsUri(mod.entryFile))) {
    vscode.window.showErrorMessage(`Module entry file not found: ${mod.entryFile.fsPath}`);
    return;
  }

  await writeTextFile(headerUri, componentHeader(typeName, kind.empty));

  let moduleSource = await readTextFile(mod.entryFile);
  try {
    moduleSource = ensureInclude(moduleSource, `#include "components/${typeName}.hpp"`);
    moduleSource = insertFluentCall(
      moduleSource,
      `.Component<${typeName}>()`,
      "Component",
      entryFileName(mod.entryFile)
    );
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    vscode.window.showErrorMessage(message);
    return;
  }
  await writeTextFile(mod.entryFile, moduleSource);

  await openDocument(headerUri);
  vscode.window.showInformationMessage(
    `Created ${typeName} in ${mod.name}. Registered in FRI_MODULE — rebuild & reload the module.`
  );
}
