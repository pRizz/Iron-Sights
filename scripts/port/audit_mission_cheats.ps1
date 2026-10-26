# Capture mission/difficulty cheat hints with RAM-only save fixtures in an isolated directory.
param(
    [ValidateSet('Missions','Difficulty')][string]$Menu = 'Difficulty',
    [ValidateRange(0,19)][int]$Mission = 0,
    [ValidateRange(0,3)][int]$HoverDifficulty = 1,
    [switch]$Earned,
    [switch]$UnlockAll,
    [switch]$LockedDifficulty,
    [string]$Resolution = '960x540'
)
$ErrorActionPreference='Stop'
$repoDir=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$dir=Join-Path $repoDir ('build_win/mission-cheat-audit-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
$auditExe=[IO.Path]::GetFullPath((Join-Path $dir 'IronSights.exe'))
if (!$auditExe.StartsWith($repoDir+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Audit executable must stay inside the workspace.' }
New-Item -ItemType Directory -Path $dir | Out-Null
Copy-Item -LiteralPath (Join-Path $repoDir 'build_win/IronSights.exe') -Destination $auditExe
Copy-Item -LiteralPath (Join-Path $repoDir 'build_win/SDL2.dll') -Destination $dir
Set-Content -LiteralPath (Join-Path $dir 'midas.ini') -Value @('[display]','fullscreen=window','vsync=0','maxfps=60',"unlock_all=$([int][bool]$UnlockAll)")
$xs=@(73,142,212,282,352)
$ys=@(62,131,201,270)
$cursor=if ($Menu -eq 'Missions') { "$($xs[$Mission%5]),$($ys[[int][Math]::Floor($Mission/5)])" } else { "200,$(186+30*$HoverDifficulty)" }
$auditEnv=@{
    GE_ROM=(Join-Path $repoDir 'baserom.u.z64'); GE_WINDOW=$Resolution; GE_NOINPUT='1';
    GE_MENU=$(if ($Menu -eq 'Missions') {'imissions'} else {'jdifficulty'}); GE_CURSOR=$cursor;
    GE_MISSION_CHEAT_AUDIT="$Mission,$([int][bool]$Earned),$([int](!$LockedDifficulty))";
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
