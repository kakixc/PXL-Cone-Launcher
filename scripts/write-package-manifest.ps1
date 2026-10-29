# SPDX-License-Identifier: GPL-3.0-only
param([Parameter(Mandatory = $true)][string]$PackageRoot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $PackageRoot).Path
if (-not (Test-Path -LiteralPath $root -PathType Container)) {
    throw 'PackageRoot must be a directory.'
}
$prefix = $root.TrimEnd([char[]]'\/') + [IO.Path]::DirectorySeparatorChar
$manifest = Join-Path $root 'manifest.txt'
$entries = @('manifest.txt') + @(Get-ChildItem -LiteralPath $root -File -Recurse -Force | ForEach-Object {
    if (-not $_.FullName.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'A package file is outside PackageRoot.'
    }
    if (-not $_.FullName.Equals($manifest, [StringComparison]::OrdinalIgnoreCase)) {
        $_.FullName.Substring($prefix.Length).Replace('\', '/')
    }
})
# The updater reads UTF-8 paths relative to the package, not directories or
# absolute machine-specific names. Regeneration must replace a copied manifest.
[IO.File]::WriteAllLines($manifest, [string[]]($entries | Sort-Object -Unique), [Text.UTF8Encoding]::new($false))
