# Capture the debrief hit map after scripted hits on a solo stage; never writes the user's settings.
param(
    [string]$Executable,
    [string]$Stage = 'dam',
    [ValidateRange(1,1024)][int]$Hits = 80,
    [switch]$Zoom,
    # The closer look's PARTS tab (opens the closer look too).
    [switch]$Parts,
    # Only characters still standing; GE_HITMAP_LOG's bearing then checks the front/back split.
    [switch]$Live,
    [switch]$Off,
    # Park the menu cursor at native x,y (440x330), e.g. '358,150' over the figures.
    [string]$Cursor,
    # 0-3; a mission's cheat difficulty prints the Target row.
    [ValidateRange(0,3)][int]$Difficulty = 0,
    # Seconds of a seeded RAM-only best time, and the New Cheat notice: the debrief's longest rows.
    [int]$Best = 0,
    [switch]$Cheat,
    # Record at most this many hits; the rest are the Not recorded line.
    [int]$Cap = 0,
    [string]$Resolution = '960x720',
    # Fonts: 0 Original, 1 Enhanced (needs Data/FontPacks/gepd), 2 Iron Sights (FORK-051/052).
    [ValidateRange(0,2)][int]$FontStyle = 0
)
$ErrorActionPreference = 'Stop'
$repoDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (!$Executable) { $Executable = Join-Path $repoDir 'build_win\IronSights.exe' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$dir = Join-Path $repoDir ('build_win\hitmap-audit-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
$auditExe = [IO.Path]::GetFullPath((Join-Path $dir 'IronSights.exe'))
if (!$auditExe.StartsWith($repoDir+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Audit executable must stay inside the workspace.' }
New-Item -ItemType Directory -Path $dir | Out-Null
Copy-Item -LiteralPath $Executable -Destination $auditExe
Copy-Item -LiteralPath (Join-Path (Split-Path $Executable) 'SDL2.dll') -Destination $dir
Set-Content -LiteralPath (Join-Path $dir 'midas.ini') -Value @('[display]', 'fullscreen=window', 'vsync=0', 'maxfps=60',
    'tickhz=60', 'render_thread=0', 'msaa=0', 'music_volume=0', "debrief_hit_map=$([int](!$Off))", "font_style=$FontStyle")
$auditEnv = @{
    GE_ROM=(Join-Path $repoDir 'baserom.u.z64'); GE_WINDOW=$Resolution; GE_NOINPUT='1'; GE_SEED='1';
    GE_IRONSIGHTS_FONTS='1';   # FORK-051: allows the hidden Iron Sights font when asked for
    GE_STAGE=$Stage; GE_STAGE_AFTER='1'; GE_SKIPINTRO='1'; GE_CHEATS='invuln';
    GE_DIFF=[string]$Difficulty;
    GE_HITMAP_PROBE=(@([string]$Hits) + @(if ($Zoom) { 'zoom' }) + @(if ($Parts) { 'parts' }) + @(if ($Live) { 'live' }) + @(if ($Best) { "best=$Best" }) + @(if ($Cheat) { 'cheat' }) + @(if ($Cap) { "cap=$Cap" })) -join ','; GE_HITMAP_LOG='1';
    # The probe exits after its capture; this only bounds a run that never gets there.
    GE_FRAMES='12000'
}
if ($Cursor) { $auditEnv.GE_CURSOR = $Cursor }
$saved = @{}
try {
    foreach ($key in $auditEnv.Keys) {
        $saved[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
        [Environment]::SetEnvironmentVariable($key, $auditEnv[$key], 'Process')
    }
    $p = Start-Process -FilePath $auditExe -WorkingDirectory $dir -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $dir 'stdout.log') -RedirectStandardError (Join-Path $dir 'stderr.log')
    # Cache the process handle so Windows PowerShell retains the redirected child's exit code.
    $null = $p.Handle
} finally {
    foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key, $saved[$key], 'Process') }
}
Write-Output "Hit map audit: $dir"
try {
    if (!$p.WaitForExit(300000)) { $p.Kill(); $p.WaitForExit(); throw "Hit map audit timed out: $dir" }
} finally {
    if ($p.HasExited -and (Test-Path -LiteralPath $auditExe)) { Remove-Item -LiteralPath $auditExe }
}
if ($p.ExitCode -ne 0) { throw "Game exited with code $($p.ExitCode)." }
Select-String -LiteralPath (Join-Path $dir 'stderr.log') -Pattern 'hitmap probe' | ForEach-Object { $_.Line }
$shots = @(Get-ChildItem -LiteralPath $dir -Filter 'shot_*.bmp')
if (!$shots.Count) { throw "Missing debrief capture: $dir" }
$shots | Select-Object -ExpandProperty FullName
