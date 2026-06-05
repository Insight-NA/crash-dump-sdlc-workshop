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
export declare class CdbRunner {
    private process;
    private outputBuffer;
    private resolveWaiting;
    private symbolPath;
    /**
     * Open a minidump file for analysis.
     */
    open(dumpPath: string, pdbPath?: string): Promise<void>;
    /**
     * Execute a single cdb command and return the output.
     */
    execute(command: string): Promise<string>;
    /**
     * Close the cdb session.
     */
    close(): Promise<void>;
    /**
     * Wait for the initial cdb prompt after opening the dump.
     */
    private waitForPrompt;
}
/**
 * Parse a cdb stack trace output (from `k` or `kP` command) into structured frames.
 */
export declare function parseCallStack(raw: string): import("./types.js").StackFrame[];
/**
 * Parse `!analyze -v` output into a structured analysis result.
 */
export declare function parseAnalyzeOutput(raw: string): Partial<import("./types.js").CrashAnalysis>;
