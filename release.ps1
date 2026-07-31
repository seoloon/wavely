<#
Publishes a packaged Wavely release to GitHub Releases (github.com/seoloon/wavely) via the
Velopack CLI. Reuses package.ps1 for the build+publish+pack steps (same layering as
build.ps1 -> package.ps1: this script only adds the upload step, it doesn't reimplement
packaging).

Requires:
- The Velopack CLI (`vpk`) installed: `dotnet tool install -g vpk`.
- A GITHUB_TOKEN environment variable holding a GitHub PAT with `repo` scope (needed even to
  create a draft release). Never committed, never written to disk by this script.

By default the GitHub release is created as a DRAFT (vpk's own default when --publish is
omitted) - nothing reaches users' auto-updaters until the draft is published manually on
GitHub. Pass -Publish to publish immediately instead.

CLI syntax verified live against vpk 1.2.0 (`vpk upload github --help`) - do not assume flag
names without re-checking if vpk is upgraded, its flags have changed across versions before
(see package.ps1's own header comment).
#>
param(
    [Parameter(Mandatory)][string]$Version,
    [switch]$Publish,
    [switch]$PreRelease
)

$ErrorActionPreference = 'Stop'
$repoRoot = $PSScriptRoot

if (-not $env:GITHUB_TOKEN) {
    throw "GITHUB_TOKEN environment variable is not set - needed to upload to GitHub Releases (PAT with 'repo' scope)."
}

Write-Host "== Package (Release) ==" -ForegroundColor Cyan
& "$repoRoot\package.ps1" -Version $Version
if ($LASTEXITCODE -ne 0) { throw "Packaging failed." }

$releasesDir = "$repoRoot\dist\Releases"

Write-Host "== Upload to GitHub Releases ==" -ForegroundColor Cyan
vpk upload github `
    --outputDir $releasesDir `
    --repoUrl "https://github.com/seoloon/wavely" `
    --token $env:GITHUB_TOKEN `
    --tag "v$Version" `
    --releaseName "Wavely v$Version" `
    --publish $($Publish.IsPresent) `
    --pre $($PreRelease.IsPresent)
if ($LASTEXITCODE -ne 0) { throw "vpk upload failed." }

if ($Publish.IsPresent) {
    Write-Host "Release v$Version published live on GitHub." -ForegroundColor Green
} else {
    Write-Host "Release v$Version uploaded as a DRAFT - publish it manually on GitHub when ready." -ForegroundColor Yellow
}
