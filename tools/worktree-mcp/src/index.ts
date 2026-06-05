#!/usr/bin/env node
/**
 * worktree-mcp — MCP server for git worktree-based parallel fix execution.
 *
 * Manages isolated git worktrees for each resolution branch, runs cmake builds,
 * executes ctest, and reports results. Enables tree-of-thought parallel fix
 * attempts without conflicts.
 */

import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import { resolve, join } from "node:path";
import { mkdir, readFile, rm, stat } from "node:fs/promises";

const exec = promisify(execFile);

const server = new McpServer({
  name: "worktree-mcp",
  version: "1.0.0",
});

interface WorktreeInfo {
  path: string;
  branch: string;
  commit: string;
  linked: boolean;
}

interface BuildResult {
  success: boolean;
  worktreePath: string;
  preset: string;
  errors: string[];
  warnings: string[];
  elapsed: number;
}

interface TestResult {
  success: boolean;
  worktreePath: string;
  total: number;
  passed: number;
  failed: number;
  failedTests: string[];
  elapsed: number;
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: create_worktree
// ─────────────────────────────────────────────────────────────────────────────
server.tool(
  "create_worktree",
  "Create a new git worktree for an isolated fix branch. Each resolution attempt gets its own worktree so fixes can be developed and tested in parallel.",
  {
    repoPath: z.string().describe("Path to the main git repository"),
    branchName: z.string().describe("Branch name for this fix attempt (e.g. 'fix/BUG-001-bounds-check')"),
    baseBranch: z.string().optional().default("main").describe("Base branch to fork from"),
  },
  async ({ repoPath, branchName, baseBranch }): Promise<{ content: Array<{ type: "text"; text: string }> }> => {
    const absRepo = resolve(repoPath);
    const worktreePath = join(absRepo, ".worktrees", branchName.replace(/\//g, "-"));

    await mkdir(join(absRepo, ".worktrees"), { recursive: true });

    // Create the worktree with a new branch
    const { stdout, stderr } = await exec(
      "git",
      ["worktree", "add", "-b", branchName, worktreePath, baseBranch],
      { cwd: absRepo }
    );

    return {
      content: [{
        type: "text",
        text: JSON.stringify({
          success: true,
          worktreePath,
          branchName,
          baseBranch,
          message: stdout.trim() || stderr.trim(),
        }, null, 2),
      }],
    };
  }
);

// ─────────────────────────────────────────────────────────────────────────────
// Tool: list_worktrees
// ─────────────────────────────────────────────────────────────────────────────
server.tool(
  "list_worktrees",
  "List all active git worktrees for the repository. Shows which fix branches are currently active.",
  {
    repoPath: z.string().describe("Path to the main git repository"),
  },
  async ({ repoPath }): Promise<{ content: Array<{ type: "text"; text: string }> }> => {
    const absRepo = resolve(repoPath);
    const { stdout } = await exec("git", ["worktree", "list", "--porcelain"], { cwd: absRepo });

    const worktrees: WorktreeInfo[] = [];
    let current: Partial<WorktreeInfo> = {};

    for (const line of stdout.split("\n")) {
      if (line.startsWith("worktree ")) {
        if (current.path) worktrees.push(current as WorktreeInfo);
        current = { path: line.slice(9), linked: true };
      } else if (line.startsWith("HEAD ")) {
        current.commit = line.slice(5);
      } else if (line.startsWith("branch ")) {
        current.branch = line.slice(7).replace("refs/heads/", "");
      }
    }
    if (current.path) worktrees.push(current as WorktreeInfo);

    return {
      content: [{ type: "text", text: JSON.stringify(worktrees, null, 2) }],
    };
  }
);

// ─────────────────────────────────────────────────────────────────────────────
// Tool: remove_worktree
// ─────────────────────────────────────────────────────────────────────────────
server.tool(
  "remove_worktree",
  "Remove a git worktree after a fix is merged or abandoned. Cleans up the branch and working directory.",
  {
    repoPath: z.string().describe("Path to the main git repository"),
    worktreePath: z.string().describe("Path to the worktree to remove"),
    force: z.boolean().optional().default(false).describe("Force removal even with uncommitted changes"),
  },
  async ({ repoPath, worktreePath, force }): Promise<{ content: Array<{ type: "text"; text: string }> }> => {
    const absRepo = resolve(repoPath);
    const args = ["worktree", "remove", worktreePath];
    if (force) args.push("--force");

    const { stdout, stderr } = await exec("git", args, { cwd: absRepo });

    return {
      content: [{
        type: "text",
        text: JSON.stringify({
          success: true,
          removed: worktreePath,
          message: stdout.trim() || stderr.trim() || "Worktree removed",
        }, null, 2),
      }],
    };
  }
);

// ─────────────────────────────────────────────────────────────────────────────
// Tool: cmake_build
// ─────────────────────────────────────────────────────────────────────────────
server.tool(
  "cmake_build",
  "Run a CMake configure + build in a worktree. Uses the project's CMakePresets.json. Returns build status, errors, and warnings.",
  {
    worktreePath: z.string().describe("Path to the worktree to build"),
    preset: z.string().optional().default("dev-msvc").describe("CMake preset name from CMakePresets.json"),
  },
  async ({ worktreePath, preset }): Promise<{ content: Array<{ type: "text"; text: string }> }> => {
    const absPath = resolve(worktreePath);
    const start = Date.now();
    const errors: string[] = [];
    const warnings: string[] = [];

    try {
      // Configure
      await exec("cmake", ["--preset", preset], { cwd: absPath, timeout: 60000 });

      // Build
      const { stdout, stderr } = await exec(
        "cmake",
        ["--build", "--preset", preset],
        { cwd: absPath, timeout: 300000 }
      );

      // Parse warnings and errors from output
      const combined = stdout + stderr;
      for (const line of combined.split("\n")) {
        if (/error\s*(C|LNK)\d+/i.test(line)) errors.push(line.trim());
        else if (/warning\s*C\d+/i.test(line)) warnings.push(line.trim());
      }

      const result: BuildResult = {
        success: errors.length === 0,
        worktreePath: absPath,
        preset,
        errors,
        warnings,
        elapsed: Date.now() - start,
      };

      return {
        content: [{ type: "text", text: JSON.stringify(result, null, 2) }],
      };
    } catch (err: unknown) {
      const msg = err instanceof Error ? err.message : String(err);
      errors.push(msg);

      const result: BuildResult = {
        success: false,
        worktreePath: absPath,
        preset,
        errors,
        warnings,
        elapsed: Date.now() - start,
      };

      return {
        content: [{ type: "text", text: JSON.stringify(result, null, 2) }],
      };
    }
  }
);

// ─────────────────────────────────────────────────────────────────────────────
// Tool: run_tests
// ─────────────────────────────────────────────────────────────────────────────
server.tool(
  "run_tests",
  "Run CTest in a worktree build directory. Returns pass/fail counts and names of failed tests. Use after cmake_build to validate a fix.",
  {
    worktreePath: z.string().describe("Path to the worktree"),
    preset: z.string().optional().default("dev-msvc").describe("CTest preset name"),
    filter: z.string().optional().describe("Test name filter regex"),
  },
  async ({ worktreePath, preset, filter }): Promise<{ content: Array<{ type: "text"; text: string }> }> => {
    const absPath = resolve(worktreePath);
    const start = Date.now();
    const args = ["--test-dir", join(absPath, "build", preset), "--output-on-failure"];
    if (filter) args.push("-R", filter);

    try {
      const { stdout, stderr } = await exec("ctest", args, {
        cwd: absPath,
        timeout: 120000,
      });

      const combined = stdout + stderr;
      const totalMatch = combined.match(/(\d+) tests? from/);
      const passedMatch = combined.match(/(\d+) tests? passed/);
      const failedMatch = combined.match(/(\d+) tests? failed/);

      const failedTests: string[] = [];
      const failLines = combined.matchAll(/^\d+\/\d+ Test #\d+: (.+?) \.+\*\*\*Failed/gm);
      for (const m of failLines) failedTests.push(m[1]);

      const result: TestResult = {
        success: failedTests.length === 0,
        worktreePath: absPath,
        total: parseInt(totalMatch?.[1] ?? "0", 10),
        passed: parseInt(passedMatch?.[1] ?? "0", 10),
        failed: parseInt(failedMatch?.[1] ?? "0", 10),
        failedTests,
        elapsed: Date.now() - start,
      };

      return {
        content: [{ type: "text", text: JSON.stringify(result, null, 2) }],
      };
    } catch (err: unknown) {
      const msg = err instanceof Error ? err.message : String(err);
      const result: TestResult = {
        success: false,
        worktreePath: absPath,
        total: 0,
        passed: 0,
        failed: 1,
        failedTests: [msg],
        elapsed: Date.now() - start,
      };

      return {
        content: [{ type: "text", text: JSON.stringify(result, null, 2) }],
      };
    }
  }
);

// ─────────────────────────────────────────────────────────────────────────────
// Tool: worktree_diff
// ─────────────────────────────────────────────────────────────────────────────
server.tool(
  "worktree_diff",
  "Get the git diff of changes in a worktree relative to its base branch. Use to review a fix before the HITL gate.",
  {
    worktreePath: z.string().describe("Path to the worktree"),
    baseBranch: z.string().optional().default("main").describe("Base branch to diff against"),
  },
  async ({ worktreePath, baseBranch }): Promise<{ content: Array<{ type: "text"; text: string }> }> => {
    const absPath = resolve(worktreePath);

    const { stdout } = await exec(
      "git",
      ["diff", `${baseBranch}...HEAD`, "--stat"],
      { cwd: absPath }
    );

    const { stdout: fullDiff } = await exec(
      "git",
      ["diff", `${baseBranch}...HEAD`],
      { cwd: absPath }
    );

    return {
      content: [{
        type: "text",
        text: JSON.stringify({
          summary: stdout.trim(),
          diff: fullDiff.trim(),
        }, null, 2),
      }],
    };
  }
);

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────

async function main() {
  const transport = new StdioServerTransport();
  await server.connect(transport);
}

main().catch((err) => {
  console.error("Fatal:", err);
  process.exit(1);
});
