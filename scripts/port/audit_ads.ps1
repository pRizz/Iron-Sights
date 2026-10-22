# Captures all supported ADS guns in an isolated game folder and builds a labelled contact sheet.
param(
    [string]$Executable,
    [string]$Rom,
    [ValidateSet('Wide', 'Classic')][string]$Aspect = 'Wide',
    [int]$Fov = 90
)
$ErrorActionPreference = 'Stop'
$repoDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (!$Executable) { $Executable = Join-Path $repoDir 'build_win\ProjectMidas.exe' }
if (!$Rom) { $Rom = Join-Path $repoDir 'baserom.u.z64' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$Rom = (Resolve-Path -LiteralPath $Rom).Path
$auditDir = Join-Path $repoDir ('build_win\ads-audit-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $auditDir | Out-Null
Copy-Item -LiteralPath $Executable -Destination (Join-Path $auditDir 'ProjectMidas.exe')
Copy-Item -LiteralPath (Join-Path (Split-Path $Executable) 'SDL2.dll') -Destination $auditDir
$resolution = if ($Aspect -eq 'Wide') { '960x540' } else { '800x600' }
$settings = @('[display]', 'fullscreen=window', 'vsync=0', 'maxfps=60', 'tickhz=60',
    "hfov_wide=$Fov", 'aim_mode=1', 'sight_on_screen=1', '[input]', 'mouse=1')
Set-Content -LiteralPath (Join-Path $auditDir 'midas.ini') -Value $settings
$auditEnv = @{
    GE_ROM=$Rom; GE_WINDOW=$resolution; GE_NOINPUT='1'; GE_STAGE='dam'; GE_STAGE_AFTER='1';
    GE_CHEATS='invuln,invis,guns'; GE_ADS_AUDIT='1'; GE_FRAMES='8000'; GE_SEED='1'
}
$previousEnv = @{}
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
if (!($log -match 'ads-audit: complete \(20 guns\)')) { throw 'ADS audit did not finish.' }
$captures = @()
$captureItem = $null
foreach ($line in $log) {
    if ($line -match 'ads-audit: capture item=(\d+)') { $captureItem = [int]$Matches[1] }
    if ($null -ne $captureItem -and $line -match 'gfx: wrote (shot_\d+\.bmp)') {
        $captures += [pscustomobject]@{ Item=$captureItem; File=$Matches[1] }
        $captureItem = $null
    }
}
if ($captures.Count -ne 20) { throw "Expected 20 screenshots, found $($captures.Count)." }
$names = @{
    4='PP7'; 5='Silenced PP7'; 6='DD44'; 7='Klobb'; 8='KF7 / AK47'; 9='ZMG';
    10='D5K'; 11='Silenced D5K'; 12='Phantom'; 13='AR33'; 14='RC-P90';
    15='Shotgun'; 16='Auto Shotgun'; 17='Sniper'; 18='Cougar Magnum';
    19='Golden Gun'; 20='Silver PP7'; 21='Gold PP7'; 22='Laser'; 24='Grenade Launcher'
}
Add-Type -AssemblyName System.Drawing
$height = if ($Aspect -eq 'Wide') { 225 } else { 300 }
$rowHeight = $height + 25
$sheet = [Drawing.Bitmap]::new(1600, $rowHeight * 5)
$graphics = [Drawing.Graphics]::FromImage($sheet)
$font = [Drawing.Font]::new('Arial', 12)
$pen = [Drawing.Pen]::new([Drawing.Color]::Magenta, 1)
try {
    $graphics.Clear([Drawing.Color]::FromArgb(25,25,25))
    for ($i=0; $i -lt $captures.Count; $i++) {
        $x = ($i % 4) * 400
        $y = [int][Math]::Floor($i / 4) * $rowHeight
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
Write-Output "Verified 20 captures. Review: $auditDir\contact-sheet.png"
