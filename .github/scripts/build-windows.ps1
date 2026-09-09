param(
    [Parameter(Mandatory)][ValidateSet('x86', 'x64')][string] $Architecture,
    [Parameter(Mandatory)][ValidateSet('v142', 'v143')][string] $Toolset,
    [Parameter(Mandatory)][ValidateSet('static', 'shared')][string] $Linkage,
    [Parameter(Mandatory)][string] $WorkDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (-not $IsWindows) { throw 'The Windows build requires MSVC on Windows' }

function Invoke-Native([string] $Command, [string[]] $Arguments) {
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Command failed with exit code $LASTEXITCODE" }
}

$source = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$work = [IO.Path]::GetFullPath($WorkDirectory)
if (Test-Path -LiteralPath $work) { throw "Build directory already exists: $work" }
New-Item -ItemType Directory -Path $work | Out-Null
$manifest = Get-Content -LiteralPath (Join-Path $source 'vcpkg.json') -Raw | ConvertFrom-Json
$version = $manifest.'version-string'
$baseline = $manifest.'builtin-baseline'
if ($version -notmatch '^[0-9]+\.[0-9]+\.[0-9]+$' -or $baseline -notmatch '^[0-9a-f]{40}$') {
    throw 'Invalid version or vcpkg baseline in vcpkg.json'
}
if ($env:GITHUB_REF_TYPE -eq 'tag' -and $env:GITHUB_REF_NAME -cne "v$version") {
    throw "Tag $env:GITHUB_REF_NAME does not match package version v$version"
}

# Only the header-only JSON port is needed. PHP consumers need no parser libraries.
$vcpkg = Join-Path $work 'vcpkg'
$dependencies = Join-Path $work 'dependencies'
Invoke-Native git @('init', $vcpkg)
Invoke-Native git @('-C', $vcpkg, 'fetch', '--depth=1', 'https://github.com/microsoft/vcpkg.git', $baseline)
Invoke-Native git @('-C', $vcpkg, 'checkout', '--detach', 'FETCH_HEAD')
Invoke-Native (Join-Path $vcpkg 'bootstrap-vcpkg.bat') @('-disableMetrics')
Invoke-Native (Join-Path $vcpkg 'vcpkg.exe') @(
    'install', 'nlohmann-json:x64-windows', '--classic', "--x-install-root=$dependencies"
)

$build = Join-Path $work 'build'
$prefix = Join-Path $work 'installed'
$cmakeArchitecture = if ($Architecture -eq 'x86') { 'Win32' } else { 'x64' }
$staticCli = if ($Linkage -eq 'static') { 'ON' } else { 'OFF' }
$generator = @('-G', 'Visual Studio 17 2022', '-A', $cmakeArchitecture, '-T', $Toolset)
$runtime = '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>DLL'
Invoke-Native cmake (@('-S', $source, '-B', $build) + $generator + @(
    $runtime, "-DCMAKE_INSTALL_PREFIX=$prefix", '-DMUSTACHE_WARNINGS_AS_ERRORS=ON',
    '-DMUSTACHE_ENABLE_TESTS=ON', '-DMUSTACHE_ENABLE_ARCHIVED_TEMPLATES=ON',
    '-DMUSTACHE_ENABLE_JSON=ON', '-DMUSTACHE_ENABLE_YAML=OFF',
    "-Dnlohmann_json_DIR=$dependencies/x64-windows/share/nlohmann_json",
    "-DMUSTACHE_CLI_STATIC=$staticCli"
))
Invoke-Native cmake @('--build', $build, '--config', 'Release', '--parallel', '2')
Invoke-Native ctest @('--test-dir', $build, '-C', 'Release', '--output-on-failure',
    '--output-junit', "$build/ctest-results.xml", '--output-log', "$build/ctest-output.log")
Invoke-Native cmake @('--install', $build, '--config', 'Release')

$archive = & (Join-Path $PSScriptRoot '../../scripts/package-windows.ps1') -InstallPrefix $prefix `
    -SourceDirectory $source -JsonLicense "$dependencies/x64-windows/share/nlohmann-json/copyright" `
    -OutputDirectory (Join-Path $work 'archives') -Version $version `
    -Architecture $Architecture -Toolset $Toolset -Linkage $Linkage

# Verify the extracted SDK without the original build/install directories on PATH.
$sdk = Join-Path $work 'extracted'
Expand-Archive -LiteralPath $archive -DestinationPath $sdk
$consumer = Join-Path $work 'consumer'
Invoke-Native cmake (@('-S', "$source/tests/cmake-consumer", '-B', $consumer) + $generator + @(
    $runtime, "-DCMAKE_PREFIX_PATH=$sdk", "-DMUSTACHE_CONSUMER_LINKAGE=$Linkage"
))
Invoke-Native cmake @('--build', $consumer, '--config', 'Release', '--parallel', '2')
$originalPath = $env:PATH
try {
    $env:PATH = "$sdk/bin;$originalPath"
    Invoke-Native "$consumer/Release/mustache_consumer.exe" @()
    Invoke-Native "$sdk/bin/mustachec.exe" @('-v')
    $template = Join-Path $work 'example.mustache'
    $data = Join-Path $work 'example.json'
    [IO.File]::WriteAllText($template, 'Hello, {{name}}!', [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($data, '{"name":"PHP"}', [Text.UTF8Encoding]::new($false))
    $rendered = & "$sdk/bin/mustachec.exe" -t $template -d $data
    if ($LASTEXITCODE -ne 0 -or $rendered -cne 'Hello, PHP!') {
        throw "Packaged executable failed to render JSON: $rendered"
    }
} finally {
    $env:PATH = $originalPath
}
