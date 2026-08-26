import * as vscode from "vscode";
import { FriggaProject, resolveFriggaProject } from "./project";
import { loadProjectDescriptor } from "./projectFile";
import {
  FriggaModule,
  listBundledModules,
  listProjectModules,
  listUserLibraryModules,
} from "./modules";
import {
  createModule,
  exportModule,
  installModule,
  setModuleEnabled,
} from "./moduleScaffold";
import { buildAllModules, buildModule } from "./moduleBuild";

type TreeNode =
  | { kind: "actions" }
  | { kind: "heading"; label: string }
  | { kind: "projectModule"; module: FriggaModule }
  | { kind: "libraryModule"; module: FriggaModule; installable: true };

export class ModuleTreeProvider implements vscode.TreeDataProvider<TreeNode> {
  private readonly onDidChange = new vscode.EventEmitter<TreeNode | undefined>();
  readonly onDidChangeTreeData = this.onDidChange.event;

  private project: FriggaProject | undefined;
  private projectModules: FriggaModule[] = [];
  private userModules: FriggaModule[] = [];
  private bundledModules: FriggaModule[] = [];

  constructor(private readonly context: vscode.ExtensionContext) {
    context.subscriptions.push(
      vscode.workspace.onDidSaveTextDocument((doc) => {
        if (doc.uri.fsPath.endsWith("frigga.project")) {
          void this.refresh();
        }
      })
    );
  }

  async refresh(): Promise<void> {
    this.project = await resolveFriggaProject();
    if (!this.project) {
      this.projectModules = [];
      this.userModules = [];
      this.bundledModules = [];
      this.onDidChange.fire(undefined);
      return;
    }

    this.projectModules = await listProjectModules(this.project);
    this.userModules = await listUserLibraryModules();
    const desc = await loadProjectDescriptor(this.project.projectFile);
    this.bundledModules = desc ? await listBundledModules(desc) : [];
    this.onDidChange.fire(undefined);
  }

  getTreeItem(element: TreeNode): vscode.TreeItem {
    if (element.kind === "actions") {
      const item = new vscode.TreeItem("Actions", vscode.TreeItemCollapsibleState.None);
      item.contextValue = "frigga.actions";
      return item;
    }
    if (element.kind === "heading") {
      const item = new vscode.TreeItem(element.label, vscode.TreeItemCollapsibleState.Expanded);
      item.contextValue = "frigga.heading";
      return item;
    }

    const mod = element.module;
    const item = new vscode.TreeItem(
      mod.name,
      vscode.TreeItemCollapsibleState.None
    );
    item.description = mod.id;
    item.tooltip = mod.root.fsPath;
    if (element.kind === "projectModule") {
      item.label = `${mod.enabled ? "☑" : "☐"} ${mod.name}`;
      item.contextValue = "frigga.projectModule";
    } else {
      item.contextValue = "frigga.libraryModule";
    }
    return item;
  }

  async getChildren(element?: TreeNode): Promise<TreeNode[]> {
    if (!this.project) {
      return [];
    }
    if (!element) {
      return [
        { kind: "actions" },
        { kind: "heading", label: "Project Modules" },
        ...this.projectModules.map((module) => ({ kind: "projectModule" as const, module })),
        { kind: "heading", label: "User Library" },
        ...this.userModules.map((module) => ({
          kind: "libraryModule" as const,
          module,
          installable: true as const,
        })),
        ...(this.bundledModules.length > 0
          ? [{ kind: "heading" as const, label: "Bundled" }]
          : []),
        ...this.bundledModules.map((module) => ({
          kind: "libraryModule" as const,
          module,
          installable: true as const,
        })),
      ];
    }
    return [];
  }

  getProject(): FriggaProject | undefined {
    return this.project;
  }

  moduleFromContext(element?: TreeNode): FriggaModule | undefined {
    if (!element) {
      return undefined;
    }
    if (element.kind === "projectModule" || element.kind === "libraryModule") {
      return element.module;
    }
    return undefined;
  }
}

export function registerModuleTreeView(
  context: vscode.ExtensionContext
): ModuleTreeProvider {
  const provider = new ModuleTreeProvider(context);
  context.subscriptions.push(
    vscode.window.registerTreeDataProvider("frigga.modules", provider),
    vscode.commands.registerCommand("frigga.refreshModules", () => provider.refresh()),
    vscode.commands.registerCommand("frigga.openModulesPanel", () => {
      void vscode.commands.executeCommand("frigga.modules.focus");
    }),
    vscode.commands.registerCommand("frigga.newModule", async () => {
      const project = await resolveFriggaProject();
      if (!project) {
        return;
      }
      await createModule(project);
      await provider.refresh();
    }),
    vscode.commands.registerCommand("frigga.buildAllModules", async () => {
      const project = provider.getProject() ?? (await resolveFriggaProject());
      if (!project) {
        return;
      }
      await buildAllModules(project);
    }),
    vscode.commands.registerCommand("frigga.buildModule", async (element?: TreeNode) => {
      const project = provider.getProject() ?? (await resolveFriggaProject());
      const mod = provider.moduleFromContext(element);
      if (!project || !mod) {
        return;
      }
      await buildModule(project, mod);
    }),
    vscode.commands.registerCommand("frigga.enableModule", async (element?: TreeNode) => {
      const project = provider.getProject() ?? (await resolveFriggaProject());
      const mod = provider.moduleFromContext(element);
      if (!project || !mod) {
        return;
      }
      await setModuleEnabled(project, mod, true);
      await provider.refresh();
    }),
    vscode.commands.registerCommand("frigga.disableModule", async (element?: TreeNode) => {
      const project = provider.getProject() ?? (await resolveFriggaProject());
      const mod = provider.moduleFromContext(element);
      if (!project || !mod) {
        return;
      }
      await setModuleEnabled(project, mod, false);
      await provider.refresh();
    }),
    vscode.commands.registerCommand("frigga.exportModule", async (element?: TreeNode) => {
      const project = provider.getProject() ?? (await resolveFriggaProject());
      const mod = provider.moduleFromContext(element);
      if (!project || !mod) {
        return;
      }
      await exportModule(project, mod);
    }),
    vscode.commands.registerCommand("frigga.installModule", async (element?: TreeNode) => {
      const project = provider.getProject() ?? (await resolveFriggaProject());
      const mod = provider.moduleFromContext(element);
      if (!project || !mod) {
        return;
      }
      await installModule(project, mod);
      await provider.refresh();
    })
  );
  void provider.refresh();
  return provider;
}
