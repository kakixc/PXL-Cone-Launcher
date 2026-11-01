# SPDX-License-Identifier: GPL-3.0-only
param(
    [Parameter(Mandatory = $true)][string]$PackageRoot,
    [switch]$RequireMsvcRuntime
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $PackageRoot).Path
$required = @(
    'pxlcone.exe', 'pxlcone_filelink.exe', 'portable.txt',
    'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'Qt6Network.dll',
    'platforms/qwindows.dll', 'qt.conf', 'qtlogging.ini',
    'jars/NewLaunch.jar', 'jars/NewLaunchLegacy.jar', 'jars/JavaCheck.jar',
    'LICENSE', 'COPYING.md'
)
if ($RequireMsvcRuntime) {
    $required += @('msvcp140.dll', 'vcruntime140.dll')
}
$missing = @($required | Where-Object { -not (Test-Path -LiteralPath (Join-Path $root $_) -PathType Leaf) })
if ($missing.Count -gt 0) {
    throw "Incomplete Windows portable package: $($missing -join ', ')"
}
$privateNames = @('UserData', 'instances', 'accounts.json', 'pxlcone.cfg', 'elyprismlauncher.cfg',
    'prismlauncher.cfg', 'polymc.cfg', 'multimc.cfg', 'logs')
$privateFiles = @(Get-ChildItem -LiteralPath $root -Recurse -Force | Where-Object { $_.Name -in $privateNames })
if ($privateFiles.Count -gt 0) {
    throw 'Personal data found in Windows portable package. Use a clean CMake installation.'
}
Write-Output 'Windows portable package contents: OK (not a clean-system runtime test).'
