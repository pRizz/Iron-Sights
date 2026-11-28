# Capture mission/difficulty hints and best times with isolated RAM-only saves.
param(
    [ValidateSet('Missions','Difficulty')][string]$Menu = 'Difficulty',
    [ValidateRange(0,19)][int]$Mission = 0,
    [ValidateRange(0,3)][int]$HoverDifficulty = 1,
    [switch]$Earned,
    [switch]$UnlockAll,
    [switch]$LockedDifficulty,
    [string]$Resolution = '960x540',
    [ValidateRange(1,1023)][int[]]$BestTimes = @(1023,1023,1023),
    # Headshots Only (FORK-083): records for Agent..00 Agent (0 = none,
    # 1023 = untimed), the mode's setting, and an optional cursor "x,y".
    [ValidateRange(0,1023)][int[]]$HeadshotTimes = @(0,0,0),
    [switch]$HeadshotsOnly,
    [string]$Cursor = '',
    # An isolated build to capture instead of build_win/IronSights.exe (for
    # example build_win/hd-prototype/IronSights.exe while a game is running).
    [string]$Executable = '',
    # Reinforcements (FORK-081): 0 Original, 1 More, 2 Relentless.
    [ValidateRange(0,2)][int]$Reinforcements = 0
)
$ErrorActionPreference='Stop'
if ($BestTimes.Count -ne 3) { throw 'BestTimes needs Agent, Secret Agent and 00 Agent seconds; 1023 means no measured record.' }
if ($HeadshotTimes.Count -ne 3) { throw 'HeadshotTimes needs Agent, Secret Agent and 00 Agent seconds; 0 means none.' }
$repoDir=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$dir=Join-Path $repoDir ('build_win/mission-cheat-audit-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
$auditExe=[IO.Path]::GetFullPath((Join-Path $dir 'IronSights.exe'))
if (!$auditExe.StartsWith($repoDir+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Audit executable must stay inside the workspace.' }
New-Item -ItemType Directory -Path $dir | Out-Null
$sourceExe=if ($Executable) { (Resolve-Path $Executable).Path } else { Join-Path $repoDir 'build_win/IronSights.exe' }
Copy-Item -LiteralPath $sourceExe -Destination $auditExe
# SDL2.dll from the executable's own folder, so a build_win64 exe gets its 64-bit one (WIN64-003).
Copy-Item -LiteralPath (Join-Path (Split-Path $sourceExe) 'SDL2.dll') -Destination $dir
Set-Content -LiteralPath (Join-Path $dir 'midas.ini') -Value @('[display]','fullscreen=window','vsync=0','maxfps=60',"unlock_all=$([int][bool]$UnlockAll)","headshots_only=$([int][bool]$HeadshotsOnly)","reinforcements=$Reinforcements")
$headshotLines=@('[headshots_only]','version=1')
for ($d=0; $d -lt 3; $d++) { if ($HeadshotTimes[$d] -gt 0) { $headshotLines+=('f1_m{0:D2}_d{1}={2}' -f $Mission,$d,$HeadshotTimes[$d]) } }
New-Item -ItemType Directory -Path (Join-Path $dir 'Data/Saves') -Force | Out-Null
Set-Content -LiteralPath (Join-Path $dir 'Data/Saves/headshots.ini') -Value $headshotLines
$xs=@(73,142,212,282,352)
$ys=@(62,131,201,270)
$cursor=if ($Cursor) { $Cursor } elseif ($Menu -eq 'Missions') { "$($xs[$Mission%5]),$($ys[[int][Math]::Floor($Mission/5)])" } else { "200,$(186+30*$HoverDifficulty)" }
$auditEnv=@{ SDL_AUDIODRIVER='dummy';
    GE_ROM=(Join-Path $repoDir 'baserom.u.z64'); GE_WINDOW=$Resolution; GE_NOINPUT='1';
    GE_MENU=$(if ($Menu -eq 'Missions') {'imissions'} else {'jdifficulty'}); GE_CURSOR=$cursor;
    GE_MISSION_CHEAT_AUDIT="$Mission,$([int][bool]$Earned),$([int](!$LockedDifficulty)),$($BestTimes -join ',')";
    GE_SHOT='500'; GE_FRAMES='650'
}
$saved=@{}
try {
    foreach ($key in $auditEnv.Keys) { $saved[$key]=[Environment]::GetEnvironmentVariable($key,'Process'); [Environment]::SetEnvironmentVariable($key,$auditEnv[$key],'Process') }
    $p=Start-Process -FilePath $auditExe -WorkingDirectory $dir -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $dir 'stdout.txt') -RedirectStandardError (Join-Path $dir 'stderr.txt')
    $null=$p.Handle
    if (!$p.WaitForExit(60000)) { $p.Kill(); $p.WaitForExit(); throw "Mission cheat audit timed out: $dir" }
    if ($p.ExitCode -ne 0) { throw "Mission cheat audit failed: $($p.ExitCode) ($dir)" }
} finally {
    foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key,$saved[$key],'Process') }
    if ($p -and $p.HasExited -and (Test-Path -LiteralPath $auditExe)) { Remove-Item -LiteralPath $auditExe }
}
$shots=@(Get-ChildItem -LiteralPath $dir -Filter 'shot_*.bmp')
if (!$shots.Count) { throw "Missing mission cheat capture: $dir" }
$shots | Select-Object -ExpandProperty FullName
