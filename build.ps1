<#
Orchestrates the Wavely build for the new architecture (see docs/ADR-001,
docs/ADR-002): MSBuild compiles the C++/WinRT backend component
(backend/Wavely.Backend), producing Wavely.Backend.winmd + .dll, then
dotnet build compiles the Avalonia frontend (frontend/Wavely.App), which
consumes that component via <CsWinRTInputs> pointing at the backend's build
output (not a ProjectReference - see the addendum in docs/ADR-002).

No business logic lives here (RULES.md SS8): this script only sequences the
two builds, because CMake cannot drive dotnet build, and dotnet build alone
cannot drive the C++/WinRT metadata pipeline outside MSBuild. Producing a
distributable build is a separate concern - see package.ps1/release.ps1 and
BUILD.md.
#>
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$repoRoot = $PSScriptRoot

& "$repoRoot\restore-packages.ps1"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found; install Visual Studio 2022." }

$msbuild = & $vswhere -latest -products '*' -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) { throw "MSBuild.exe not found. Install Visual Studio 2022 with the 'Desktop development with C++' workload." }

Write-Host "== Backend (C++/WinRT): Wavely.Backend ==" -ForegroundColor Cyan
& $msbuild "$repoRoot\backend\Wavely.Backend\Wavely.Backend.vcxproj" /p:Configuration=$Configuration /p:Platform=x64 /nologo
if ($LASTEXITCODE -ne 0) {
    # On a fully clean build tree (e.g. right after a fresh clone), the first MSBuild pass
    # fails to link: the <ClCompile Include="...module.g.cpp" Condition="Exists(...)"> item is
    # evaluated before the CppWinRT component-projection target generates that file, so it's
    # silently excluded from pass 1 and WINRT_GetActivationFactory/WINRT_CanUnloadNow end up
    # undefined (LNK2001). module.g.cpp exists on disk by the time pass 2 evaluates the same
    # Condition, so it links cleanly. See docs/ADR-002-winrt-component-msbuild.md addendum.
    Write-Host "Backend build failed (expected on a clean build tree - see build.ps1 comment), retrying once..." -ForegroundColor Yellow
    & $msbuild "$repoRoot\backend\Wavely.Backend\Wavely.Backend.vcxproj" /p:Configuration=$Configuration /p:Platform=x64 /nologo
    if ($LASTEXITCODE -ne 0) { throw "Backend build failed." }
}

Write-Host "== Frontend (Avalonia): Wavely.App ==" -ForegroundColor Cyan
dotnet build "$repoRoot\frontend\Wavely.App\Wavely.App.csproj" -c $Configuration
if ($LASTEXITCODE -ne 0) { throw "Frontend build failed." }

Write-Host "Build complete ($Configuration)." -ForegroundColor Green
