import * as fs from "fs";
import * as path from "path";
import * as vscode from "vscode";
import { FriggaProject } from "./project";
import { FriggaModule } from "./modules";
import { loadProjectDescriptor, FriggaProjectDescriptor } from "./projectFile";

function shellQuote(value: string): string {
  if (process.platform === "win32") {
    return `"${value.replace(/"/g, '\\"')}"`;
  }
  return `"${value.replace(/"/g, '\\"')}"`;
}

function looksLikeFriggaSdk(sdkRoot: string): boolean {
  return (
    !!sdkRoot &&
    fs.existsSync(path.join(sdkRoot, "cmake", "FriggaSdk.cmake")) &&
    fs.existsSync(path.join(sdkRoot, "include", "Frigga", "Module", "frigga_module.h"))
  );
}

function resolveEnginePath(
  fromProject: string | undefined,
  envName: string
): string {
  if (fromProject && fs.existsSync(fromProject)) {
    if (envName === "FRIGGA_SDK") {
      return looksLikeFriggaSdk(fromProject) ? fromProject : "";
    }
    return fromProject;
  }
  const fromEnv = process.env[envName];
  if (fromEnv && fs.existsSync(fromEnv)) {
    if (envName === "FRIGGA_SDK") {
      return looksLikeFriggaSdk(fromEnv) ? fromEnv : "";
    }
    return fromEnv;
  }
  return "";
}

function makeConfigureCommand(
  project: FriggaProject,
  desc: FriggaProjectDescriptor | undefined
): string {
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
  const sdk = resolveEnginePath(desc?.engine.friggaSdk, "FRIGGA_SDK");
  const build = resolveEnginePath(desc?.engine.friggaBuild, "FRIGGA_BUILD");
  if (sdk) {
    parts.push(`-DFRIGGA_SDK=${shellQuote(sdk)}`);
  }
  if (build) {
    parts.push(`-DFRIGGA_BUILD=${shellQuote(build)}`);
  }
  return parts.join(" ");
}

export async function buildModule(
  project: FriggaProject,
  mod?: FriggaModule
): Promise<void> {
  const desc = await loadProjectDescriptor(project.projectFile);
  const sdk = resolveEnginePath(desc?.engine.friggaSdk, "FRIGGA_SDK");
  if (!sdk) {
    vscode.window.showErrorMessage(
      "Frigga SDK not found. Set FRIGGA_SDK to your Sdk/ (or engine tree), or build modules from the Frigga Editor (Ctrl+B)."
    );
    return;
  }

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
