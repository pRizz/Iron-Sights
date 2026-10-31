param([string]$Executable = '', [switch]$RenderThread,
    [ValidateSet(60,120)][int]$TickHz = 60, [int]$RoomCache = 1,
    [string]$RestoreFrom = '', [switch]$ActiveAtSave, [int]$Msaa = 0,
    [int]$DelaySeconds = 0, [switch]$ChangeWeapon, [string]$Stage = 'dam',
    [string]$Spawn = '', [switch]$WeaponHud, [switch]$StressAds)
$ErrorActionPreference = 'Stop'
$repoDir = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (!$Executable) { $Executable = Join-Path $repoDir 'build_win/IronSights.exe' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$dir = Join-Path $repoDir ('build_win/state-effects-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
$auditExe = [IO.Path]::GetFullPath((Join-Path $dir 'IronSights.exe'))
if (!$auditExe.StartsWith($repoDir + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Audit executable must stay inside the workspace.' }
New-Item -ItemType Directory -Path $dir | Out-Null
Copy-Item -LiteralPath $Executable -Destination $auditExe
Copy-Item -LiteralPath (Join-Path (Split-Path $Executable) 'SDL2.dll') -Destination $dir
Set-Content -LiteralPath (Join-Path $dir 'midas.ini') -Value @('[display]', 'fullscreen=window', 'vsync=0', "maxfps=$TickHz", "tickhz=$TickHz", "room_cache=$RoomCache", "msaa=$Msaa", 'mission_hints=0', "weapon_hud=$(if ($WeaponHud) {1} else {0})", 'weapon_hud_icons=1', 'always_health=0', "render_thread=$(if ($RenderThread) {1} else {0})", '[input]', 'mouse=1')
Set-Content -LiteralPath (Join-Path $dir 'input.txt') -Value @('P 0 0 0 0', 'P 80 8000 0 0', 'P 82 0 0 0', 'P 120 8000 0 0', 'P 122 0 0 0', 'P 160 8000 0 0', 'P 162 0 0 0', 'P 1800 0 0 0')
if ($StressAds) {
    # Port action 21 is ADS. Raise/lower the generated arms before saving.
    Add-Content -LiteralPath (Join-Path $dir 'input.txt') -Value @('A 0 0 0', 'A 900 200000 0', 'A 1150 0 0', 'A 6000 0 0')
}
$auditEnv = @{ GE_ROM=(Join-Path $repoDir 'baserom.u.z64'); GE_WINDOW='960x540'; GE_NOINPUT='1'; GE_STAGE=$Stage; GE_DIFF='0'; GE_STAGE_AFTER='1'; GE_CHEATS='invuln,invis'; GE_FRAMES='6000'; GE_SEED='1'; GE_INPUTPLAY=(Join-Path $dir 'input.txt'); GE_STATE_EFFECTS_AUDIT='1'; GE_NOINISAVE='1' }
if ($Spawn) { $auditEnv.GE_SPAWNXYZ = $Spawn }
if ($StressAds) { $auditEnv.GE_STATE_EFFECTS_ADS = '1'; $auditEnv.GE_ROOM_AUDIT = '1' }
if ($ActiveAtSave) { $auditEnv.GE_STATE_EFFECTS_ACTIVE = '1' }
$auditEnv.GE_STATE_EFFECTS_DELAY = [string][Math]::Max(1, $DelaySeconds * $TickHz)
if ($ChangeWeapon) { $auditEnv.GE_STATE_EFFECTS_WEAPON = '1' }
if ($RestoreFrom) {
    New-Item -ItemType Directory -Path (Join-Path $dir 'Data/SaveStates') -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $RestoreFrom 'Data/SaveStates/profile-0-slot-5.mstate') -Destination (Join-Path $dir 'Data/SaveStates/profile-0-slot-5.mstate')
    $auditEnv.GE_STATE_EFFECTS_READ = '1'; $auditEnv.GE_STAGE = 'facility'
}
$saved = @{}
try {
    foreach ($key in $auditEnv.Keys) { $saved[$key] = [Environment]::GetEnvironmentVariable($key, 'Process'); [Environment]::SetEnvironmentVariable($key, $auditEnv[$key], 'Process') }
    $p = Start-Process -FilePath $auditExe -WorkingDirectory $dir -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $dir 'stdout.log') -RedirectStandardError (Join-Path $dir 'stderr.log')
    $null = $p.Handle
} finally {
    foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key, $saved[$key], 'Process') }
}
Write-Output "State effects audit: $dir"
try { $p.WaitForExit() }
finally { if ($p.HasExited -and (Test-Path -LiteralPath $auditExe)) { Remove-Item -LiteralPath $auditExe } }
if ($p.ExitCode -ne 0 -or !(Test-Path -LiteralPath (Join-Path $dir 'state-effects-captured.txt'))) { throw "Effect sequence capture failed: $dir (exit $($p.ExitCode))" }
$captures = @(Get-ChildItem -LiteralPath $dir -Filter 'shot_*.bmp')
if ($captures.Count -ne 24) { throw "Unexpected effect capture count: $($captures.Count)" }
$captures | Select-Object -ExpandProperty FullName
