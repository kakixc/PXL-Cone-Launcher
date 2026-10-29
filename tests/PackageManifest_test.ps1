# SPDX-License-Identifier: GPL-3.0-only
$ErrorActionPreference = 'Stop'
$script = Join-Path $PSScriptRoot '../scripts/write-package-manifest.ps1'
$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([char[]]'\/')
$fixture = Join-Path $tempRoot ('pxl-package-manifest-' + [Guid]::NewGuid().ToString('N'))
try {
    $normal = Join-Path $fixture 'install'
    $portable = Join-Path $fixture 'install-portable'
    $files = @('installer-helper.exe', 'tls/qopensslbackend.dll', 'styles/qwindowsvistastyle.dll',
               '.hidden', 'folder with spaces/example.txt', 'assets/пример.png')
    foreach ($relative in $files) {
        $path = Join-Path $normal $relative
        New-Item -ItemType Directory -Path (Split-Path $path) -Force | Out-Null
        [IO.File]::WriteAllText($path, 'fixture')
    }
    & $script -PackageRoot $normal
    $manifest = Join-Path $normal 'manifest.txt'
    $expected = @('manifest.txt') + $files | Sort-Object
    $actual = [IO.File]::ReadAllLines($manifest)
    if (@(Compare-Object $expected $actual).Count -ne 0) { throw 'Normal manifest differs from the actual files.' }
    $bytes = [IO.File]::ReadAllBytes($manifest)
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 239 -and $bytes[1] -eq 187 -and $bytes[2] -eq 191) { throw 'Unexpected UTF-8 BOM.' }
    $originalHash = (Get-FileHash -LiteralPath $manifest).Hash
    & $script -PackageRoot $normal
    if ((Get-FileHash -LiteralPath $manifest).Hash -ne $originalHash) { throw 'Manifest regeneration is not idempotent.' }
    Copy-Item -LiteralPath $normal -Destination $portable -Recurse
    [IO.File]::WriteAllText((Join-Path $portable 'portable.txt'), '')
    & $script -PackageRoot $portable
    $portableFiles = [IO.File]::ReadAllLines((Join-Path $portable 'manifest.txt'))
    if (@(Compare-Object ($expected + 'portable.txt' | Sort-Object) $portableFiles).Count -ne 0) { throw 'Portable manifest is incorrect.' }
    if ((Get-FileHash -LiteralPath $manifest).Hash -ne $originalHash) { throw 'Portable packaging modified the normal manifest.' }
    $rejected = $false
    try { & $script -PackageRoot (Join-Path $normal 'installer-helper.exe') } catch { $rejected = $true }
    if (-not $rejected) { throw 'A file was accepted as PackageRoot.' }
    Write-Output 'Package manifest checks passed: paths, hidden files, spaces, Unicode, file-only entries, UTF-8, idempotence, portable isolation and invalid input.'
} finally {
    $resolved = [IO.Path]::GetFullPath($fixture)
    if ($resolved.StartsWith($tempRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $resolved)) {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
