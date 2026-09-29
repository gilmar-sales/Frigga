#pragma once

namespace publish
{
    /// True when the command line asks for a headless publish (`--publish <project>`).
    [[nodiscard]] bool IsPublishCommand(int argc, char *argv[]);

    /// `Editor --publish <frigga.project|dir> --out <dir> [--profile development|shipping]
    /// [--clean]`. Runs the pipeline without creating a window; returns the process exit code.
    int RunPublishCli(int argc, char *argv[]);
} // namespace publish
