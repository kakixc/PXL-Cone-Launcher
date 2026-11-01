# SPDX-License-Identifier: GPL-3.0-only
param(
    [Parameter(Mandatory = $true)][string]$PackageRoot,
    [Parameter(Mandatory = $true)][string]$Version,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [string]$Compiler = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
)
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)-alpha\.(0|[1-9][0-9]*)$') {
    throw 'Use an explicit x.y.z-alpha.N version.'
}
$numeric = "$($Matches[1]).$($Matches[2]).$($Matches[3]).0"
$root = (Resolve-Path -LiteralPath $PackageRoot).Path
$output = (Resolve-Path -LiteralPath $OutputDirectory).Path
$compilerPath = (Resolve-Path -LiteralPath $Compiler).Path
$destination = Join-Path $output "PXL-Cone-Windows-x64-Setup-$Version.exe"
if (Test-Path -LiteralPath $destination) { throw 'Preserve the existing Setup output; choose a new output directory.' }
if ($output -eq $root -or $output.StartsWith($root.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Output must be outside the Setup payload.'
}
& "$PSScriptRoot/check-windows-portable.ps1" -PackageRoot $root -RequireMsvcRuntime -SetupPayload
$files = @(Get-ChildItem -LiteralPath $root -Recurse -Force)
if ($files | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }) {
    throw 'Setup payload cannot contain symbolic links or junctions.'
}
if ($files | Where-Object { $_.Extension -eq '.pdb' -or $_.Name -match '^PXL-Cone-(Test|Diagnostic).*\.exe$' }) {
    throw 'Setup payload contains development artifacts.'
}
# Match the installed application structure, not arbitrary files legitimized by a new manifest.
$rootFiles = @('pxlcone.exe', 'pxlcone_filelink.exe', 'pxlcone_updater.exe', 'qt.conf', 'qtlogging.ini',
    'LICENSE', 'COPYING.md', 'manifest.txt')
$pluginDirectories = @('iconengines', 'imageformats', 'platforms', 'styles', 'tls', 'networkinformation', 'generic')
foreach ($file in $files | Where-Object { -not $_.PSIsContainer }) {
    $relative = [IO.Path]::GetRelativePath($root, $file.FullName).Replace('\', '/')
    $parts = $relative.Split('/')
    $allowed = $false
    if ($parts.Count -eq 1) {
        $allowed = $file.Name -in $rootFiles -or $file.Extension -eq '.dll'
    } elseif ($parts.Count -eq 2 -and $parts[0] -in $pluginDirectories) {
        $allowed = $file.Extension -eq '.dll'
    } elseif ($parts.Count -eq 2 -and $parts[0] -eq 'jars') {
        $allowed = $parts[1] -in @('NewLaunch.jar', 'NewLaunchLegacy.jar', 'JavaCheck.jar')
    } elseif ($parts[0] -eq 'licenses') {
        $allowed = $file.Name -match '^(LICENSE|COPYING|NOTICE|OFL)([.-].*)?$' -or $file.Extension -in @('.txt', '.md')
    }
    if (-not $allowed) { throw "Unexpected file in Setup payload: $relative" }
}
$embedded = (Get-Item -LiteralPath (Join-Path $root 'pxlcone.exe')).VersionInfo.ProductVersion
if ($embedded.Trim() -ne $Version) { throw "Embedded version differs from requested version: $embedded" }
$manifestPath = Join-Path $root 'manifest.txt'
$actual = @($files | Where-Object { -not $_.PSIsContainer } | ForEach-Object {
    [IO.Path]::GetRelativePath($root, $_.FullName).Replace('\', '/')
} | Sort-Object)
$listed = @(Get-Content -LiteralPath $manifestPath -Encoding utf8 | Sort-Object)
if (Compare-Object $actual $listed) { throw 'Setup payload manifest does not match its files.' }
$script = Join-Path $PSScriptRoot '../program_info/pxlcone-setup.iss'
& $compilerPath "/DPayloadDir=$root" "/DOutputDir=$output" "/DReleaseVersion=$Version" "/DNumericVersion=$numeric" $script
if ($LASTEXITCODE -ne 0) { throw "Inno Setup compilation failed: $LASTEXITCODE" }
$digest = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
[IO.File]::WriteAllText("$destination.sha256", "$digest  $([IO.Path]::GetFileName($destination))`n", [Text.Encoding]::ASCII)
Write-Output "Setup built: $destination"
Write-Output "SHA256: $digest"
