<#
Produces a deliverable installer for Wavely (Task 28): builds the backend + frontend in
Release configuration, publishes the frontend as a self-contained win-x64 app (so end users
don't need the .NET 8 runtime installed), then packs that publish output into a Velopack
Setup.exe installer.

This is deliberately a separate script from build.ps1 (see that script's own header comment:
"this script only sequences the two builds" - packaging is a distinct concern). This script
reuses build.ps1 rather than re-implementing the MSBuild/dotnet build orchestration.

Requires the Velopack CLI (`vpk`) to be installed: `dotnet tool install -g vpk`.
CLI syntax verified live against vpk 1.2.0 (`vpk --help` / `vpk pack --help`) - do not assume
flag names without re-checking if vpk is upgraded, its flags have changed across versions.
#>
param(
    [string]$Version = '0.1.0'
)

$ErrorActionPreference = 'Stop'
$repoRoot = $PSScriptRoot

Write-Host "== Build (Release) ==" -ForegroundColor Cyan
& "$repoRoot\build.ps1" -Configuration Release
if ($LASTEXITCODE -ne 0) { throw "Release build failed." }

$publishDir = "$repoRoot\dist\publish"
$releasesDir = "$repoRoot\dist\Releases"

Write-Host "== Publish (self-contained win-x64) ==" -ForegroundColor Cyan
if (Test-Path $publishDir) { Remove-Item -Recurse -Force $publishDir }
dotnet publish "$repoRoot\frontend\Wavely.App\Wavely.App.csproj" -c Release -r win-x64 --self-contained true -o "$publishDir"
if ($LASTEXITCODE -ne 0) { throw "Frontend publish failed." }

$backendDll = Join-Path $publishDir 'Wavely.Backend.dll'
if (-not (Test-Path $backendDll)) {
    throw "Wavely.Backend.dll is missing from the publish output ($publishDir). The <None Include=...> CopyToOutputDirectory item in Wavely.App.csproj did not survive 'dotnet publish' - investigate before packaging."
}
Write-Host "Verified Wavely.Backend.dll is present in publish output." -ForegroundColor DarkGray

Write-Host "== Pack (Velopack) ==" -ForegroundColor Cyan
vpk pack `
    --packId "Wavely" `
    --packVersion $Version `
    --packDir "$publishDir" `
    --mainExe "Wavely.App.exe" `
    --packTitle "Wavely" `
    --outputDir "$releasesDir"
if ($LASTEXITCODE -ne 0) { throw "vpk pack failed." }

$setupExe = Get-ChildItem -Path $releasesDir -Filter '*Setup.exe' -File | Select-Object -First 1
if (-not $setupExe) {
    throw "vpk pack reported success but no *Setup.exe was found in $releasesDir."
}
$setupExePath = $setupExe.FullName

Write-Host "Installer ready: $setupExePath" -ForegroundColor Green
