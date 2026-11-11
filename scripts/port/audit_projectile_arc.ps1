# Projectile Arc (FORK-069): fire scripted grenade-launcher rounds, rockets,
# tank shells, hand grenades, throwing knives or remote mines at several pitches (from the hip unless -Ads), capture the drawn arc, and compare where each
# round really explodes with the prediction. Never writes the user's settings.
param(
    [string]$Executable,
    [string]$Stage = 'dam',
    [ValidateSet('gl', 'rocket', 'tank', 'shells', 'grenade', 'knife', 'mine')][string]$Mode = 'gl',
    [string]$Pitch = '-12,-4,4,12,24',
    [int]$Hold = 360,
    [string]$Spawn,
    [string]$Goto,
    [string]$Resolution = '960x540',
    [switch]$NoFire,
    [switch]$Ads,
    # Draw faster than the 60 Hz tick and walk about (GE_DRIVE=1), to check
    # the arc stays single while moving.
    [int]$MaxFps = 60,
    [switch]$Drive,
    # The same weapon in the left hand too (dual wield): an arc for each.
    [switch]$Dual,
    # Turn towards the nearest living character each tick; -Pitch then
    # counts from the line to its feet (a knife striking a guard).
    [switch]$FaceChr,
    # Draw the character-hit marker (a knife's, or a round's burst) at every
    # arc's end, to capture its look.
    [switch]$StrikeMarker,
    # Leave the arc itself off (the Arc Impact Crosshair still predicts).
    [switch]$NoArc,
    # Draw every Arc Impact Crosshair as out of sight (its dashed cross).
    [switch]$HiddenCross
)
$ErrorActionPreference = 'Stop'
$repoDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (!$Executable) { $Executable = Join-Path $repoDir 'build_win\IronSights.exe' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$rom = Join-Path $repoDir 'baserom.u.z64'
if (!(Test-Path -LiteralPath $rom)) { $rom = Join-Path (Split-Path (git -C $repoDir rev-parse --path-format=absolute --git-common-dir)) 'baserom.u.z64' }
$dir = Join-Path $repoDir ('build_win\arc-audit-' + $Mode + '-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
$auditExe = [IO.Path]::GetFullPath((Join-Path $dir 'IronSights.exe'))
if (!$auditExe.StartsWith($repoDir+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Audit executable must stay inside the workspace.' }
New-Item -ItemType Directory -Path $dir | Out-Null
Copy-Item -LiteralPath $Executable -Destination $auditExe
Copy-Item -LiteralPath (Join-Path (Split-Path $Executable) 'SDL2.dll') -Destination $dir
# The audit turns the arc on in memory; the ini leaves it at its default.
Set-Content -LiteralPath (Join-Path $dir 'midas.ini') -Value @('[display]', 'fullscreen=window', 'vsync=0', "maxfps=$MaxFps",
    'tickhz=60', 'render_thread=0', 'msaa=0', 'music_volume=0', 'sfx_volume=0')
$pitches = @($Pitch.Split(',')).Count
$auditEnv = @{
    GE_ROM=$rom; GE_WINDOW=$Resolution; GE_NOINPUT='1'; GE_SEED='1';
    GE_STAGE=$Stage; GE_STAGE_AFTER='1'; GE_SKIPINTRO='1'; GE_CHEATS='invuln,invis,guns';
    GE_ARC_AUDIT=$(if ($Mode -eq 'gl') { '1' } else { $Mode }); GE_ARC_AUDIT_PITCH=$Pitch;
    GE_ARC_AUDIT_HOLD=[string]$Hold; GE_ARCDBG='1'; GE_AUTOFIRE='600';
    # Stage load and intro, then each pitch in turn; the audit exits itself.
    GE_FRAMES=[string][Math]::Ceiling((1500 + $Hold * ($pitches + 1)) * [Math]::Max(1, $MaxFps / 60.0))
}
if ($Spawn) { $auditEnv.GE_SPAWNXYZ = $Spawn }
# The tank audit walks Bond onto the tank (GE_GOTO), then taps Use to board.
if ($Goto) { $auditEnv.GE_GOTO = $Goto }
# Capture the arc alone, without rounds or their smoke in the frame.
if ($NoFire) { $auditEnv.Remove('GE_AUTOFIRE') }
if ($Ads) { $auditEnv.GE_ARC_AUDIT_ADS = '1' }
if ($Drive) { $auditEnv.GE_DRIVE = '1' }
if ($Dual) { $auditEnv.GE_ARC_AUDIT_DUAL = '1' }
if ($FaceChr) { $auditEnv.GE_ARC_AUDIT_FACE = '1' }
if ($StrikeMarker) { $auditEnv.GE_ARC_AUDIT_STRIKE = '1' }
if ($NoArc) { $auditEnv.GE_ARC_AUDIT_NOARC = '1' }
if ($HiddenCross) { $auditEnv.GE_ARC_AUDIT_HIDDEN = '1' }
# A hand grenade's fuse (3.5 s) outlasts the usual two-second throw cadence,
# and a blast, while it lasts, sets off a grenade rolling into it; throw one
# every ten seconds. (GE_AUTOFIRE counts drawn frames.)
if ($Mode -eq 'grenade') { $auditEnv.GE_AUTOFIRE_PERIOD = [string][Math]::Ceiling(600 * [Math]::Max(1, $MaxFps / 60.0)) }
$saved = @{}
try {
    foreach ($key in $auditEnv.Keys) {
        $saved[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
        [Environment]::SetEnvironmentVariable($key, $auditEnv[$key], 'Process')
    }
    $p = Start-Process -FilePath $auditExe -WorkingDirectory $dir -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $dir 'stdout.log') -RedirectStandardError (Join-Path $dir 'stderr.log')
    $null = $p.Handle
} finally {
    foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key, $saved[$key], 'Process') }
}
Write-Output "Projectile arc audit: $dir"
try { $p.WaitForExit() }
finally {
    if ($p.HasExited -and (Test-Path -LiteralPath $auditExe)) { Remove-Item -LiteralPath $auditExe }
}
if ($p.ExitCode -ne 0) { throw "Game exited with code $($p.ExitCode)." }
$log = Get-Content -LiteralPath (Join-Path $dir 'stdout.log')
$log | Where-Object { $_ -like 'arcdbg:*' } | ForEach-Object { Write-Output $_ }
# Ground (xz) distances: the game lifts a blast a few units off the floor.
# -1 means that prediction had no impact.
$errors = @($log | Where-Object { $_ -match 'arcdbg: exploded .* shown_xz=([-0-9.]+) model_err=[-0-9.]+ model_xz=([-0-9.]+)' } | ForEach-Object {
    [pscustomobject]@{ Shown = [double]$Matches[1]; Model = [double]$Matches[2] } })
Write-Output ("Explosions logged: {0}" -f $errors.Count)
if ($errors.Count) {
    $sorted = @($errors | Sort-Object Shown)
    Write-Output ("Along the ground, drawn arc to blast: median {0:N1}, largest {1:N1} units; launch-state model: largest {2:N1} units" -f
        $sorted[[int][Math]::Floor($sorted.Count / 2)].Shown, ($errors | Measure-Object Shown -Maximum).Maximum,
        ($errors | Measure-Object Model -Maximum).Maximum)
}
