/**
 * cdb-runner.ts — Wraps Microsoft's cdb.exe (command-line WinDbg) for
 * programmatic crash dump analysis.
 *
 * Design decisions:
 *  - Spawns cdb.exe as a child process with piped stdin/stdout.
 *  - Each command sends a marker sentinel so we can detect end-of-output.
 *  - Symbol path is configured via .sympath before any analysis.
 *  - All operations are read-only (no live attach, no modification).
 */

import { spawn, type ChildProcess } from "node:child_process";
import { resolve } from "node:path";

const CDB_PATH = process.env.CDB_PATH ?? "cdb.exe";
const SENTINEL = "__MCP_END_SENTINEL__";

export class CdbRunner {
  private process: ChildProcess | null = null;
  private outputBuffer = "";
  private resolveWaiting: ((output: string) => void) | null = null;
  private symbolPath: string | null = null;

  /**
   * Open a minidump file for analysis.
   */
  async open(dumpPath: string, pdbPath?: string): Promise<void> {
    const absPath = resolve(dumpPath);
    this.symbolPath = pdbPath ? resolve(pdbPath) : null;

    this.process = spawn(CDB_PATH, ["-z", absPath, "-logo", "nul"], {
      stdio: ["pipe", "pipe", "pipe"],
      env: {
        ...process.env,
        _NT_SYMBOL_PATH: this.symbolPath
          ? `${this.symbolPath};srv*`
          : "srv*",
      },
    });

    this.process.stdout?.setEncoding("utf-8");
    this.process.stdout?.on("data", (chunk: string) => {
      this.outputBuffer += chunk;
      if (this.outputBuffer.includes(SENTINEL)) {
        const output = this.outputBuffer.split(SENTINEL)[0];
        this.outputBuffer = "";
        this.resolveWaiting?.(output.trim());
        this.resolveWaiting = null;
      }
    });

    this.process.stderr?.setEncoding("utf-8");
    this.process.stderr?.on("data", (chunk: string) => {
      // Silently collect warnings — symbol load noise, etc.
      if (process.env.DEBUG_CDB) {
        process.stderr.write(`[cdb stderr] ${chunk}`);
      }
    });

    // Wait for initial prompt
    await this.waitForPrompt();

    // Configure symbols
    if (this.symbolPath) {
      await this.execute(`.sympath+ ${this.symbolPath}`);
      await this.execute(".reload /f");
    }
  }

  /**
   * Execute a single cdb command and return the output.
   */
  async execute(command: string): Promise<string> {
    if (!this.process?.stdin) {
      throw new Error("CDB process not running. Call open() first.");
    }

    return new Promise((resolve) => {
      this.resolveWaiting = resolve;
      this.process!.stdin!.write(`${command}\n.echo ${SENTINEL}\n`);
    });
  }

  /**
   * Close the cdb session.
   */
  async close(): Promise<void> {
    if (this.process) {
      this.process.stdin?.write("q\n");
      this.process.kill();
      this.process = null;
    }
  }

  /**
   * Wait for the initial cdb prompt after opening the dump.
   */
  private waitForPrompt(): Promise<string> {
    return new Promise((resolve) => {
      // Give cdb time to load the dump and produce initial output.
      const timeout = setTimeout(() => {
        const output = this.outputBuffer;
        this.outputBuffer = "";
        resolve(output);
      }, 3000);

      this.process?.stdout?.once("data", () => {
        clearTimeout(timeout);
        // Wait a bit more for full initial output
        setTimeout(() => {
          const output = this.outputBuffer;
          this.outputBuffer = "";
          resolve(output);
        }, 1000);
      });
    });
  }
}

/**
 * Parse a cdb stack trace output (from `k` or `kP` command) into structured frames.
 */
export function parseCallStack(raw: string): import("./types.js").StackFrame[] {
  const frames: import("./types.js").StackFrame[] = [];
  const lines = raw.split("\n").filter((l) => l.trim().length > 0);

  // Skip header lines (Child-SP, RetAddr, etc.)
  const dataLines = lines.filter(
    (l) => !l.startsWith("Child-SP") && !l.startsWith(" #") && !l.startsWith("---")
  );

  let frameNum = 0;
  for (const line of dataLines) {
    // Format: RetAddr : Module!Function+Offset [SourceFile @ Line]
    const match = line.match(
      /([0-9a-f`]+)\s+([0-9a-f`]+)\s+(\w+)!(.+?)(?:\+0x([0-9a-f]+))?(?:\s+\[(.+?)\s*@\s*(\d+)\])?/i
    );
    if (match) {
      frames.push({
        frameNumber: frameNum++,
        address: match[1],
        module: match[3],
        function: match[4],
        offset: match[5] ? `0x${match[5]}` : "0x0",
        sourceFile: match[6] ?? null,
        lineNumber: match[7] ? parseInt(match[7], 10) : null,
      });
    }
  }

  return frames;
}

/**
 * Parse `!analyze -v` output into a structured analysis result.
 */
export function parseAnalyzeOutput(raw: string): Partial<import("./types.js").CrashAnalysis> {
  const result: Partial<import("./types.js").CrashAnalysis> = {
    additionalContext: [],
  };

  // Extract exception code
  const codeMatch = raw.match(/EXCEPTION_CODE:\s+\(NTSTATUS\)\s+(0x[0-9a-f]+)/i);
  if (codeMatch) {
    result.exception = {
      code: codeMatch[1],
      codeName: extractBetween(raw, "EXCEPTION_CODE_STR:", "\n") ?? "UNKNOWN",
      address: extractBetween(raw, "FAULTING_IP:", "\n") ?? "0x0",
      threadId: parseInt(extractBetween(raw, "FAULTING_THREAD:", "\n") ?? "0", 16),
      firstChance: raw.includes("first chance"),
    };
  }

  // Extract faulting module/function
  const faultingIp = extractBetween(raw, "FAULTING_IP:", "\n");
  if (faultingIp) {
    const ipMatch = faultingIp.match(/(\w+)!(.+?)(?:\+0x([0-9a-f]+))?$/i);
    if (ipMatch) {
      result.faultingFrame = {
        frameNumber: 0,
        module: ipMatch[1],
        function: ipMatch[2],
        offset: ipMatch[3] ? `0x${ipMatch[3]}` : "0x0",
        address: "0x0",
        sourceFile: null,
        lineNumber: null,
      };
    }
  }

  // Extract root cause hypothesis from FOLLOWUP_NAME or BUCKET_ID
  const bucket = extractBetween(raw, "BUCKET_ID:", "\n");
  if (bucket) {
    result.rootCauseHypothesis = bucket.trim();
  }

  return result;
}

function extractBetween(text: string, start: string, end: string): string | null {
  const startIdx = text.indexOf(start);
  if (startIdx === -1) return null;
  const afterStart = startIdx + start.length;
  const endIdx = text.indexOf(end, afterStart);
  if (endIdx === -1) return text.slice(afterStart).trim();
  return text.slice(afterStart, endIdx).trim();
}
