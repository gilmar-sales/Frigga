import * as vscode from "vscode";
import { FriggaProject } from "./project";
import { FriggaModule } from "./modules";
import { loadProjectDescriptor } from "./projectFile";

function shellQuote(value: string): string {
  if (process.platform === "win32") {
    return `"${value.replace(/"/g, '\\"')}"`;
  }
  return `"${value.replace(/"/g, '\\"')}"`;
}

function makeConfigureCommand(project: FriggaProject, desc: Awaited<ReturnType<typeof loadProjectDescriptor>>): string {
  const buildDir = vscode.Uri.joinPath(project.root, "build").fsPath;
  const parts = [
    "cmake",
    "-S",
    shellQuote(project.root.fsPath),
    "-B",
    shellQuote(buildDir),
    "-G",
    "Ninja",
    "-DCMAKE_BUILD_TYPE=Debug",
    "-DCMAKE_CXX_STANDARD=26",
    "-DCMAKE_CXX_STANDARD_REQUIRED=ON",
    "-DCMAKE_CXX_EXTENSIONS=ON",
  ];
  if (desc?.engine.friggaSdk) {
    parts.push("-DFRIGGA_SDK", shellQuote(desc.engine.friggaSdk));
  }
  if (desc?.engine.friggaBuild) {
    parts.push("-DFRIGGA_BUILD", shellQuote(desc.engine.friggaBuild));
  }
  return parts.join(" ");
}

export async function buildModule(
  project: FriggaProject,
  mod?: FriggaModule
): Promise<void> {
  const desc = await loadProjectDescriptor(project.projectFile);
  const target = mod?.target;
  const label = target ? `Frigga: Build ${target}` : "Frigga: Build All Modules";
  const buildArgs = target
    ? ["--build", "build", "--target", target]
    : ["--build", "build"];

  const terminal = vscode.window.createTerminal({
    name: label,
    cwd: project.root.fsPath,
  });
  terminal.show();
  terminal.sendText(makeConfigureCommand(project, desc));
  terminal.sendText(`cmake ${buildArgs.join(" ")}`);
  vscode.window.showInformationMessage(
    "Configure + build started. Press Ctrl+R in the Frigga Editor to reload modules when it finishes."
  );
}

export async function buildAllModules(project: FriggaProject): Promise<void> {
  await buildModule(project);
}
