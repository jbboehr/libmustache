param(
    [Parameter(Mandatory)][string] $InstallPrefix,
    [Parameter(Mandatory)][string] $SourceDirectory,
    [Parameter(Mandatory)][string] $JsonLicense,
    [Parameter(Mandatory)][string] $OutputDirectory,
    [Parameter(Mandatory)][ValidatePattern('^[0-9]+\.[0-9]+\.[0-9]+$')][string] $Version,
    [Parameter(Mandatory)][ValidateSet('x86', 'x64')][string] $Architecture,
    [Parameter(Mandatory)][ValidateSet('v142', 'v143')][string] $Toolset,
    [Parameter(Mandatory)][ValidateSet('static', 'shared')][string] $Linkage
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$name = "libmustache-$Version-windows-$Architecture-$Toolset-md-$Linkage"
$package = Join-Path $OutputDirectory $name
if (Test-Path -LiteralPath $package) {
    throw "Package staging directory already exists: $package"
}

$targetKind = if ($Linkage -eq 'static') { 'Static' } else { 'Shared' }
$files = @(
    'bin/mustachec.exe',
    'include/mustache/mustache_config.h',
    'include/mustache/mustache_export.hpp',
    'lib/cmake/mustache/mustacheConfig.cmake',
    'lib/cmake/mustache/mustacheConfigVersion.cmake',
    "lib/cmake/mustache/mustache${targetKind}Targets.cmake",
    "lib/cmake/mustache/mustache${targetKind}Targets-release.cmake"
)
if ($Linkage -eq 'static') {
    $files += 'lib/mustache_static.lib'
} else {
    $files += @('lib/mustache.lib', 'bin/mustache.dll')
}
foreach ($relativePath in $files) {
    $path = Join-Path $InstallPrefix $relativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing required package file: $path"
    }
}

New-Item -ItemType Directory -Path $package | Out-Null
foreach ($relativePath in $files) {
    $destination = Join-Path $package $relativePath
    New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $InstallPrefix $relativePath) -Destination $destination
}
Copy-Item -LiteralPath (Join-Path $InstallPrefix 'include/mustache') `
    -Destination (Join-Path $package 'include') -Recurse -Force
Copy-Item -LiteralPath (Join-Path $SourceDirectory 'LICENSE.md') -Destination $package
Copy-Item -LiteralPath (Join-Path $SourceDirectory 'docs/windows-binaries.md') `
    -Destination (Join-Path $package 'README.md')
foreach ($dependency in @('cista', 'xxhash')) {
    $destination = Join-Path $package "licenses/$dependency"
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $SourceDirectory "vendor/$dependency/LICENSE") -Destination $destination
}
$jsonLicenseDirectory = Join-Path $package 'licenses/nlohmann-json'
New-Item -ItemType Directory -Path $jsonLicenseDirectory -Force | Out-Null
Copy-Item -LiteralPath $JsonLicense -Destination (Join-Path $jsonLicenseDirectory 'LICENSE')

$archive = Join-Path $OutputDirectory "$name.zip"
[IO.Compression.ZipFile]::CreateFromDirectory($package, $archive)
$digest = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$archive.sha256", "$digest *$name.zip`n", [Text.Encoding]::ASCII)
Write-Output $archive
