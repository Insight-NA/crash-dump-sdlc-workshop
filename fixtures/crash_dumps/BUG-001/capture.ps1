# Capture a Windows minidump for BUG-001.
#
# Requires: cl.exe (MSVC 19.38+) on PATH.
# procdump is optional; if absent the self-capturing repro (capture_self.cpp) is used.

$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

$root = (Resolve-Path "..\..\..").Path

# Bootstrap MSVC environment if cl.exe is not directly on PATH (e.g., plain PowerShell).
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue | Where-Object { $_.Source -like "*MSVC*" })) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsInstall = & $vswhere -latest -products '*' -property installationPath
        $vcvars = Join-Path $vsInstall "VC\Auxiliary\Build\vcvarsall.bat"
        if (Test-Path $vcvars) {
            # Import env vars from vcvarsall.bat into current PowerShell session.
            $envOutput = cmd /c "`"$vcvars`" x64 > nul 2>&1 && set"
            foreach ($line in $envOutput) {
                if ($line -match "^([^=]+)=(.*)$") {
                    [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
                }
            }
            Write-Host "MSVC environment initialized via $vcvars"
        }
    }
}

if (Get-Command procdump -ErrorAction SilentlyContinue) {
    # Preferred path: procdump captures the dump externally.
    cl.exe /nologo /std:c++20 /EHs-c- /GR- /Zi /Od `
        /I "$root\include" `
        "$root\src\engine_demo\allocator.cpp" `
        "repro.cpp" `
        /Fe:repro.exe /Fo:.\ /Fd:repro.pdb
    if ($LASTEXITCODE -ne 0) { Write-Error "Compilation failed."; exit $LASTEXITCODE }
    procdump -accepteula -ma -e .\repro.exe BUG-001.dmp
} else {
    # Fallback: self-capturing repro uses SetUnhandledExceptionFilter + MiniDumpWriteDump.
    Write-Warning "procdump not found; using self-capturing repro (capture_self.cpp)."
    cl.exe /nologo /std:c++20 /EHs-c- /GR- /Zi /Od `
        /I "$root\include" `
        "$root\src\engine_demo\allocator.cpp" `
        "capture_self.cpp" `
        /Fe:capture_self.exe /Fo:.\ /Fd:capture_self.pdb
    if ($LASTEXITCODE -ne 0) { Write-Error "Compilation failed."; exit $LASTEXITCODE }
    & .\capture_self.exe
    if ($LASTEXITCODE -eq -1073741819) {
        Write-Host "BUG-001 confirmed (0xC0000005). BUG-001.dmp written." -ForegroundColor Green
    } else {
        Write-Warning "Unexpected exit $LASTEXITCODE -- check allocator bug state."
    }
}
