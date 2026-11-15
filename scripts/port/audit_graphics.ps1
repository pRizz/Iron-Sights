# Graphics (FORK-082): capture a stage with chosen Graphics settings, from an
# isolated copy with its own midas.ini (never the user's), and keep the
# screenshots and logs. Several -Set variants of one spot compare stock, Tone
# Mapping and Baked Lighting; -Auto presses the bindable toggles mid-run.
#   scripts/port/audit_graphics.ps1 -Set 'tone_mapping=1','baked_lighting=1'
#   scripts/port/audit_graphics.ps1 -Auto 'toggle_baked_lighting:900' -Shot 300
# The copied executable is deleted when the run exits; the folder is printed.
param(
    [string]$Executable,
    [string]$Stage = 'dam',
    [int]$Frames = 1500,
    # GE_SHOT: a screenshot every this many frames.
    [int]$Shot = 700,
    # midas.ini [display] overrides, e.g. 'tone_mapping=1','vibrance=50'.
    [string[]]$Set = @(),
    # GE_JOYPORT_AUTO presses, e.g. 'toggle_tone_mapping:600,graphics_compare:600'.
    [string]$Auto,
    # GE_SPAWNXYZ "x,y,z[,heading_deg[,pitch_deg]]".
    [string]$Spawn,
    # Extra environment, e.g. 'GE_BAKE_RAYS=128'.
    [string[]]$Env = @(),
    [string]$Resolution = '960x540',
    [string]$Label = 'graphics',
    # Show the window (HDR Output presents only to a visible one); -ScreenAt
    # grabs the desktop at these seconds into the run (screen_N.png), since
    # GE_SHOT reads GL's buffer and cannot see what DXGI presented.
    [switch]$Visible,
    [int[]]$ScreenAt = @()
)
$ErrorActionPreference = 'Stop'
$repoDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (!$Executable) { $Executable = Join-Path $repoDir 'build_win\IronSights.exe' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$rom = Join-Path $repoDir 'baserom.u.z64'
if (!(Test-Path -LiteralPath $rom)) { $rom = Join-Path (Split-Path (git -C $repoDir rev-parse --path-format=absolute --git-common-dir)) 'baserom.u.z64' }
$dir = Join-Path $repoDir ('build_win\graphics-audit-' + $Label + '-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$auditExe = [IO.Path]::GetFullPath((Join-Path $dir 'IronSights.exe'))
if (!$auditExe.StartsWith($repoDir + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Audit executable must stay inside the workspace.' }
New-Item -ItemType Directory -Path $dir | Out-Null
Copy-Item -LiteralPath $Executable -Destination $auditExe
Copy-Item -LiteralPath (Join-Path (Split-Path $Executable) 'SDL2.dll') -Destination $dir
Set-Content -LiteralPath (Join-Path $dir 'midas.ini') -Value (@('[display]', 'fullscreen=window', 'vsync=0', 'maxfps=60',
    'tickhz=60', 'render_thread=0', 'msaa=0', 'music_volume=0', 'sfx_volume=0') + $Set)
$auditEnv = @{ SDL_AUDIODRIVER = 'dummy'; GE_ROM = $rom; GE_WINDOW = $Resolution; GE_NOINPUT = '1'; GE_SEED = '1';
    GE_STAGE = $Stage; GE_STAGE_AFTER = '1'; GE_SKIPINTRO = '1'; GE_CHEATS = 'invuln,invis';
    GE_SHOT = [string]$Shot; GE_FRAMES = [string]$Frames }
if ($Auto) { $auditEnv.GE_JOYPORT_AUTO = $Auto }
if ($Spawn) { $auditEnv.GE_SPAWNXYZ = $Spawn }
foreach ($e in $Env) { $k, $v = $e.Split('=', 2); $auditEnv[$k] = $v }
$saved = @{}
try {
    foreach ($key in $auditEnv.Keys) {
        $saved[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
        [Environment]::SetEnvironmentVariable($key, $auditEnv[$key], 'Process')
    }
    $style = if ($Visible) { 'Normal' } else { 'Hidden' }
    $p = Start-Process -FilePath $auditExe -WorkingDirectory $dir -WindowStyle $style -PassThru `
        -RedirectStandardOutput (Join-Path $dir 'stdout.log') -RedirectStandardError (Join-Path $dir 'stderr.log')
    if ($ScreenAt.Count -gt 0) {
        Add-Type -AssemblyName System.Drawing, System.Windows.Forms
        $t0 = Get-Date
        foreach ($sec in ($ScreenAt | Sort-Object)) {
            while (((Get-Date) - $t0).TotalSeconds -lt $sec -and !$p.HasExited) { Start-Sleep -Milliseconds 100 }
            if ($p.HasExited) { break }
            $b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
            $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
            $g = [System.Drawing.Graphics]::FromImage($bmp)
            $g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
            $bmp.Save((Join-Path $dir "screen_$sec.png"), [System.Drawing.Imaging.ImageFormat]::Png)
            $g.Dispose(); $bmp.Dispose()
        }
    }
    if (!$p.WaitForExit(600000)) { $p.Kill(); throw "audit timed out: $dir" }
} finally {
    foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key, $saved[$key], 'Process') }
    Start-Sleep -Milliseconds 300
    Remove-Item -LiteralPath $auditExe -Force -ErrorAction SilentlyContinue
}
Write-Output $dir
