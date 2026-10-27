# Isolated screenshots of gameplay sights, with MSAA disabled.
param(
    [string]$Executable,
    [ValidateSet('Hip','RedReticle','Sniper','Menu','HudMenu','Watch')][string]$Mode = 'RedReticle',
    [string]$Resolution = '1920x1080',
    [string]$Color = 'default',
    [ValidateSet(1,2,4)][int]$Players = 1,
    [switch]$Edge,
    [ValidateRange(10,200)][int]$CrosshairSize = 100,
    [switch]$HighHudScale
)
$ErrorActionPreference = 'Stop'
$repoDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (!$Executable) { $Executable = Join-Path $repoDir 'build_win\IronSights.exe' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$dir = Join-Path $repoDir ('build_win\sight-audit-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $dir | Out-Null
$auditExe = [IO.Path]::GetFullPath((Join-Path $dir 'IronSights.exe'))
if (!$auditExe.StartsWith($repoDir+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Audit must stay inside the workspace.' }
Copy-Item -LiteralPath $Executable -Destination $auditExe
Copy-Item -LiteralPath (Join-Path (Split-Path $Executable) 'SDL2.dll') -Destination $dir
$aimMode = if ($Mode -eq 'RedReticle') {'0'} else {'1'}
Set-Content -LiteralPath (Join-Path $dir 'midas.ini') -Value @('[display]', 'fullscreen=window', 'vsync=0',
    'maxfps=60', 'tickhz=60', 'render_thread=0', 'watch_static=0', 'msaa=0', "aim_mode=$aimMode", 'aim_control=0', 'always_crosshair=1',
    "always_crosshair_size=$CrosshairSize", "hud_scale=$([int]$HighHudScale.IsPresent)",
    'sight_on_screen=1', 'mp_always_crosshair=1', "crosshair_color=$Color", 'hit_markers=0', 'damage_indicators=0',
    'objective_notifications=0', 'weapon_hud=0', '[input]', 'mouse=1')
$pad = if ($Mode -eq 'RedReticle') {'10'} else {'0'}
$actions = if ($Mode -eq 'Sniper') {'200000'} else {'0'}
$inputFile = Join-Path $dir 'input.txt'
$records = @('P 0 0 0 0', 'P 80 8000 0 0', 'P 82 0 0 0', 'P 120 8000 0 0', 'P 122 0 0 0',
    'P 160 8000 0 0', 'P 162 0 0 0', "P 220 $pad 0 0", 'P 1500 0 0 0',
    'A 0 0 0', "A 220 $actions 0", 'A 1500 0 0')
if ($Edge) { for ($t=235; $t -le 260; $t++) { $records += "M $t 1 18 0 3 0" } }
if ($Mode -eq 'Watch') {
    $records = @('P 0 0 0 0', 'P 80 8000 0 0', 'P 82 0 0 0', 'P 120 8000 0 0', 'P 122 0 0 0',
        'P 160 8000 0 0', 'P 162 0 0 0', 'P 500 1000 0 0', 'P 502 0 0 0', 'P 1500 0 0 0')
}
Set-Content -LiteralPath $inputFile -Value $records
$testEnv = @{
    GE_ROM=(Join-Path $repoDir 'baserom.u.z64'); GE_WINDOW=$Resolution; GE_NOINPUT='1';
    GE_STAGE='dam'; GE_STAGE_AFTER='1'; GE_CHEATS='invuln,invis,guns'; GE_INPUTPLAY=$inputFile;
    GE_HUD_AUDIT=$(if ($Mode -eq 'Sniper') {'17'} else {'8'}); GE_FRAMES='1000'; GE_SEED='1'
}
if ($Mode -in @('Menu','HudMenu')) {
    $testEnv.Remove('GE_STAGE'); $testEnv.Remove('GE_INPUTPLAY'); $testEnv.Remove('GE_HUD_AUDIT');
    $testEnv.GE_MENU=$(if ($Mode -eq 'HudMenu') {'hud'} else {'general'}); $testEnv.GE_CURSOR='240,160'; $testEnv.GE_SHOT='500'; $testEnv.GE_FRAMES='650'
}
if ($Mode -eq 'Watch') {
    $testEnv.Remove('GE_HUD_AUDIT')
    $testEnv.GE_WATCHPAGE='3,3,1,0'; $testEnv.GE_SHOT='900'
}
if ($Players -gt 1) { $testEnv.Remove('GE_STAGE'); $testEnv.GE_MP='temple'; $testEnv.GE_MPPLAYERS=[string]$Players }
$saved = @{}
try {
    foreach ($key in $testEnv.Keys) {
        $saved[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
        [Environment]::SetEnvironmentVariable($key, $testEnv[$key], 'Process')
    }
    $p = Start-Process -FilePath $auditExe -WorkingDirectory $dir -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $dir 'stdout.log') -RedirectStandardError (Join-Path $dir 'stderr.log')
    $null = $p.Handle
} catch {
    if (Test-Path -LiteralPath $auditExe) { Remove-Item -LiteralPath $auditExe }
    throw
} finally {
    foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key, $saved[$key], 'Process') }
}
Write-Output "Sight audit ($Mode): $dir"
try {
    if (!$p.WaitForExit(60000)) { $p.Kill(); $p.WaitForExit(); throw "Sight audit timed out: $dir" }
} finally {
    if ($p.HasExited -and (Test-Path -LiteralPath $auditExe)) { Remove-Item -LiteralPath $auditExe }
}
if ($p.ExitCode -ne 0) { throw "Game exited with code $($p.ExitCode)." }
$shots = @(Get-ChildItem -LiteralPath $dir -Filter 'shot_*.bmp')
if ($shots.Count -lt $(if ($Mode -in @('Menu','HudMenu','Watch')) {1} else {3})) { throw 'Missing sight screenshots.' }
$shots | Select-Object -ExpandProperty FullName
