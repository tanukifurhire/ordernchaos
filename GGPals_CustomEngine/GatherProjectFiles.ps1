# GatherProjectFiles.ps1
# Collects all code/implementation files for project backup or analysis

param(
    [string]$DestinationPath = "$env:USERPROFILE\Documents\GAM"
)

# Create destination if it doesn't exist
if (-not (Test-Path $DestinationPath)) {
    New-Item -ItemType Directory -Path $DestinationPath -Force | Out-Null
    Write-Host "Created folder: $DestinationPath"
}

$baseDir = "C:\git\csd2401f25_team_ggpals\GGPals_CustomEngine"

# Source directories for code files
$codeDirs = @(
    "$baseDir\GGEditor\include",
    "$baseDir\GGEditor\src",
    "$baseDir\GGEngine\include",
    "$baseDir\GGEngine\src",
    "$baseDir\GGMath\include\GGMath"
)

# Specific root files
$rootFiles = @(
    "$baseDir\CLAUDE.MD",
    "$baseDir\CMakeLists.txt",
    "$baseDir\editorGuide.pdf",
    "$baseDir\GGPals_ReadMe.txt",
    "$baseDir\GGScriptingReference.MD",
    "$baseDir\projectGuide.txt",
    "$baseDir\_git_ls-files.txt"
)

# Copy files from code directories (exclude .aps, .rc, .ico)
foreach ($dir in $codeDirs) {
    Write-Host "`nProcessing directory: $dir"
    if (Test-Path $dir) {
        $files = Get-ChildItem -Path $dir -File -Recurse | Where-Object {
            $_.Extension -ne ".aps" -and $_.Extension -ne ".rc" -and $_.Extension -ne ".ico"
        }
        Write-Host "Found $($files.Count) files (excluding .aps, .rc, .ico)"
        
        foreach ($file in $files) {
            Copy-Item $file.FullName -Destination $DestinationPath -Force
            Write-Host "  Copied: $($file.Name)"
        }
    } else {
        Write-Host "  Directory not found!" -ForegroundColor Red
    }
}

# Copy specific root files
Write-Host "`nCopying root files..."
foreach ($file in $rootFiles) {
    if (Test-Path $file) {
        Copy-Item $file -Destination $DestinationPath -Force
        Write-Host "  Copied: $(Split-Path $file -Leaf)"
    } else {
        Write-Host "  Not found: $(Split-Path $file -Leaf)" -ForegroundColor Yellow
    }
}

# Copy all files from Assets EXCEPT Animations, Textures, Audio, Fonts
Write-Host "`nProcessing Assets folder..."
$assetsDir = "$baseDir\Assets"
if (Test-Path $assetsDir) {
    $assetFiles = Get-ChildItem -Path $assetsDir -File -Recurse | Where-Object {
        $_.FullName -notmatch "\\Animations\\" -and
        $_.FullName -notmatch "\\Textures\\" -and
        $_.FullName -notmatch "\\Audio\\" -and
        $_.FullName -notmatch "\\Fonts\\"
    }
    Write-Host "Found $($assetFiles.Count) asset files (excluding Animations, Textures, Audio, Fonts)"
    
    foreach ($file in $assetFiles) {
        Copy-Item $file.FullName -Destination $DestinationPath -Force
        Write-Host "  Copied: $($file.Name)"
    }
}

Write-Host "`nDone. Files copied to: $DestinationPath"