# Capture new HUD elements in an isolated game directory; never writes the user's settings.
param(
    [string]$Executable,
    [ValidateSet('Wide','Classic')][string]$Aspect = 'Wide',
    [string]$Resolution,
    [switch]$BodyHit,
    [switch]$Icons,
    [switch]$AutomaticHealth,
    [int]$Item = 8,
    [switch]$Disabled,
    [switch]$Menu,
    [switch]$MissionTimer,
    [switch]$Dialogue,
    [switch]$SmallHud,
    [switch]$Chop,
    [ValidateRange(0,1023)][int]$BestSeconds = 0,
    [string]$Stage = 'dam',
    [ValidateRange(0,3)][int]$Difficulty = 0,
    [ValidateSet(1,2,4)][int]$Players = 1
)
$ErrorActionPreference = 'Stop'
if ($Dialogue) { $Stage = 'statue' }
$repoDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (!$Executable) { $Executable = Join-Path $repoDir 'build_win\IronSights.exe' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$dir = Join-Path $repoDir ('build_win\hud-audit-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
$auditExe = [IO.Path]::GetFullPath((Join-Path $dir 'IronSights.exe'))
if (!$auditExe.StartsWith($repoDir+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Audit executable must stay inside the workspace.' }
New-Item -ItemType Directory -Path $dir | Out-Null
Copy-Item -LiteralPath $Executable -Destination (Join-Path $dir 'IronSights.exe')
Copy-Item -LiteralPath (Join-Path (Split-Path $Executable) 'SDL2.dll') -Destination $dir
$ini = @('[display]', 'fullscreen=window', 'vsync=0', 'maxfps=60', 'tickhz=60', 'msaa=0', 'aim_mode=1', 'ammo_on_screen=1', '[input]', 'mouse=1')
if ($Icons) { $ini = @('[display]', 'weapon_hud_icons=1') + $ini }
if ($AutomaticHealth) { $ini = @('[display]', 'always_health=0') + $ini }
if ($MissionTimer) { $ini = @('[display]', 'mission_timer=1') + $ini }
if ($SmallHud) { $ini = @('[display]', 'hud_scale=1') + $ini }
if ($Disabled) { $ini = @('[display]', 'hit_markers=0', 'damage_indicators=0', 'objective_notifications=0', 'weapon_hud=0', 'always_health=0') + $ini }
Set-Content -LiteralPath (Join-Path $dir 'midas.ini') -Value $ini
$auditEnv = @{
    GE_ROM=(Join-Path $repoDir 'baserom.u.z64'); GE_WINDOW=$(if ($Resolution) {$Resolution} elseif ($Aspect -eq 'Wide') {'960x540'} else {'800x600'});
    GE_NOINPUT='1'; GE_STAGE=$Stage; GE_DIFF=[string]$Difficulty; GE_STAGE_AFTER='1'; GE_CHEATS='invuln,invis,guns';
    GE_HUD_AUDIT=[string]$Item; GE_FRAMES='2100'; GE_SEED='1'
}
if ($BodyHit) { $auditEnv.GE_HUD_AUDIT_BODY = '1' }
if ($Dialogue) { $auditEnv.GE_HUD_AUDIT_DIALOGUE = '1' }
if ($Chop) {
    $auditEnv.GE_HUD_AUDIT = '1'
    $auditEnv.GE_HUD_AUDIT_CHOP = '1'
    # Owning the sniper replaces upstream's chop with its rifle-butt attack.
    $auditEnv.GE_CHEATS = 'invuln,invis'
    $auditEnv.GE_SHOT = '10'
    $auditEnv.GE_FRAMES = '700'
}
if ($BestSeconds) { $auditEnv.GE_HUD_AUDIT_BEST = [string]$BestSeconds }
# Fresh intro-skip taps stop before live play, so they cannot cycle weapons.
if (!$Menu -and $Players -eq 1) {
    Set-Content -LiteralPath (Join-Path $dir 'input.txt') -Value @('P 0 0 0 0',
        'P 80 8000 0 0', 'P 82 0 0 0', 'P 120 8000 0 0', 'P 122 0 0 0',
        'P 160 8000 0 0', 'P 162 0 0 0', 'P 2200 0 0 0')
    $auditEnv.GE_INPUTPLAY = Join-Path $dir 'input.txt'
    if ($Chop) {
        Set-Content -LiteralPath (Join-Path $dir 'input.txt') -Value @('P 0 0 0 0',
            'P 80 8000 0 0', 'P 82 0 0 0', 'P 120 8000 0 0', 'P 122 0 0 0',
            'P 160 8000 0 0', 'P 162 0 0 0', 'P 320 2000 0 0','P 440 0 0 0','P 2200 0 0 0')
    }
}
if ($Menu) { $auditEnv.Remove('GE_STAGE'); $auditEnv.Remove('GE_HUD_AUDIT'); $auditEnv.GE_MENU='hud'; $auditEnv.GE_SHOT='500'; $auditEnv.GE_FRAMES='650' }
if ($Players -gt 1) { $auditEnv.Remove('GE_STAGE'); $auditEnv.GE_MP='temple'; $auditEnv.GE_MPPLAYERS=[string]$Players; $auditEnv.GE_SKIPINTRO='1' }
$saved = @{}
try {
    foreach ($key in $auditEnv.Keys) {
        $saved[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
        [Environment]::SetEnvironmentVariable($key, $auditEnv[$key], 'Process')
    }
    $p = Start-Process -FilePath (Join-Path $dir 'IronSights.exe') -WorkingDirectory $dir -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $dir 'stdout.log') -RedirectStandardError (Join-Path $dir 'stderr.log')
    # Cache the process handle so Windows PowerShell retains the redirected child's exit code.
    $null = $p.Handle
} finally {
    foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key, $saved[$key], 'Process') }
}
Write-Output "HUD audit: $dir"
try { $p.WaitForExit() }
finally {
    if ($p.HasExited -and (Test-Path -LiteralPath $auditExe)) { Remove-Item -LiteralPath $auditExe }
}
if ($p.ExitCode -ne 0) { throw "Game exited with code $($p.ExitCode)." }
$captures = @(Get-ChildItem -LiteralPath $dir -Filter 'shot_*.bmp')
if ($captures.Count -lt $(if ($Menu) {1} else {3})) { throw "Missing HUD captures ($($captures.Count))." }
if ($Chop) { Write-Output "Verified $($captures.Count) captures, including chop motion: $dir" }
else { $captures | Select-Object -ExpandProperty FullName }
