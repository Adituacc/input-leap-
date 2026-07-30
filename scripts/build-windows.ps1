param(
    [ValidateSet("Debug", "Release")]
    [string] $Configuration = "Debug",

    [string] $QtRoot,
    [string] $OpenSslRoot,
    [string] $BonjourRoot,

    [string] $VersionDescription = "dragdrop-final1",

    [switch] $SkipTests
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot

if (-not $QtRoot) {
    $QtRoot = Join-Path $repoRoot ".deps\Qt\6.6.3\msvc2019_64"
}
if (-not $OpenSslRoot) {
    $OpenSslRoot = Join-Path $repoRoot ".deps\Qt\Tools\OpenSSLv3\Win_x64"
}
if (-not $BonjourRoot) {
    $BonjourRoot = Join-Path $repoRoot ".deps\BonjourSDKLike"
}

$requiredPaths = @{
    "Qt" = Join-Path $QtRoot "bin\Qt6Core.dll"
    "OpenSSL" = Join-Path $OpenSslRoot "bin\libcrypto-3-x64.dll"
    "Bonjour SDK" = Join-Path $BonjourRoot "Lib\x64\dnssd.lib"
}
foreach ($dependency in $requiredPaths.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $dependency.Value)) {
        throw "$($dependency.Key) was not found at '$($dependency.Value)'."
    }
}

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    $cmake = $cmakeCommand.Source
}
else {
    $cmake = Get-ChildItem (Join-Path $env:APPDATA "Python") -Filter cmake.exe `
        -File -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $cmake) {
    throw "CMake was not found. Install CMake 3.24 or newer and add it to PATH."
}

$ctest = Join-Path (Split-Path -Parent $cmake) "ctest.exe"
if (-not (Test-Path -LiteralPath $ctest)) {
    throw "CTest was not found next to CMake at '$ctest'."
}

$configurationName = $Configuration.ToLowerInvariant()
$buildDir = Join-Path $repoRoot "out\build\windows-$configurationName"
$installDir = Join-Path $repoRoot "out\install\windows-$configurationName"
$dnssdLibrary = Join-Path $BonjourRoot "Lib\x64\dnssd.lib"
$dnssdInclude = Join-Path $BonjourRoot "Include"

$configureArguments = @(
    "-S", $repoRoot,
    "-B", $buildDir,
    "--fresh",
    "-G", "Visual Studio 17 2022",
    "-A", "x64",
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DQT_DEFAULT_MAJOR_VERSION=6",
    "-DCMAKE_PREFIX_PATH=$QtRoot",
    "-DOPENSSL_ROOT_DIR=$OpenSslRoot",
    "-DDNSSD_LIB=$dnssdLibrary",
    "-DDNSSD_INCLUDE_DIR=$dnssdInclude",
    "-DINPUTLEAP_VERSION_DESC=$VersionDescription",
    "-DCMAKE_INSTALL_PREFIX=$installDir"
)

Write-Host "Configuring $Configuration build..."
& $cmake @configureArguments
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed with exit code $LASTEXITCODE."
}

Write-Host "Building $Configuration targets..."
& $cmake --build $buildDir --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

if (-not $SkipTests) {
    $originalPath = $env:PATH
    $originalQpaPlatform = $env:QT_QPA_PLATFORM
    try {
        $runtimePaths = @(
            (Join-Path $QtRoot "bin"),
            (Join-Path $OpenSslRoot "bin")
        )

        if ($Configuration -eq "Debug") {
            $visualStudioRoots = @(
                "C:\Program Files\Microsoft Visual Studio\2022",
                "C:\Program Files (x86)\Microsoft Visual Studio\2022"
            ) | Where-Object { Test-Path -LiteralPath $_ }
            $debugCrt = Get-ChildItem $visualStudioRoots `
                -Filter "Microsoft.VC143.DebugCRT" -Directory -Recurse `
                -ErrorAction SilentlyContinue |
                Where-Object {
                    $_.FullName -like "*\debug_nonredist\x64\Microsoft.VC143.DebugCRT"
                } |
                Sort-Object FullName -Descending |
                Select-Object -First 1 -ExpandProperty FullName

            $debugUcrt = Get-ChildItem `
                "C:\Program Files (x86)\Windows Kits\10\bin" `
                -Directory -ErrorAction SilentlyContinue |
                Sort-Object Name -Descending |
                ForEach-Object { Join-Path $_.FullName "x64\ucrt" } |
                Where-Object { Test-Path -LiteralPath $_ } |
                Select-Object -First 1

            if (-not $debugCrt -or -not $debugUcrt) {
                throw "The Visual C++ and Universal CRT debug runtimes were not found."
            }
            $runtimePaths += $debugCrt, $debugUcrt
        }

        $env:PATH = ($runtimePaths + $originalPath) -join [IO.Path]::PathSeparator
        $env:QT_QPA_PLATFORM = "offscreen"

        Write-Host "Running tests..."
        & $ctest --test-dir $buildDir -C $Configuration --output-on-failure
        if ($LASTEXITCODE -ne 0) {
            throw "Tests failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        $env:PATH = $originalPath
        $env:QT_QPA_PLATFORM = $originalQpaPlatform
    }
}

Write-Host "Installing into '$installDir'..."
& $cmake --install $buildDir --config $Configuration
if ($LASTEXITCODE -ne 0) {
    throw "Installation failed with exit code $LASTEXITCODE."
}

Get-ChildItem (Join-Path $OpenSslRoot "bin") -Filter "lib*-3-x64.dll" -File |
    Copy-Item -Destination $installDir -Force

Write-Host "Build complete: $(Join-Path $installDir 'input-leap.exe')"
