# SPDX-License-Identifier: GPL-3.0-only
$ErrorActionPreference = 'Stop'
$checker = Join-Path $PSScriptRoot '../scripts/check-windows-portable.ps1'
$temporaryBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$fixture = Join-Path $temporaryBase ('pxl-portable-check-' + [guid]::NewGuid().ToString('N'))
$files = @('pxlcone.exe', 'pxlcone_filelink.exe', 'portable.txt',
    'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'Qt6Network.dll', 'platforms/qwindows.dll',
    'qt.conf', 'qtlogging.ini', 'jars/NewLaunch.jar', 'jars/NewLaunchLegacy.jar',
    'jars/JavaCheck.jar', 'LICENSE', 'COPYING.md')
try {
    foreach ($relative in $files) {
        $path = Join-Path $fixture $relative
        [IO.Directory]::CreateDirectory((Split-Path $path)) | Out-Null
        [IO.File]::WriteAllText($path, 'fixture')
    }
    & $checker -PackageRoot $fixture
    $rejected = $false
    try { & $checker -PackageRoot $fixture -RequireMsvcRuntime } catch {
        if ($_.Exception.Message -notlike '*msvcp140.dll*') { throw }
        $rejected = $true
    }
    if (-not $rejected) { throw 'Missing MSVC runtime was accepted.' }
    foreach ($runtime in @('msvcp140.dll', 'vcruntime140.dll')) {
        [IO.File]::WriteAllText((Join-Path $fixture $runtime), 'fixture')
    }
    & $checker -PackageRoot $fixture -RequireMsvcRuntime
    $rejected = $false
    try { & $checker -PackageRoot $fixture -RequireMsvcRuntime -SetupPayload } catch {
        if ($_.Exception.Message -notlike '*portable.txt*') { throw }
        $rejected = $true
    }
    if (-not $rejected) { throw 'Setup payload accepted a portable marker.' }
    Remove-Item -LiteralPath (Join-Path $fixture 'portable.txt')
    & $checker -PackageRoot $fixture -RequireMsvcRuntime -SetupPayload
    foreach ($relative in @('.env', 'CMakeCache.txt', 'debug.log', 'test.exe', 'jars/extra.jar', 'platforms/notes.txt')) {
        $stray = Join-Path $fixture $relative
        [IO.File]::WriteAllText($stray, 'fixture')
        $rejected = $false
        try {
            & (Join-Path $PSScriptRoot '../scripts/build-windows-inno.ps1') -PackageRoot $fixture `
                -Version '0.1.0-alpha.1' -OutputDirectory $temporaryBase -Compiler (Join-Path $fixture 'pxlcone.exe')
        } catch {
            if ($_.Exception.Message -notlike '*Unexpected file in Setup payload*') { throw }
            $rejected = $true
        }
        if (-not $rejected) { throw "Unexpected Setup file was accepted: $relative" }
        Remove-Item -LiteralPath $stray
    }
    [IO.File]::WriteAllText((Join-Path $fixture 'portable.txt'), 'fixture')
    foreach ($name in @('pxlcone.cfg', 'elyprismlauncher.cfg', 'prismlauncher.cfg', 'polymc.cfg', 'multimc.cfg')) {
        $config = Join-Path $fixture $name
        [IO.File]::WriteAllText($config, 'private settings')
        $rejected = $false
        try { & $checker -PackageRoot $fixture -RequireMsvcRuntime } catch {
            if ($_.Exception.Message -notlike '*Personal data*') { throw }
            $rejected = $true
        }
        if (-not $rejected) { throw "Personal configuration was accepted: $name" }
        if ([IO.File]::ReadAllText($config) -ne 'private settings') { throw 'Source configuration changed.' }
        Remove-Item -LiteralPath $config -Force
    }
    [IO.Directory]::CreateDirectory((Join-Path $fixture 'UserData')) | Out-Null
    $rejected = $false
    try { & $checker -PackageRoot $fixture -RequireMsvcRuntime } catch {
        if ($_.Exception.Message -notlike '*Personal data*') { throw }
        $rejected = $true
    }
    if (-not $rejected) { throw 'Personal data was accepted.' }
    Write-Output 'Windows portable checks: complete payload, missing runtime and personal-data guard passed.'
} finally {
    $resolved = [IO.Path]::GetFullPath($fixture)
    if (-not $resolved.StartsWith($temporaryBase, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Refusing to clean a fixture outside its temporary directory.'
    }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
