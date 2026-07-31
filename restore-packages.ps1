<#
Vendors Microsoft.Windows.CppWinRT into backend/packages/, without requiring
nuget.exe or the .NET SDK on the machine (neither is guaranteed to be present
- see docs/TECHNICAL.md). Downloads the .nupkg directly from the NuGet v3
flat container and extracts it, matching the layout Wavely.Backend.vcxproj's
explicit <Import> paths expect (see docs/ADR-002-winrt-component-msbuild.md).

Lives at the repo root (not backend/) so every build script is in one place -
see BUILD.md - but the vendored packages themselves stay under backend/packages/
because Wavely.Backend.vcxproj's <Import> paths are hardcoded relative to its
own location (backend/Wavely.Backend/..\packages\...) and moving them would
break the C++ build.
#>
param(
    [string]$Version = "3.0.260715.1"
)

$ErrorActionPreference = 'Stop'
$packagesDir = Join-Path $PSScriptRoot "backend\packages"
$targetDir = Join-Path $packagesDir "Microsoft.Windows.CppWinRT.$Version"

if (Test-Path (Join-Path $targetDir "build\native\Microsoft.Windows.CppWinRT.targets")) {
    Write-Host "Microsoft.Windows.CppWinRT $Version already present in $targetDir"
    return
}

New-Item -ItemType Directory -Force -Path $packagesDir | Out-Null
$nupkgPath = Join-Path $packagesDir "cppwinrt.nupkg"
$url = "https://api.nuget.org/v3-flatcontainer/microsoft.windows.cppwinrt/$Version/microsoft.windows.cppwinrt.$Version.nupkg"

Write-Host "Downloading $url"
Invoke-WebRequest -Uri $url -OutFile $nupkgPath

New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
Expand-Archive -Path $nupkgPath -DestinationPath $targetDir -Force
Remove-Item $nupkgPath

Write-Host "Microsoft.Windows.CppWinRT $Version restored to $targetDir"
