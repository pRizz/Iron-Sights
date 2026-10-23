# Captures all supported ADS guns in an isolated game folder and builds a labelled contact sheet.
param(
    [string]$Executable,
    [string]$Rom,
    [ValidateSet('Wide', 'Classic')][string]$Aspect = 'Wide',
    [int]$Fov = 90,
    [int]$Item = 0,
    [switch]$Transitions,
    [ValidateSet(30,60,120,240)][int]$TickHz = 60
)
$ErrorActionPreference = 'Stop'
$repoDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (!$Executable) { $Executable = Join-Path $repoDir 'build_win\ProjectMidas.exe' }
if (!$Rom) { $Rom = Join-Path $repoDir 'baserom.u.z64' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$Rom = (Resolve-Path -LiteralPath $Rom).Path
$auditDir = Join-Path $repoDir ('build_win\ads-audit-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $auditDir | Out-Null
Copy-Item -LiteralPath $Executable -Destination (Join-Path $auditDir 'ProjectMidas.exe')
Copy-Item -LiteralPath (Join-Path (Split-Path $Executable) 'SDL2.dll') -Destination $auditDir
$resolution = if ($Aspect -eq 'Wide') { '960x540' } else { '800x600' }
$settings = @('[display]', 'fullscreen=window', 'vsync=0', "maxfps=$([Math]::Max(60,$TickHz))", "tickhz=$TickHz",
    "hfov_wide=$Fov", 'aim_mode=1', 'sight_on_screen=1', 'always_crosshair=1',
    'weapon_hud=0', 'always_health=0', '[input]', 'mouse=1')
Set-Content -LiteralPath (Join-Path $auditDir 'midas.ini') -Value $settings
# Skip the intro before the gun settles; no input is replayed during aiming.
$inputFile = Join-Path $auditDir 'input.txt'
Set-Content -LiteralPath $inputFile -Value @('P 0 0 0 0',
    'P 80 8000 0 0', 'P 82 0 0 0', 'P 120 8000 0 0', 'P 122 0 0 0',
    'P 160 8000 0 0', 'P 162 0 0 0', 'P 2200 0 0 0')
$auditEnv = @{
    GE_ROM=$Rom; GE_WINDOW=$resolution; GE_NOINPUT='1'; GE_STAGE='dam'; GE_STAGE_AFTER='1';
    GE_CHEATS='invuln,invis,guns'; GE_ADS_AUDIT='1';
    GE_INPUTPLAY=$inputFile;
    GE_FRAMES=[string][Math]::Ceiling(8000 * [Math]::Max(1, $TickHz / 60.0)); GE_SEED='1'
}
$previousEnv = @{}
if ($Item -ne 0) { $auditEnv.GE_ADS_AUDIT = [string]$Item }
if ($Transitions) { $auditEnv.GE_ADS_AUDIT_TRANSITIONS = '1' }
$expectedCount = if ($Item -eq 0) { 20 } else { 1 }
try {
    foreach ($key in $auditEnv.Keys) {
        $previousEnv[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
        [Environment]::SetEnvironmentVariable($key, $auditEnv[$key], 'Process')
    }
    $auditProcess = Start-Process -FilePath (Join-Path $auditDir 'ProjectMidas.exe') `
        -WorkingDirectory $auditDir -WindowStyle Hidden -PassThru
} finally {
    foreach ($key in $previousEnv.Keys) {
        [Environment]::SetEnvironmentVariable($key, $previousEnv[$key], 'Process')
    }
}
Write-Output "Auditing ADS: $auditDir"
$auditProcess.WaitForExit()
if ($auditProcess.ExitCode -ne 0) { throw "Game exited with code $($auditProcess.ExitCode)." }
$log = Get-Content -LiteralPath (Join-Path $auditDir 'Data\Logs\ge_direct.log')
if (!($log -match "ads-audit: complete \($expectedCount guns\)")) { throw 'ADS audit did not finish.' }
$captures = @()
$transitionCaptures = @()
$transition = $null
$captureItem = $null
foreach ($line in $log) {
    if ($line -match 'ads-audit: transition item=(\d+) aiming=(\d+) blend=([\d.]+)') {
        $transition = [pscustomobject]@{ Item=[int]$Matches[1]; Aiming=[int]$Matches[2]; Blend=$Matches[3]; File='' }
    }
    if ($null -ne $transition -and $line -match 'gfx: wrote (shot_\d+\.bmp)') {
        $transition.File = $Matches[1]
        $transitionCaptures += $transition
        $transition = $null
    }
    if ($line -match 'ads-audit: capture item=(\d+)') { $captureItem = [int]$Matches[1] }
    if ($null -ne $captureItem -and $line -match 'gfx: wrote (shot_\d+\.bmp)') {
        $captures += [pscustomobject]@{ Item=$captureItem; File=$Matches[1] }
        $captureItem = $null
    }
}
if ($captures.Count -ne $expectedCount) { throw "Expected $expectedCount screenshots, found $($captures.Count)." }
$names = @{
    4='PP7'; 5='Silenced PP7'; 6='DD44'; 7='Klobb'; 8='KF7 / AK47'; 9='ZMG';
    10='D5K'; 11='Silenced D5K'; 12='Phantom'; 13='AR33'; 14='RC-P90';
    15='Shotgun'; 16='Auto Shotgun'; 17='Sniper'; 18='Cougar Magnum';
    19='Golden Gun'; 20='Silver PP7'; 21='Gold PP7'; 22='Laser'; 24='Grenade Launcher'
}
Add-Type -AssemblyName System.Drawing
$height = if ($Aspect -eq 'Wide') { 225 } else { 300 }
$rowHeight = $height + 25
$columns = [Math]::Min(4, $expectedCount)
$sheet = [Drawing.Bitmap]::new(400 * $columns, $rowHeight * [int][Math]::Ceiling($expectedCount / $columns))
$graphics = [Drawing.Graphics]::FromImage($sheet)
$font = [Drawing.Font]::new('Arial', 12)
$pen = [Drawing.Pen]::new([Drawing.Color]::Magenta, 1)
try {
    $graphics.Clear([Drawing.Color]::FromArgb(25,25,25))
    for ($i=0; $i -lt $captures.Count; $i++) {
        $x = ($i % $columns) * 400
        $y = [int][Math]::Floor($i / $columns) * $rowHeight
        $screenshot = [Drawing.Image]::FromFile((Join-Path $auditDir $captures[$i].File))
        try { $graphics.DrawImage($screenshot, $x, ($y+25), 400, $height) }
        finally { $screenshot.Dispose() }
        $graphics.DrawString($names[$captures[$i].Item], $font, [Drawing.Brushes]::White,
            [single]($x+5), [single]($y+3))
        $cx = $x + 200; $cy = $y + 25 + $height / 2
        $graphics.DrawLine($pen, [single]($cx-10), [single]$cy, [single]($cx-5), [single]$cy)
        $graphics.DrawLine($pen, [single]($cx+5), [single]$cy, [single]($cx+10), [single]$cy)
    }
    $sheet.Save((Join-Path $auditDir 'contact-sheet.png'), [Drawing.Imaging.ImageFormat]::Png)
} finally {
    $graphics.Dispose(); $sheet.Dispose(); $font.Dispose(); $pen.Dispose()
}
$captures | Export-Csv -LiteralPath (Join-Path $auditDir 'captures.csv') -NoTypeInformation
Write-Output "Verified $expectedCount captures. Review: $auditDir\contact-sheet.png"
if ($Transitions) {
    $transitionCaptures | Export-Csv -LiteralPath (Join-Path $auditDir 'transitions.csv') -NoTypeInformation
    if ($transitionCaptures.Count -lt 2) { throw 'Missing transition captures.' }
    # Keep the all-gun overview readable; the CSV retains every captured phase.
    $sheetCaptures = if ($Item -eq 0) {
        foreach ($group in ($transitionCaptures | Group-Object Item)) {
            foreach ($direction in @(1,0)) {
                $group.Group | Where-Object { $_.Aiming -eq $direction -and [double]$_.Blend -gt 0 -and [double]$_.Blend -lt 1 } |
                    Sort-Object { [Math]::Abs([double]$_.Blend - 0.5) } | Select-Object -First 1
            }
        }
    } else { $transitionCaptures }
    $sheet = [Drawing.Bitmap]::new(1600, $rowHeight * [int][Math]::Ceiling($sheetCaptures.Count / 4))
    $graphics = [Drawing.Graphics]::FromImage($sheet)
    $font = [Drawing.Font]::new('Arial', 12)
    try {
        $graphics.Clear([Drawing.Color]::FromArgb(25,25,25))
        for ($i=0; $i -lt $sheetCaptures.Count; $i++) {
            $capture = $sheetCaptures[$i]
            $x = ($i % 4) * 400; $y = [int][Math]::Floor($i / 4) * $rowHeight
            $screenshot = [Drawing.Image]::FromFile((Join-Path $auditDir $capture.File))
            try { $graphics.DrawImage($screenshot, $x, ($y+25), 400, $height) }
            finally { $screenshot.Dispose() }
            $direction = if ($capture.Aiming) { 'Raise' } else { 'Lower' }
            $label = "$($names[$capture.Item]): $direction $($capture.Blend)"
            $graphics.DrawString($label, $font, [Drawing.Brushes]::White, [single]($x+5), [single]($y+3))
        }
        $sheet.Save((Join-Path $auditDir 'transitions.png'), [Drawing.Imaging.ImageFormat]::Png)
    } finally { $graphics.Dispose(); $sheet.Dispose(); $font.Dispose() }
    Write-Output "Verified $($transitionCaptures.Count) transition captures. Review: $auditDir\transitions.png"
}
