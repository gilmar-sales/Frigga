import * as vscode from "vscode";
import { readTextFile, writeTextFile } from "./project";

export type ModuleSourceKind = "project" | "user";

export interface ProjectModuleEntry {
  id: string;
  target: string;
  libraryRelative: string;
  enabled: boolean;
  source: ModuleSourceKind;
}

export interface ProjectEnginePaths {
  friggaSdk: string;
  friggaRoot: string;
  friggaBuild: string;
}

export interface FriggaProjectDescriptor {
  version: number;
  name: string;
  template: string;
  scene: string;
  moduleTarget: string;
  moduleLibraryRelative: string;
  modules: ProjectModuleEntry[];
  engine: ProjectEnginePaths;
}

function escapeJson(value: string): string {
  return value.replace(/\\/g, "\\\\").replace(/"/g, '\\"');
}

function defaultLibraryRelative(target: string): string {
  switch (process.platform) {
    case "win32":
      return `build/${target}.dll`;
    case "darwin":
      return `build/lib${target}.dylib`;
    default:
      return `build/lib${target}.so`;
  }
}

function ensureGameplayModule(desc: FriggaProjectDescriptor): void {
  const gameplay = desc.modules.find((m) => m.id === "gameplay" || m.target === "gameplay");
  if (gameplay) {
    desc.moduleTarget = gameplay.target || "gameplay";
    desc.moduleLibraryRelative = gameplay.libraryRelative;
    return;
  }
  if (!desc.moduleTarget) {
    desc.moduleTarget = "gameplay";
  }
  if (!desc.moduleLibraryRelative) {
    desc.moduleLibraryRelative = defaultLibraryRelative(desc.moduleTarget);
  }
  desc.modules.unshift({
    id: "gameplay",
    target: desc.moduleTarget,
    libraryRelative: desc.moduleLibraryRelative,
    enabled: true,
    source: "project",
  });
}

export async function loadProjectDescriptor(
  projectFile: vscode.Uri
): Promise<FriggaProjectDescriptor | undefined> {
  try {
    const raw = await readTextFile(projectFile);
    const parsed = JSON.parse(raw) as Record<string, unknown>;

    const modulesRaw = Array.isArray(parsed.modules) ? parsed.modules : [];
    const modules: ProjectModuleEntry[] = modulesRaw.map((item) => {
      const entry = item as Record<string, unknown>;
      const source = entry.source === "user" ? "user" : "project";
      return {
        id: String(entry.id ?? entry.target ?? ""),
        target: String(entry.target ?? entry.id ?? ""),
        libraryRelative: String(entry.library ?? ""),
        enabled: entry.enabled !== false,
        source,
      };
    });

    const moduleBlock =
      parsed.module && typeof parsed.module === "object"
        ? (parsed.module as Record<string, unknown>)
        : {};

    const engine =
      parsed.engine && typeof parsed.engine === "object"
        ? (parsed.engine as Record<string, unknown>)
        : {};

    const desc: FriggaProjectDescriptor = {
      version: typeof parsed.version === "number" ? parsed.version : 1,
      name: String(parsed.name ?? "Project"),
      template: String(parsed.template ?? "3d"),
      scene: String(parsed.scene ?? "Scenes/main.json"),
      moduleTarget: String(moduleBlock.target ?? "gameplay"),
      moduleLibraryRelative: String(moduleBlock.library ?? ""),
      modules,
      engine: {
        friggaSdk: String(engine.friggaSdk ?? ""),
        friggaRoot: String(engine.friggaRoot ?? ""),
        friggaBuild: String(engine.friggaBuild ?? ""),
      },
    };

    ensureGameplayModule(desc);
    return desc;
  } catch {
    return undefined;
  }
}

export async function saveProjectDescriptor(
  projectFile: vscode.Uri,
  desc: FriggaProjectDescriptor
): Promise<void> {
  ensureGameplayModule(desc);

  const lines: string[] = [
    "{",
    `  "version": ${desc.version},`,
    `  "name": "${escapeJson(desc.name)}",`,
    `  "template": "${escapeJson(desc.template)}",`,
    `  "scene": "${escapeJson(desc.scene)}",`,
    `  "module": {`,
    `    "target": "${escapeJson(desc.moduleTarget)}",`,
    `    "library": "${escapeJson(desc.moduleLibraryRelative)}"`,
    `  },`,
    `  "modules": [`,
  ];

  for (let i = 0; i < desc.modules.length; i++) {
    const entry = desc.modules[i];
    lines.push("    {");
    lines.push(`      "id": "${escapeJson(entry.id)}",`);
    lines.push(`      "target": "${escapeJson(entry.target)}",`);
    lines.push(`      "library": "${escapeJson(entry.libraryRelative)}",`);
    lines.push(`      "enabled": ${entry.enabled ? "true" : "false"},`);
    lines.push(`      "source": "${entry.source}"`);
    lines.push(`    }${i + 1 < desc.modules.length ? "," : ""}`);
  }

  lines.push("  ],");
  lines.push(`  "engine": {`);
  lines.push(`    "friggaSdk": "${escapeJson(desc.engine.friggaSdk)}",`);
  lines.push(`    "friggaRoot": "${escapeJson(desc.engine.friggaRoot)}",`);
  lines.push(`    "friggaBuild": "${escapeJson(desc.engine.friggaBuild)}"`);
  lines.push("  }");
  lines.push("}");

  await writeTextFile(projectFile, lines.join("\n") + "\n");
}
