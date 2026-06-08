#!/usr/bin/env node
/**
 * crash-dump-mcp — MCP server for Windows crash dump analysis.
 *
 * Exposes tools for parsing .dmp files, resolving symbols from .pdb,
 * extracting call stacks, and performing automated crash analysis.
 *
 * Requires: cdb.exe (Windows Debugging Tools) on PATH or CDB_PATH env var.
 */

import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";
import { CdbRunner, parseCallStack, parseAnalyzeOutput } from "./utils/cdb-runner.js";
import type {
  ParsedMinidump,
  StackFrame,
  CrashAnalysis,
  ModuleInfo,
  ThreadContext,
  MemoryRegion,
  SymbolResolution,
} from "./utils/types.js";

const server = new McpServer({
  name: "crash-dump-mcp",
  version: "1.0.0",
});

// Shared CDB session (one dump at a time)
let cdb: CdbRunner | null = null;

type ToolResult = { content: Array<{ type: "text"; text: string }>; isError?: boolean };

/**
 * Wrap a tool handler so any thrown error (e.g. a cdb command timeout) is
 * returned as a structured MCP error result instead of rejecting and leaving
 * the calling agent waiting indefinitely.
 */
function withErrors<A>(
  fn: (args: A) => Promise<ToolResult>
): (args: A) => Promise<ToolResult> {
  return async (args: A) => {
    try {
      return await fn(args);
    } catch (err) {
      const message = err instanceof Error ? err.message : String(err);
      return {
        isError: true,
        content: [{ type: "text", text: JSON.stringify({ error: message }, null, 2) }],
      };
    }
  };
}

// ─────────────────────────────────────────────────────────────────────────────
// Tool: parse_minidump
// ─────────────────────────────────────────────────────────────────────────────
server.tool(
  "parse_minidump",
  "Open and parse a Windows minidump (.dmp) file. Returns exception code, faulting thread, module count, and process info. Must be called before other tools.",
  {
    dumpPath: z.string().describe("Absolute path to the .dmp file"),
    pdbPath: z.string().optional().describe("Path to directory containing .pdb symbol files"),
  },
  withErrors(async ({ dumpPath, pdbPath }): Promise<{ content: Array<{ type: "text"; text: string }> }> => {
    // Close any existing session
    if (cdb) await cdb.close();
    cdb = new CdbRunner();
    await cdb.open(dumpPath, pdbPath);

    // Get basic dump info
    const vertarget = await cdb.execute("vertarget");
    const exrOutput = await cdb.execute(".exr -1");
    const threadOutput = await cdb.execute("~");
    const moduleOutput = await cdb.execute("lm");

    const threadCount = (threadOutput.match(/^\s*\d+/gm) ?? []).length;
    const moduleCount = (moduleOutput.match(/^[0-9a-f`]+\s+[0-9a-f`]+/gm) ?? []).length;

    // Parse exception code
    const exCodeMatch = exrOutput.match(/ExceptionCode:\s+(0x[0-9a-f]+)/i);
    const exCode = exCodeMatch?.[1] ?? "unknown";

    const result: ParsedMinidump = {
      dumpPath,
      exceptionCode: exCode,
      exceptionCodeName: mapExceptionCode(exCode),
      faultingThread: 0,
      threadCount,
      moduleCount,
      timestamp: new Date().toISOString(),
      processName: extractProcessName(vertarget),
    };

    return {
      content: [{ type: "text", text: JSON.stringify(result, null, 2) }],
    };
  })
);

// ──────────────────────────────────────────────────────────────────────
// Tool: get_call_stack
// ──────────────────────────────────────────────────────────────────────
server.tool(
  "get_call_stack",
  "Get the call stack for a specific thread in the loaded dump. Returns frames with module, function, source file, and line number.",
  {
    threadId: z.number().optional().describe("Thread ID to inspect (default: faulting thread)"),
    maxFrames: z.number().optional().default(50).describe("Maximum frames to return"),
  },
  withErrors(async ({ threadId, maxFrames }): Promise<{ content: Array<{ type: "text"; text: string }> }> => {
    if (!cdb) throw new Error("No dump loaded. Call parse_minidump first.");

    if (threadId !== undefined) {
      await cdb.execute(`~${threadId}s`);
    }

    const raw = await cdb.execute(`kP ${maxFrames}`);
    const frames: StackFrame[] = parseCallStack(raw);

    return {
      content: [{ type: "text", text: JSON.stringify(frames, null, 2) }],
    };
  })
);

// ──────────────────────────────────────────────────────────────────────
// Tool: resolve_symbols
// ──────────────────────────────────────────────────────────────────────
server.tool(
  "resolve_symbols",
  "Resolve an address to a symbol (function name, source file, line number) using loaded PDB symbols.",
  {
    address: z.string().describe("Virtual address to resolve (hex, e.g. '0x00007ff6abcd1234')"),
  },
  withErrors(async ({ address }): Promise<{ content: Array<{ type: "text"; text: string }> }> => {
    if (!cdb) throw new Error("No dump loaded. Call parse_minidump first.");

    const lnOutput = await cdb.execute(`ln ${address}`);
    const result = parseSymbolOutput(lnOutput, address);

    return {
      content: [{ type: "text", text: JSON.stringify(result, null, 2) }],
    };
  })
);

// ──────────────────────────────────────────────────────────────────────
// Tool: analyze_crash
// ──────────────────────────────────────────────────────────────────────
server.tool(
  "analyze_crash",
  "Run automated crash analysis (equivalent to WinDbg '!analyze -v'). Returns exception details, faulting frame, root cause hypothesis, and call stack.",
  {},
  withErrors(async (): Promise<{ content: Array<{ type: "text"; text: string }> }> => {
    if (!cdb) throw new Error("No dump loaded. Call parse_minidump first.");

    const raw = await cdb.execute("!analyze -v");
    const analysis = parseAnalyzeOutput(raw);

    // Also get the full call stack
    const stackRaw = await cdb.execute("kP 30");
    analysis.callStack = parseCallStack(stackRaw);

    return {
      content: [{ type: "text", text: JSON.stringify(analysis, null, 2) }],
    };
  })
);

// ──────────────────────────────────────────────────────────────────────
// Tool: get_thread_context
// ──────────────────────────────────────────────────────────────────────
server.tool(
  "get_thread_context",
  "Get register state and thread context for a specific thread.",
  {
    threadId: z.number().describe("Thread ID to inspect"),
  },
  withErrors(async ({ threadId }): Promise<{ content: Array<{ type: "text"; text: string }> }> => {
    if (!cdb) throw new Error("No dump loaded. Call parse_minidump first.");

    await cdb.execute(`~${threadId}s`);
    const regOutput = await cdb.execute("r");
    const threadInfo = await cdb.execute(`~${threadId}`);

    const context: ThreadContext = {
      threadId,
      state: threadInfo.includes("Suspend") ? "suspended" : "running",
      priority: 0,
      registers: parseRegisters(regOutput),
      stackPointer: "",
      instructionPointer: "",
    };

    context.stackPointer = context.registers["rsp"] ?? context.registers["esp"] ?? "";
    context.instructionPointer = context.registers["rip"] ?? context.registers["eip"] ?? "";

    return {
      content: [{ type: "text", text: JSON.stringify(context, null, 2) }],
    };
  })
);

// ──────────────────────────────────────────────────────────────────────
// Tool: list_modules
// ──────────────────────────────────────────────────────────────────────
server.tool(
  "list_modules",
  "List all loaded modules (DLLs/executables) with PDB match status. Use to verify symbols are loaded correctly before analysis.",
  {},
  withErrors(async (): Promise<{ content: Array<{ type: "text"; text: string }> }> => {
    if (!cdb) throw new Error("No dump loaded. Call parse_minidump first.");

    const raw = await cdb.execute("lmv");
    const modules = parseModuleList(raw);

    return {
      content: [{ type: "text", text: JSON.stringify(modules, null, 2) }],
    };
  })
);

// ──────────────────────────────────────────────────────────────────────
// Tool: get_memory_region
// ──────────────────────────────────────────────────────────────────────
server.tool(
  "get_memory_region",
  "Read a memory region from the dump at a specific address. Returns hex + ASCII dump. Useful for inspecting heap corruption.",
  {
    address: z.string().describe("Start address (hex)"),
    size: z.number().optional().default(256).describe("Number of bytes to read"),
  },
  withErrors(async ({ address, size }): Promise<{ content: Array<{ type: "text"; text: string }> }> => {
    if (!cdb) throw new Error("No dump loaded. Call parse_minidump first.");

    const raw = await cdb.execute(`db ${address} L${size.toString(16)}`);

    const result: MemoryRegion = {
      address,
      size,
      hexDump: raw,
      asciiDump: extractAsciiColumn(raw),
      protection: "unknown",
    };

    return {
      content: [{ type: "text", text: JSON.stringify(result, null, 2) }],
    };
  })
);

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

function mapExceptionCode(code: string): string {
  const map: Record<string, string> = {
    "0xc0000005": "ACCESS_VIOLATION",
    "0xc0000374": "HEAP_CORRUPTION",
    "0xc00000fd": "STACK_OVERFLOW",
    "0xc0000409": "STACK_BUFFER_OVERRUN",
    "0xc0000417": "INVALID_CRUNTIME_PARAMETER",
    "0x80000003": "BREAKPOINT",
    "0xc000001d": "ILLEGAL_INSTRUCTION",
    "0xc0000094": "INTEGER_DIVIDE_BY_ZERO",
  };
  return map[code.toLowerCase()] ?? `UNKNOWN(${code})`;
}

function extractProcessName(vertarget: string): string {
  const match = vertarget.match(/Image:\s+(.+?)(?:\s|$)/i);
  return match?.[1] ?? "unknown";
}

function parseSymbolOutput(lnOutput: string, address: string): SymbolResolution {
  const match = lnOutput.match(/(\w+)!(.+?)(?:\+0x([0-9a-f]+))?(?:\s+\[(.+?)\s*@\s*(\d+)\])?/i);
  return {
    address,
    module: match?.[1] ?? "unknown",
    function: match?.[2] ?? "unknown",
    sourceFile: match?.[4] ?? null,
    lineNumber: match?.[5] ? parseInt(match[5], 10) : null,
    displacement: match?.[3] ? `0x${match[3]}` : "0x0",
  };
}

function parseRegisters(output: string): Record<string, string> {
  const regs: Record<string, string> = {};
  const matches = output.matchAll(/(\w+)=([0-9a-f`]+)/gi);
  for (const m of matches) {
    regs[m[1].toLowerCase()] = m[2];
  }
  return regs;
}

function parseModuleList(raw: string): ModuleInfo[] {
  const modules: ModuleInfo[] = [];
  const lines = raw.split("\n");

  for (const line of lines) {
    const match = line.match(
      /([0-9a-f`]+)\s+([0-9a-f`]+)\s+(\w+)\s+.*?\(([^)]*)\)/i
    );
    if (match) {
      modules.push({
        name: match[3],
        path: "",
        baseAddress: match[1],
        size: parseInt(match[2], 16) - parseInt(match[1], 16),
        timestamp: "",
        pdbPath: null,
        pdbMatched: line.includes("(pdb symbols)") || line.includes("private pdb"),
      });
    }
  }

  return modules;
}

function extractAsciiColumn(hexDump: string): string {
  return hexDump
    .split("\n")
    .map((line) => {
      const asciiMatch = line.match(/\s{2}(.{1,16})$/);
      return asciiMatch?.[1] ?? "";
    })
    .join("\n");
}

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
