/**
 * Shared types for crash dump analysis MCP server.
 */
export interface StackFrame {
    frameNumber: number;
    module: string;
    function: string;
    sourceFile: string | null;
    lineNumber: number | null;
    offset: string;
    address: string;
}
export interface ModuleInfo {
    name: string;
    path: string;
    baseAddress: string;
    size: number;
    timestamp: string;
    pdbPath: string | null;
    pdbMatched: boolean;
}
export interface ExceptionRecord {
    code: string;
    codeName: string;
    address: string;
    threadId: number;
    firstChance: boolean;
}
export interface CrashAnalysis {
    exception: ExceptionRecord;
    faultingFrame: StackFrame | null;
    callStack: StackFrame[];
    rootCauseHypothesis: string;
    confidence: "high" | "medium" | "low";
    additionalContext: string[];
}
export interface ThreadContext {
    threadId: number;
    state: string;
    priority: number;
    registers: Record<string, string>;
    stackPointer: string;
    instructionPointer: string;
}
export interface MemoryRegion {
    address: string;
    size: number;
    hexDump: string;
    asciiDump: string;
    protection: string;
}
export interface ParsedMinidump {
    dumpPath: string;
    exceptionCode: string;
    exceptionCodeName: string;
    faultingThread: number;
    threadCount: number;
    moduleCount: number;
    timestamp: string;
    processName: string;
}
export interface SymbolResolution {
    address: string;
    module: string;
    function: string;
    sourceFile: string | null;
    lineNumber: number | null;
    displacement: string;
}
