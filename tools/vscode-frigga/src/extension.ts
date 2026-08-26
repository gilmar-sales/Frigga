import * as vscode from "vscode";
import { attachDebuggerToEditor } from "./attachDebugger";
import { createComponent } from "./newComponent";
import { createSystem } from "./newSystem";
import { refreshProjectContext, resolveFriggaProject } from "./project";
import { registerModuleTreeView } from "./moduleTreeView";
import { FriggaModule } from "./modules";

export async function activate(context: vscode.ExtensionContext): Promise<void> {
  await refreshProjectContext();

  const tree = registerModuleTreeView(context);

  const watcher = vscode.workspace.createFileSystemWatcher("**/frigga.project");
  watcher.onDidCreate(() => {
    void refreshProjectContext();
    void tree.refresh();
  });
  watcher.onDidDelete(() => {
    void refreshProjectContext();
    void tree.refresh();
  });

  context.subscriptions.push(
    watcher,
    vscode.commands.registerCommand("frigga.newComponent", async (uri?: vscode.Uri, element?: unknown) => {
      const project = await resolveFriggaProject(uri);
      if (!project) {
        return;
      }
      const mod = moduleFromTreeElement(element);
      await createComponent(project, uri, mod);
    }),
    vscode.commands.registerCommand("frigga.newSystem", async (uri?: vscode.Uri, element?: unknown) => {
      const project = await resolveFriggaProject(uri);
      if (!project) {
        return;
      }
      const mod = moduleFromTreeElement(element);
      await createSystem(project, uri, mod);
    }),
    vscode.commands.registerCommand("frigga.attachDebugger", async (uri?: vscode.Uri) => {
      await attachDebuggerToEditor(uri);
    }),
    vscode.workspace.onDidChangeWorkspaceFolders(() => {
      void refreshProjectContext();
      void tree.refresh();
    })
  );
}

function moduleFromTreeElement(element: unknown): FriggaModule | undefined {
  if (!element || typeof element !== "object") {
    return undefined;
  }
  const node = element as { kind?: string; module?: FriggaModule };
  if (node.kind === "projectModule" && node.module) {
    return node.module;
  }
  return undefined;
}

export function deactivate(): void {}
