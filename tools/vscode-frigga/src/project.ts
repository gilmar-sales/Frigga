import * as vscode from "vscode";
import * as path from "path";
import { promises as fs } from "fs";

export interface FriggaProject {
  /** Directory containing frigga.project */
  root: vscode.Uri;
  projectFile: vscode.Uri;
}

async function pathExists(uri: vscode.Uri): Promise<boolean> {
  try {
    await vscode.workspace.fs.stat(uri);
    return true;
  } catch {
    return false;
  }
}

async function findProjectFileUpwards(startDir: string): Promise<string | undefined> {
  let dir = path.resolve(startDir);
  for (;;) {
    const candidate = path.join(dir, "frigga.project");
    try {
      await fs.access(candidate);
      return candidate;
    } catch {
      // continue
    }
    const parent = path.dirname(dir);
    if (parent === dir) {
      return undefined;
    }
    dir = parent;
  }
}

async function collectWorkspaceProjects(): Promise<FriggaProject[]> {
  const found = await vscode.workspace.findFiles("**/frigga.project", "**/build/**", 32);
  const projects: FriggaProject[] = [];
  const seen = new Set<string>();
  for (const projectFile of found) {
    const root = vscode.Uri.file(path.dirname(projectFile.fsPath));
    if (seen.has(root.fsPath)) {
      continue;
    }
    seen.add(root.fsPath);
    projects.push({ root, projectFile });
  }
  return projects;
}

/**
 * Resolve the Frigga project to target.
 * Prefer the project nearest the active editor / explorer selection, else prompt.
 */
export async function resolveFriggaProject(
  contextUri?: vscode.Uri
): Promise<FriggaProject | undefined> {
  const workspaceProjects = await collectWorkspaceProjects();

  const hints: string[] = [];
  if (contextUri) {
    hints.push(contextUri.fsPath);
  }
  const active = vscode.window.activeTextEditor?.document.uri;
  if (active && active.scheme === "file") {
    hints.push(active.fsPath);
  }

  for (const hint of hints) {
    const start = (await pathExists(vscode.Uri.file(hint))) &&
      (await vscode.workspace.fs.stat(vscode.Uri.file(hint))).type === vscode.FileType.Directory
      ? hint
      : path.dirname(hint);
    const projectPath = await findProjectFileUpwards(start);
    if (projectPath) {
      return {
        root: vscode.Uri.file(path.dirname(projectPath)),
        projectFile: vscode.Uri.file(projectPath),
      };
    }
  }

  if (workspaceProjects.length === 1) {
    return workspaceProjects[0];
  }
  if (workspaceProjects.length > 1) {
    const pick = await vscode.window.showQuickPick(
      workspaceProjects.map((p) => ({
        label: path.basename(p.root.fsPath),
        description: p.root.fsPath,
        project: p,
      })),
      { placeHolder: "Select Frigga project" }
    );
    return pick?.project;
  }

  vscode.window.showErrorMessage(
    "No frigga.project found. Open a Frigga project folder first."
  );
  return undefined;
}

export async function refreshProjectContext(): Promise<boolean> {
  const projects = await collectWorkspaceProjects();
  const open = projects.length > 0;
  await vscode.commands.executeCommand("setContext", "frigga.projectOpen", open);
  return open;
}

export function isValidCppIdentifier(name: string): boolean {
  return /^[A-Za-z_][A-Za-z0-9_]*$/.test(name);
}

export async function writeTextFile(uri: vscode.Uri, contents: string): Promise<void> {
  await vscode.workspace.fs.createDirectory(
    vscode.Uri.file(path.dirname(uri.fsPath))
  );
  await vscode.workspace.fs.writeFile(uri, Buffer.from(contents, "utf8"));
}

export async function readTextFile(uri: vscode.Uri): Promise<string> {
  const bytes = await vscode.workspace.fs.readFile(uri);
  return Buffer.from(bytes).toString("utf8");
}

export async function openDocument(uri: vscode.Uri): Promise<void> {
  const doc = await vscode.workspace.openTextDocument(uri);
  await vscode.window.showTextDocument(doc);
}
