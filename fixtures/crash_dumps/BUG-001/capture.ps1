# Capture a Windows minidump for BUG-001.
#
# Requires: cl.exe (MSVC 19.38+) on PATH, ProcDump from Sysinternals on PATH.

$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

$root = (Resolve-Path "..\..\..").Path

# /Fo: with a trailing backslash tells cl.exe to put all .obj files in the current dir.
cl.exe /nologo /std:c++20 /EHs-c- /GR- /Zi /Od `
    /I "$root\include" `
    "$root\src\engine_demo\allocator.cpp" `
    "repro.cpp" `
    /Fe:repro.exe /Fo:.\ /Fd:repro.pdb

if ($LASTEXITCODE -ne 0) {
    Write-Error "Compilation failed (exit $LASTEXITCODE)."
    exit $LASTEXITCODE
}

if (Get-Command procdump -ErrorAction SilentlyContinue) {
    procdump -accepteula -ma -e .\repro.exe BUG-001.dmp.local
} else {
    Write-Warning "procdump not found; running unmonitored (no .dmp will be captured)."
    & .\repro.exe
    # Expected: exit code -1073741819 (0xC0000005) = Access Violation from guard-page hit.
    if ($LASTEXITCODE -eq -1073741819) {
        Write-Host "repro.exe crashed with Access Violation (0xC0000005) -- BUG-001 confirmed." -ForegroundColor Green
    } else {
        Write-Warning "Unexpected exit code $LASTEXITCODE -- bug may not have triggered."
    }
}
