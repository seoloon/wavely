<#
Orchestrates the Wavely build for the new architecture (see docs/ADR-001,
docs/ADR-002): MSBuild compiles the C++/WinRT backend component
(backend/Wavely.Backend), producing Wavely.Backend.winmd + .dll, then
dotnet build compiles the Avalonia frontend (frontend/Wavely.App), which
consumes that component via <CsWinRTInputs> pointing at the backend's build
output (not a ProjectReference - see the addendum in docs/ADR-002).

No business logic lives here (RULES.md SS8): this script only sequences the
two builds, because CMake cannot drive dotnet build, and dotnet build alone
cannot drive the C++/WinRT metadata pipeline outside MSBuild.
#>
param(
    [ValidateSet('Debug', 'Release', 'Distribute')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$repoRoot = $PSScriptRoot

& "$repoRoot\backend\restore-packages.ps1"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found; install Visual Studio 2022." }

$msbuild = & $vswhere -latest -products '*' -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) { throw "MSBuild.exe not found. Install Visual Studio 2022 with the 'Desktop development with C++' workload." }

$backendConfiguration = if ($Configuration -eq 'Distribute') { 'Release' } else { $Configuration }

Write-Host "== Backend (C++/WinRT): Wavely.Backend ==" -ForegroundColor Cyan
& $msbuild "$repoRoot\backend\Wavely.Backend\Wavely.Backend.vcxproj" `
    /p:Configuration=$backendConfiguration `
    /p:Platform=x64 `
    /nologo

if ($LASTEXITCODE -ne 0) {
    Write-Host "Backend build failed (expected on a clean build tree), retrying once..." -ForegroundColor Yellow
    & $msbuild "$repoRoot\backend\Wavely.Backend\Wavely.Backend.vcxproj" `
        /p:Configuration=$backendConfiguration `
        /p:Platform=x64 `
        /nologo

    if ($LASTEXITCODE -ne 0) {
        throw "Backend build failed."
    }
}

Write-Host "== Frontend (Avalonia): Wavely.App ==" -ForegroundColor Cyan

if ($Configuration -eq 'Distribute') {
    $publishDir = Join-Path $repoRoot "dist\win-x64"

    dotnet publish "$repoRoot\frontend\Wavely.App\Wavely.App.csproj" `
        -c Release `
        -r win-x64 `
        --self-contained true `
        -p:PublishSingleFile=true `
        -p:PublishTrimmed=false `
        -o $publishDir

    if ($LASTEXITCODE -ne 0) {
        throw "Frontend publish failed."
    }

    Write-Host "Published to $publishDir" -ForegroundColor Green
}
else {
    dotnet build "$repoRoot\frontend\Wavely.App\Wavely.App.csproj" -c $Configuration

    if ($LASTEXITCODE -ne 0) {
        throw "Frontend build failed."
    }
}

Write-Host "Build complete ($Configuration)." -ForegroundColor Green
