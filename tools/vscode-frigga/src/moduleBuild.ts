import * as vscode from "vscode";
import { FriggaProject } from "./project";
import { FriggaModule } from "./modules";

export async function buildModule(
  project: FriggaProject,
  mod?: FriggaModule
): Promise<void> {
  const buildDir = vscode.Uri.joinPath(project.root, "build");
  const target = mod?.target;
  const label = target ? `Frigga: Build ${target}` : "Frigga: Build All Modules";
  const args = target
    ? ["--build", "build", "--target", target]
    : ["--build", "build"];

  const terminal = vscode.window.createTerminal({
    name: label,
    cwd: project.root.fsPath,
  });
  terminal.show();
  terminal.sendText(`cmake ${args.join(" ")}`);
  vscode.window.showInformationMessage(
    "Build started. Press Ctrl+R in the Frigga Editor to reload modules when it finishes."
  );
}

export async function buildAllModules(project: FriggaProject): Promise<void> {
  await buildModule(project);
}
