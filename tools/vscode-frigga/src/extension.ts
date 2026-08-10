import * as vscode from "vscode";
import { createGameplayComponent } from "./newComponent";
import { createGameplaySystem } from "./newSystem";
import { refreshProjectContext, resolveFriggaProject } from "./project";

export async function activate(context: vscode.ExtensionContext): Promise<void> {
  await refreshProjectContext();

  const watcher = vscode.workspace.createFileSystemWatcher("**/frigga.project");
  watcher.onDidCreate(() => {
    void refreshProjectContext();
  });
  watcher.onDidDelete(() => {
    void refreshProjectContext();
  });

  context.subscriptions.push(
    watcher,
    vscode.commands.registerCommand("frigga.newComponent", async (uri?: vscode.Uri) => {
      const project = await resolveFriggaProject(uri);
      if (!project) {
        return;
      }
      await createGameplayComponent(project);
    }),
    vscode.commands.registerCommand("frigga.newSystem", async (uri?: vscode.Uri) => {
      const project = await resolveFriggaProject(uri);
      if (!project) {
        return;
      }
      await createGameplaySystem(project);
    }),
    vscode.workspace.onDidChangeWorkspaceFolders(() => {
      void refreshProjectContext();
    })
  );
}

export function deactivate(): void {}
