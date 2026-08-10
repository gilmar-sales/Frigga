import * as vscode from "vscode";
import * as fs from "fs";
import * as path from "path";
import { FriggaProject, resolveFriggaProject } from "./project";

interface EditorSessionMarker {
  pid: number;
  editorPath: string;
  soSearchPath: string;
  pluginLibrary?: string;
  projectRoot: string;
  updatedAt?: string;
}

function isProcessAlive(pid: number): boolean {
  if (!Number.isFinite(pid) || pid <= 0) {
    return false;
  }
  try {
    process.kill(pid, 0);
    return true;
  } catch (error) {
    const err = error as NodeJS.ErrnoException;
    // EPERM means the process exists but we cannot signal it.
    return err.code === "EPERM";
  }
}

async function readEditorSession(
  project: FriggaProject
): Promise<EditorSessionMarker | undefined> {
  const markerPath = path.join(project.root.fsPath, ".frigga", "editor-session.json");
  try {
    const raw = await fs.promises.readFile(markerPath, "utf8");
    const parsed = JSON.parse(raw) as EditorSessionMarker;
    if (
      typeof parsed.pid !== "number" ||
      typeof parsed.editorPath !== "string" ||
      typeof parsed.soSearchPath !== "string" ||
      typeof parsed.projectRoot !== "string"
    ) {
      vscode.window.showErrorMessage(
        "Invalid .frigga/editor-session.json. Open the project in the Frigga Editor and try again."
      );
      return undefined;
    }
    return parsed;
  } catch {
    vscode.window.showErrorMessage(
      "No Frigga Editor session found. Open this project in the Editor (it writes .frigga/editor-session.json), then attach."
    );
    return undefined;
  }
}

function ensureCppTools(): boolean {
  const ext = vscode.extensions.getExtension("ms-vscode.cpptools");
  if (!ext) {
    void vscode.window
      .showErrorMessage(
        "C/C++ extension (ms-vscode.cpptools) is required to attach GDB to the Frigga Editor.",
        "Install"
      )
      .then((choice) => {
        if (choice === "Install") {
          void vscode.commands.executeCommand(
            "workbench.extensions.installExtension",
            "ms-vscode.cpptools"
          );
        }
      });
    return false;
  }
  return true;
}

export async function attachDebuggerToEditor(uri?: vscode.Uri): Promise<void> {
  if (!ensureCppTools()) {
    return;
  }

  const project = await resolveFriggaProject(uri);
  if (!project) {
    return;
  }

  const session = await readEditorSession(project);
  if (!session) {
    return;
  }

  if (!isProcessAlive(session.pid)) {
    vscode.window.showErrorMessage(
      `Frigga Editor process ${session.pid} is not running. Open the project in the Editor and try again.`
    );
    return;
  }

  if (!fs.existsSync(session.editorPath)) {
    vscode.window.showErrorMessage(
      `Editor binary not found at ${session.editorPath}. Rebuild Frigga and reopen the project.`
    );
    return;
  }

  const platform = process.platform;
  const config: vscode.DebugConfiguration =
    platform === "win32"
      ? {
          name: "Attach Frigga Editor",
          type: "cppvsdbg",
          request: "attach",
          processId: session.pid,
        }
      : {
          name: "Attach Frigga Editor",
          type: "cppdbg",
          request: "attach",
          program: session.editorPath,
          processId: session.pid,
          MIMode: platform === "darwin" ? "lldb" : "gdb",
          additionalSOLibSearchPath: session.soSearchPath,
          setupCommands: [
            {
              description: "Enable pretty-printing for gdb",
              text: "-enable-pretty-printing",
              ignoreFailures: true,
            },
          ],
        };

  const folder =
    vscode.workspace.getWorkspaceFolder(project.root) ??
    vscode.workspace.workspaceFolders?.[0];

  const started = await vscode.debug.startDebugging(folder, config);
  if (!started) {
    vscode.window.showErrorMessage("Failed to start debugger attach to Frigga Editor.");
    return;
  }

  void vscode.window.showInformationMessage(
    `Attached to Frigga Editor (pid ${session.pid}). Set breakpoints in gameplay code and press Play in the Editor.`
  );
}
