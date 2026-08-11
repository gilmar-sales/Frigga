import * as vscode from "vscode";

/**
 * Locates FRI_PLUGIN_MODULE(...) { ... } and inserts a fluent call
 * before the terminating ';' of the builder chain.
 */
export function insertFluentCall(
  source: string,
  call: string,
  kind: "Component" | "System"
): string {
  if (source.includes(call)) {
    return source;
  }

  const moduleRe = /FRI_PLUGIN_MODULE\s*\(\s*\w+\s*\)\s*\{/;
  const moduleMatch = moduleRe.exec(source);
  if (!moduleMatch) {
    throw new Error(
      "Could not find FRI_PLUGIN_MODULE(...) in GameplayPlugin.cpp. Migrate the project in the Frigga Editor first."
    );
  }

  const bodyStart = moduleMatch.index + moduleMatch[0].length;
  let depth = 1;
  let i = bodyStart;
  while (i < source.length && depth > 0) {
    const ch = source[i];
    if (ch === "{") {
      depth += 1;
    } else if (ch === "}") {
      depth -= 1;
      if (depth === 0) {
        break;
      }
    }
    i += 1;
  }
  if (depth !== 0) {
    throw new Error("Unbalanced braces in FRI_PLUGIN_MODULE");
  }

  const body = source.slice(bodyStart, i);
  const chainEnd = body.lastIndexOf(";");
  if (chainEnd < 0) {
    throw new Error("Could not find fluent registration chain in FRI_PLUGIN_MODULE");
  }

  const beforeSemi = body.slice(0, chainEnd);
  const afterSemi = body.slice(chainEnd);

  // Prefer inserting after last .Component / before first .System for components.
  if (kind === "Component") {
    const lastComponent = beforeSemi.lastIndexOf(".Component<");
    if (lastComponent >= 0) {
      const close = findCallClose(beforeSemi, lastComponent);
      if (close >= 0) {
        const insertion = `\n          ${call}`;
        const newBody =
          beforeSemi.slice(0, close + 1) + insertion + beforeSemi.slice(close + 1) + afterSemi;
        return source.slice(0, bodyStart) + newBody + source.slice(i);
      }
    }
  }

  if (kind === "System") {
    const lastSystem = beforeSemi.lastIndexOf(".System<");
    if (lastSystem >= 0) {
      const close = findCallClose(beforeSemi, lastSystem);
      if (close >= 0) {
        const insertion = `\n          ${call}`;
        const newBody =
          beforeSemi.slice(0, close + 1) + insertion + beforeSemi.slice(close + 1) + afterSemi;
        return source.slice(0, bodyStart) + newBody + source.slice(i);
      }
    }
  }

  // Default: append before terminating ';'
  const trimmed = beforeSemi.replace(/\s+$/, "");
  const insertion = `\n          ${call}`;
  const newBody = trimmed + insertion + afterSemi;
  return source.slice(0, bodyStart) + newBody + source.slice(i);
}

function findCallClose(text: string, callStart: number): number {
  const openParen = text.indexOf("(", callStart);
  if (openParen < 0) {
    return -1;
  }
  let depth = 0;
  for (let i = openParen; i < text.length; i++) {
    const ch = text[i];
    if (ch === "(") {
      depth += 1;
    } else if (ch === ")") {
      depth -= 1;
      if (depth === 0) {
        return i;
      }
    }
  }
  return -1;
}

export async function pathExistsUri(uri: vscode.Uri): Promise<boolean> {
  try {
    await vscode.workspace.fs.stat(uri);
    return true;
  } catch {
    return false;
  }
}
