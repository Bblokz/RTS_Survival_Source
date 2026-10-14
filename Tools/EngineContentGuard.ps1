<#
.SYNOPSIS
    Guards the engine's binary content (Engine/Content and Engine/Plugins/**/Content) against accidental saves
    from the RTS_Survival editor.

.DESCRIPTION
    Engine .uasset files are not in git: they come from Setup.bat (GitDependencies). An editor operation that dirties
    an engine asset (for example a force-delete that re-parents an engine blueprint) followed by "save dirty packages"
    silently overwrites the engine copy. Marking engine content read-only turns such a save into a visible
    "file is read-only" error instead.

    -Check      lists engine content that differs from Epic's manifest (GitDependencies --dry-run).
    -Protect    marks all engine content files read-only.
    -Unprotect  clears the read-only flag again (needed before Setup.bat / GitDependencies --force or engine edits).
    -Restore    re-downloads modified engine content from Epic's cache, keeping files listed in $KeepLocalChanges.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Tools\EngineContentGuard.ps1 -Check
#>
param(
    [switch]$Check,
    [switch]$Protect,
    [switch]$Unprotect,
    [switch]$Restore,
    [string]$EngineRoot = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine"
)

# Engine files that intentionally carry a local change; -Restore puts them back after re-downloading.
$KeepLocalChanges = @(
    "Engine\Content\EngineDebugMaterials\VertexColorViewMode_RedOnly.uasset"
)

$GitDependencies = Join-Path $EngineRoot "Engine\Binaries\DotNET\GitDependencies\win-x64\GitDependencies.exe"

function Get-ContentFolders {
    $folders = @(Join-Path $EngineRoot "Engine\Content")
    $folders += Get-ChildItem (Join-Path $EngineRoot "Engine\Plugins") -Directory -Recurse -Filter "Content" -ErrorAction SilentlyContinue |
        ForEach-Object { $_.FullName }
    return $folders
}

function Set-ContentReadOnly([bool]$ReadOnly) {
    $count = 0
    foreach ($folder in Get-ContentFolders) {
        Get-ChildItem $folder -Recurse -File -Include *.uasset, *.umap -ErrorAction SilentlyContinue | ForEach-Object {
            if ($_.IsReadOnly -ne $ReadOnly) {
                $_.IsReadOnly = $ReadOnly
                $count++
            }
        }
    }
    Write-Host ("{0} engine content files {1}" -f $count, $(if ($ReadOnly) { "made read-only" } else { "made writable" }))
}

function Get-ModifiedEngineFiles {
    $output = & $GitDependencies --dry-run --root="$EngineRoot" 2>&1
    return $output | Where-Object { $_ -match '^Update (.+)$' } | ForEach-Object { ($_ -replace '^Update ', '').Trim() }
}

if ($Check) {
    $modified = @(Get-ModifiedEngineFiles)
    $unexpected = $modified | Where-Object { $KeepLocalChanges -notcontains ($_ -replace '/', '\') }
    Write-Host ("Engine content differing from Epic's manifest: {0} ({1} expected local changes)" -f $modified.Count, ($modified.Count - @($unexpected).Count))
    $unexpected | ForEach-Object { Write-Warning "UNEXPECTED CHANGE: $_" }
    if (@($unexpected).Count -gt 0) { exit 1 }
}

if ($Restore) {
    if (Get-Process | Where-Object { $_.ProcessName -like "UnrealEditor*" }) {
        Write-Error "Close the Unreal Editor before restoring engine content."
        exit 1
    }
    $backup = Join-Path $env:TEMP ("EngineContentGuard_" + (Get-Date -Format "yyyyMMdd_HHmmss"))
    New-Item -ItemType Directory -Force $backup | Out-Null
    foreach ($keep in $KeepLocalChanges) {
        $source = Join-Path $EngineRoot $keep
        if (Test-Path $source) { Copy-Item $source (Join-Path $backup ([IO.Path]::GetFileName($keep))) }
    }
    Set-ContentReadOnly $false
    & $GitDependencies --force --root="$EngineRoot"
    foreach ($keep in $KeepLocalChanges) {
        $saved = Join-Path $backup ([IO.Path]::GetFileName($keep))
        if (Test-Path $saved) { Copy-Item $saved (Join-Path $EngineRoot $keep) -Force }
    }
    Write-Host "Restored. Kept local changes from $backup"
}

if ($Unprotect) { Set-ContentReadOnly $false }
if ($Protect) { Set-ContentReadOnly $true }
