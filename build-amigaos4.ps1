param(
    [ValidateSet('configure', 'build', 'clean')]
    [string]$Action = 'build',
    [int]$Jobs = 0
)

$ErrorActionPreference = 'Stop'
$projectDir = (Resolve-Path $PSScriptRoot).Path
$image = 'walkero/amigagccondocker:os4-gcc11'
$containerProject = '/work/widelands'
$buildDir = "$containerProject/build-amigaos4"

if ($Jobs -le 0) {
    $Jobs = [Environment]::ProcessorCount
}

if ($Action -eq 'clean') {
    if (Test-Path (Join-Path $projectDir 'build-amigaos4')) {
        Remove-Item -Recurse -Force -LiteralPath (Join-Path $projectDir 'build-amigaos4')
    }
    exit 0
}

if ($Action -eq 'build') {
    $versionFile = Join-Path $projectDir 'amigaos4-build-version.txt'
    $currentVersion = (Get-Content -LiteralPath $versionFile -Raw).Trim()
    if ($currentVersion -notmatch '^(\d+)\.(\d+)\.(\d+)$') {
        throw "Invalid AmigaOS4 build version: $currentVersion"
    }
    $nextVersion = '{0}.{1}.{2}' -f $Matches[1], $Matches[2], ([int]$Matches[3] + 1)
    Set-Content -LiteralPath $versionFile -Value $nextVersion -Encoding ascii
    Write-Host "AmigaOS4 port build $nextVersion"
}

$configureArgs = @(
    'run', '--rm',
    '-v', "${projectDir}:${containerProject}",
    '-w', $containerProject,
    $image,
    'cmake', '-S', '.', '-B', 'build-amigaos4',
    '-DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/AmigaOS4.cmake',
    '-DCMAKE_BUILD_TYPE=Release',
    '-DOPTION_BUILD_TESTS=OFF',
    '-DOPTION_BUILD_CODECHECK=OFF',
    '-DOPTION_BUILD_WEBSITE_TOOLS=OFF',
    '-DOPTION_AMIGAOS4_VIRTIO_GL=ON',
    '-DOPTION_AMIGAOS4_VIRTIO_NO_SHADERS=ON',
    '-DOPTION_USE_ICU=OFF',
    '-DOPTION_BUILD_NETWORK=OFF',
    '-Dasio_location=/work/widelands/auto_dependencies/asio/include',
    '-DOPTION_ASAN=OFF',
    '-DUSE_FLTO_IF_AVAILABLE=OFF',
    '-DUSE_XDG=OFF'
)

& docker @configureArgs
if ($LASTEXITCODE -ne 0 -or $Action -eq 'configure') {
    exit $LASTEXITCODE
}

& docker run --rm -v "${projectDir}:${containerProject}" -w $containerProject $image `
    cmake --build $buildDir --parallel $Jobs
exit $LASTEXITCODE
