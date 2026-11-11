# Noclip (FORK-072): press the Noclip control at scripted polls, optionally
# steer Bond at a point (GE_GOTO) from a chosen spawn and pitch, and log the
# flight (GE_NOCLIPDBG). Runs an isolated copy with its own settings; never
# writes the user's.
param(
    [string]$Executable,
    [string]$Stage = 'dam',
    # Input polls at which the Noclip control is pressed (on, off, ...).
    [string]$Presses = '1300,2300',
    # "x,y,z[,heading_deg[,pitch_deg]]"; positive pitch looks up.
    [string]$Spawn,
    # "x,y,z": walk towards this point (heading only).
    [string]$Goto,
    [int]$Frames = 2700,
    # Hold the view at this pitch while flying (positive looks up).
    [string]$Pitch,
    # Extra GE_JOYPORT_AUTO presses (one poll each), e.g. 'noclip_up:1400,walk:1400'.
    [string]$Auto,
    # Noclip View Distance (1 Normal, 4 Far, 10 Maximum) and the frame cap (0 = uncapped).
    [int]$View = 4,
    [int]$MaxFps = 60,
    # Leave Allow Noclip off, to check a press only shows the hint.
    [switch]$NotAllowed,
    [string]$Resolution = '960x540'
)
$ErrorActionPreference = 'Stop'
$repoDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (!$Executable) { $Executable = Join-Path $repoDir 'build_win\IronSights.exe' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$rom = Join-Path $repoDir 'baserom.u.z64'
if (!(Test-Path -LiteralPath $rom)) { $rom = Join-Path (Split-Path (git -C $repoDir rev-parse --path-format=absolute --git-common-dir)) 'baserom.u.z64' }
$dir = Join-Path $repoDir ('build_win\noclip-audit-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
$auditExe = [IO.Path]::GetFullPath((Join-Path $dir 'IronSights.exe'))
if (!$auditExe.StartsWith($repoDir+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Audit executable must stay inside the workspace.' }
New-Item -ItemType Directory -Path $dir | Out-Null
Copy-Item -LiteralPath $Executable -Destination $auditExe
Copy-Item -LiteralPath (Join-Path (Split-Path $Executable) 'SDL2.dll') -Destination $dir
Set-Content -LiteralPath (Join-Path $dir 'midas.ini') -Value @('[display]', 'fullscreen=window', 'vsync=0', "maxfps=$MaxFps",
    'tickhz=60', 'render_thread=0', 'msaa=0', 'music_volume=0', 'sfx_volume=0', "allow_noclip=$(if ($NotAllowed) { 0 } else { 1 })", "noclip_view=$View")
$autoList = (@($Presses.Split(',')) | ForEach-Object { "noclip:$($_.Trim())" }) -join ','
if ($Auto) { $autoList += ',' + $Auto }
$auditEnv = @{
    GE_ROM=$rom; GE_WINDOW=$Resolution; GE_NOINPUT='1'; GE_SEED='1';
    GE_STAGE=$Stage; GE_STAGE_AFTER='1'; GE_SKIPINTRO='1'; GE_CHEATS='invuln,invis';
    GE_JOYPORT_AUTO=$autoList; GE_NOCLIPDBG='1'; GE_FRAMES=[string]$Frames
}
if ($Spawn) { $auditEnv.GE_SPAWNXYZ = $Spawn }
if ($Goto) { $auditEnv.GE_GOTO = $Goto }
if ($Pitch) { $auditEnv.GE_NOCLIPDBG_PITCH = $Pitch }
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
Write-Output "Noclip audit: $dir"
try { $p.WaitForExit() }
finally {
    if ($p.HasExited -and (Test-Path -LiteralPath $auditExe)) { Remove-Item -LiteralPath $auditExe }
}
if ($p.ExitCode -ne 0) { throw "Game exited with code $($p.ExitCode)." }
Get-Content -LiteralPath (Join-Path $dir 'stdout.log') | Where-Object { $_ -like 'noclipdbg:*' } | ForEach-Object { Write-Output $_ }
Get-Content -LiteralPath (Join-Path $dir 'stderr.log') | Where-Object { $_ -match 'ge: (goto|OOB)' } | Select-Object -First 40 | ForEach-Object { Write-Output $_ }
