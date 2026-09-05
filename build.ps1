$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$build = Join-Path $root 'build'
New-Item -ItemType Directory -Force -Path $build | Out-Null

Push-Location (Join-Path $root 'assets')
try {
    rc.exe /nologo /fo (Join-Path $build 'lightswitch.res') lightswitch.rc
    if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed.' }
} finally {
    Pop-Location
}

cl.exe /nologo /O1 /W4 /DUNICODE /D_UNICODE /c `
    /Fo"$build\lightswitch.obj" "$root\src\lightswitch.c"
if ($LASTEXITCODE -ne 0) { throw 'C compilation failed.' }

link.exe /nologo /SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup /NODEFAULTLIB `
    /OUT:"$build\Lightswitch.exe" "$build\lightswitch.obj" `
    "$build\lightswitch.res" kernel32.lib user32.lib gdi32.lib advapi32.lib
if ($LASTEXITCODE -ne 0) { throw 'Linking failed.' }

Write-Host "Built $build\Lightswitch.exe"

