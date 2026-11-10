# BuildInstaller.ps1
# Builds complete installer: copies files + compiles with Inno Setup

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
Set-Location $ProjectRoot

Write-Host "[1/3] Preparing GAMEDIRECTORY..." -ForegroundColor Cyan

# ---------------------------------------------------------------------------
# Source paths
# ---------------------------------------------------------------------------
$SourceRelease      = "GGPals_CustomEngine\GGEngine\bin\ReleaseGame"
$SourceConfig       = "GGPals_CustomEngine\GGEngine\config"
$SourceAssets       = "GGPals_CustomEngine\GGProjects\Order&Chaos\Assets"
$SourceEngineAssets = "GGPals_CustomEngine\EngineAssets"

# ---------------------------------------------------------------------------
# Destination base inside GAMEDIRECTORY (mirrors the runtime-relative layout)
# ---------------------------------------------------------------------------
$DestBase = "GGPals_InstallerData\GAMEDIRECTORY\GGPals_CustomEngine"

# Guard: release build must exist before doing anything
if (-not (Test-Path "$SourceRelease\OrderNChaos.exe")) {
    Write-Host "ERROR: Release build not found at $SourceRelease" -ForegroundColor Red
    Write-Host "       Build the ReleaseGame configuration in Visual Studio first." -ForegroundColor Red
    pause
    exit 1
}

# Guard: project assets must exist
if (-not (Test-Path $SourceAssets)) {
    Write-Host "ERROR: Assets folder not found at $SourceAssets" -ForegroundColor Red
    pause
    exit 1
}

# Guard: engine assets must exist
if (-not (Test-Path $SourceEngineAssets)) {
    Write-Host "ERROR: EngineAssets folder not found at $SourceEngineAssets" -ForegroundColor Red
    pause
    exit 1
}

# Clean old GAMEDIRECTORY so stale files don't accumulate between builds
if (Test-Path "GGPals_InstallerData\GAMEDIRECTORY") {
    Remove-Item "GGPals_InstallerData\GAMEDIRECTORY\*" -Recurse -Force -ErrorAction SilentlyContinue
}

# Create the staging folder skeleton
New-Item -ItemType Directory -Force -Path "$DestBase\GGEngine\bin\Release"          | Out-Null
New-Item -ItemType Directory -Force -Path "$DestBase\GGEngine\config"               | Out-Null
New-Item -ItemType Directory -Force -Path "$DestBase\GGProjects\Order&Chaos\Assets" | Out-Null

# Copy exe and all DLLs
Copy-Item "$SourceRelease\OrderNChaos.exe" "$DestBase\GGEngine\bin\Release\" -Force
Copy-Item "$SourceRelease\*.dll"           "$DestBase\GGEngine\bin\Release\" -Force

# Copy runtime config
Copy-Item "$SourceConfig\config.json" "$DestBase\GGEngine\config\" -Force

# Copy all project assets including .meta files
# .meta files are required at runtime -- GGAssetManager::RegisterAssetsWithUUIDs() scans
# them at startup to build the UUID-to-filepath registry used by scenes and prefabs
# Without them, every UUID lookup fails and no textures load
# Shaders\ subfolder is excluded -- project-level shaders are not used by the game exe
Write-Host "    Copying project assets (excluding Shaders folder and SourceSans3 fonts)..." -ForegroundColor Gray
$resolvedSource = (Resolve-Path $SourceAssets).Path
Get-ChildItem $SourceAssets -Recurse -File |
    Where-Object { $_.FullName -notlike "*\Shaders\*" -and $_.Name -notlike "SourceSans3*" } |
    ForEach-Object {
        $dest = $_.FullName.Replace($resolvedSource, "$DestBase\GGProjects\Order&Chaos\Assets")
        New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
        Copy-Item $_.FullName $dest -Force
    }
$assetCount = (Get-ChildItem "$DestBase\GGProjects\Order&Chaos\Assets" -Recurse -File).Count
Write-Host "    $assetCount project asset files copied" -ForegroundColor Green

# Copy engine assets -- exclude only Brick_Wall.png and its .meta
# Keeping only SourceSans3-Bold.ttf and SourceSans3-Regular.ttf -- referenced by GGEngineRender
# Excluding Amiri-Bold and SourceCodePro-Black (not referenced in any runtime code)
# Excluding Brick_Wall.png (tilemap editor tool placeholder)
Write-Host "    Copying engine assets (SourceSans3 fonts and shaders only, excluding Amiri, SourceCodePro, Brick_Wall)..." -ForegroundColor Gray
$resolvedEngineSource = (Resolve-Path $SourceEngineAssets).Path
Get-ChildItem $SourceEngineAssets -Recurse -File |
    Where-Object { $_.Name -ne "Brick_Wall.png" -and $_.Name -ne "Brick_Wall.png.meta" -and $_.Name -notlike "Amiri-Bold*" -and $_.Name -notlike "SourceCodePro*" } |
    ForEach-Object {
        $dest = $_.FullName.Replace($resolvedEngineSource, "$DestBase\EngineAssets")
        New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
        Copy-Item $_.FullName $dest -Force
    }
$engineAssetCount = (Get-ChildItem "$DestBase\EngineAssets" -Recurse -File).Count
Write-Host "    $engineAssetCount engine asset files copied" -ForegroundColor Green

# ---------------------------------------------------------------------------
# Compile installer
# ---------------------------------------------------------------------------
Write-Host "[2/3] Compiling installer..." -ForegroundColor Cyan

$InnoSetup = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if (-not (Test-Path $InnoSetup)) {
    Write-Host "ERROR: Inno Setup not found." -ForegroundColor Red
    Write-Host "       Download from https://jrsoftware.org/isinfo.php" -ForegroundColor Red
    pause
    exit 1
}

Set-Location "GGPals_InstallerData"
& $InnoSetup "InstallScript.iss" | Out-Null

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Inno Setup compilation failed (exit code $LASTEXITCODE)" -ForegroundColor Red
    pause
    exit 1
}

# ---------------------------------------------------------------------------
# Verify output
# ---------------------------------------------------------------------------
Write-Host "[3/3] Verifying..." -ForegroundColor Cyan

if (Test-Path "INSTALLER\OrderNChaos_Setup.exe") {
    $size = [math]::Round((Get-Item "INSTALLER\OrderNChaos_Setup.exe").Length / 1MB, 1)
    Write-Host "SUCCESS: Installer created ($size MB)" -ForegroundColor Green
    Write-Host "Output: GGPals_InstallerData\INSTALLER\OrderNChaos_Setup.exe" -ForegroundColor White
} else {
    Write-Host "ERROR: Installer not created -- check Inno Setup output above" -ForegroundColor Red
}

Write-Host ""
pause