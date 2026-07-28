param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateRange(1, 20)]
    [int]$Runs = 5,

    [ValidateRange(1, 1000000000)]
    [int]$Iterations = 2000000
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildName = $Configuration.ToLowerInvariant()
$benchmark = Join-Path $repoRoot "out\build\windows-$buildName\bin\$Configuration\protocol_benchmark.exe"

if (-not (Test-Path -LiteralPath $benchmark)) {
    throw "Benchmark is not built: $benchmark. Run scripts\build-windows.ps1 first."
}

$samples = for ($run = 1; $run -le $Runs; ++$run) {
    $output = & $benchmark $Iterations
    if ($LASTEXITCODE -ne 0) {
        throw "Benchmark failed with exit code $LASTEXITCODE."
    }

    $match = [regex]::Match($output, "ns_per_operation=([0-9.]+)")
    if (-not $match.Success) {
        throw "Could not parse benchmark output: $output"
    }

    $value = [double]::Parse(
        $match.Groups[1].Value,
        [Globalization.CultureInfo]::InvariantCulture
    )
    [pscustomobject]@{
        Run = $run
        NanosecondsPerOperation = $value
    }
}

$sorted = @($samples.NanosecondsPerOperation | Sort-Object)
$median = $sorted[[int][math]::Floor($sorted.Count / 2)]
$average = ($samples | Measure-Object NanosecondsPerOperation -Average).Average

$samples | Format-Table -AutoSize
[pscustomobject]@{
    Configuration = $Configuration
    Runs = $Runs
    IterationsPerRun = $Iterations
    MedianNanosecondsPerOperation = [math]::Round($median, 3)
    AverageNanosecondsPerOperation = [math]::Round($average, 3)
} | Format-List
