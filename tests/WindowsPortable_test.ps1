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
