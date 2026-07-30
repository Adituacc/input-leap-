param(
    [string] $Version = "3.0.3-folder-dragdrop-final1"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$installDir = Join-Path $repoRoot "out\install\windows-release"
$packageRoot = Join-Path $repoRoot "out\package"
$packageName = "input-leap-$Version-windows-x64"
$stageDir = Join-Path $packageRoot $packageName
$binaryZip = Join-Path $packageRoot "$packageName.zip"
$sourceName = "input-leap-$Version-source"
$sourceStage = Join-Path $packageRoot $sourceName
$sourceZip = Join-Path $packageRoot "$sourceName.zip"

if (-not (Test-Path -LiteralPath (Join-Path $installDir "input-leap.exe"))) {
    throw "Release install not found. Run .\scripts\build-windows.ps1 -Configuration Release first."
}

$resolvedPackageRoot = [IO.Path]::GetFullPath($packageRoot)
foreach ($target in @($stageDir, $binaryZip, $sourceStage, $sourceZip)) {
    $resolvedTarget = [IO.Path]::GetFullPath($target)
    if (-not $resolvedTarget.StartsWith(
        $resolvedPackageRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to replace package target outside '$resolvedPackageRoot'."
    }
}

New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
foreach ($target in @($stageDir, $binaryZip, $sourceStage, $sourceZip)) {
    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}

New-Item -ItemType Directory -Path $stageDir -Force | Out-Null
Copy-Item -Path (Join-Path $installDir "*") -Destination $stageDir `
    -Recurse -Force
Compress-Archive -Path $stageDir -DestinationPath $binaryZip `
    -CompressionLevel Optimal

New-Item -ItemType Directory -Path $sourceStage -Force | Out-Null
$sourceFiles = & git -C $repoRoot ls-files --cached --others --exclude-standard
if ($LASTEXITCODE -ne 0) {
    throw "Could not enumerate the source tree with git."
}
foreach ($relativePath in $sourceFiles) {
    $sourcePath = Join-Path $repoRoot $relativePath
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        continue
    }
    $destinationPath = Join-Path $sourceStage $relativePath
    New-Item -ItemType Directory -Path (Split-Path -Parent $destinationPath) `
        -Force | Out-Null
    Copy-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
}
Compress-Archive -Path $sourceStage -DestinationPath $sourceZip `
    -CompressionLevel Optimal
Remove-Item -LiteralPath $sourceStage -Recurse -Force

$hashes = Get-FileHash -Algorithm SHA256 $binaryZip, $sourceZip
$checksumPath = Join-Path $packageRoot "SHA256SUMS-$Version.txt"
$hashes | ForEach-Object {
    "$($_.Hash.ToLowerInvariant())  $(Split-Path -Leaf $_.Path)"
} | Set-Content -LiteralPath $checksumPath -Encoding ascii

Write-Host "Binary package: $binaryZip"
Write-Host "Source package: $sourceZip"
Write-Host "Checksums: $checksumPath"
