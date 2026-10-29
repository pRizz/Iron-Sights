# Captures all supported ADS guns in an isolated game folder and builds a labelled contact sheet.
param(
    [string]$Executable,
    [string]$Rom,
    [ValidateSet('Wide', 'Classic')][string]$Aspect = 'Wide',
    [int]$Fov = 90,
    [int]$Item = 0,
    [string]$Stage = 'dam',
    [switch]$Adzy,
    [switch]$Transitions,
    [switch]$Dual,
    [ValidateSet('Mission','Blue','Boiler','Jungle','Snow','Tuxedo','Connery')][string]$Wardrobe='Mission',
    [switch]$HandDetails,
    [switch]$StockHands,
    [switch]$WeaponFov,
    [ValidateSet(30,60,120,240)][int]$TickHz = 60,
    [ValidateSet('Butt','Tip')][string]$KnifeStyle = 'Butt',
    [switch]$KnifeThrows
)
$ErrorActionPreference = 'Stop'
if ($Dual -and (($Item -ne 0 -and $Item -notin @(4,5,6,18,19,20,21)) -or $Transitions)) {
    throw '-Dual checks native left wrists in hip fire (ADS requires one gun); select one pistol item without -Transitions.'
}
$repoDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (!$Executable) { $Executable = Join-Path $repoDir 'build_win\IronSights.exe' }
if (!$Rom) { $Rom = Join-Path $repoDir 'baserom.u.z64' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$Rom = (Resolve-Path -LiteralPath $Rom).Path
$auditDir = Join-Path $repoDir ('build_win\ads-audit-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
$auditExe = [IO.Path]::GetFullPath((Join-Path $auditDir 'IronSights.exe'))
if (!$auditExe.StartsWith($repoDir+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Audit executable must stay inside the workspace.' }
New-Item -ItemType Directory -Path $auditDir | Out-Null
Copy-Item -LiteralPath $Executable -Destination (Join-Path $auditDir 'IronSights.exe')
Copy-Item -LiteralPath (Join-Path (Split-Path $Executable) 'SDL2.dll') -Destination $auditDir
$resolution = if ($Aspect -eq 'Wide') { '960x540' } else { '800x600' }
$settings = @('[display]', 'fullscreen=window', 'vsync=0', "maxfps=$([Math]::Max(60,$TickHz))", "tickhz=$TickHz",
    "hfov_wide=$Fov", 'aim_mode=1', 'sight_on_screen=1', 'always_crosshair=1',
    'weapon_hud=0', 'always_health=0', '[input]', 'mouse=1')
Set-Content -LiteralPath (Join-Path $auditDir 'midas.ini') -Value $settings
if ($Item -eq 3) { Add-Content -LiteralPath (Join-Path $auditDir 'midas.ini') -Value @('[display]', ('knife_ads='+[int]($KnifeStyle -eq 'Tip'))) }
if ($Adzy) { Add-Content -LiteralPath (Join-Path $auditDir 'midas.ini') -Value @('[display]','adzy=1') }
if ($WeaponFov) { Add-Content -LiteralPath (Join-Path $auditDir 'midas.ini') -Value @('[display]','weapon_fov=1') }
# The intro harness stops automatically once live gameplay owns input.
$auditEnv = @{
    GE_ROM=$Rom; GE_WINDOW=$resolution; GE_NOINPUT='1'; GE_STAGE=$Stage; GE_STAGE_AFTER='1';
    GE_CHEATS='invuln,invis,guns'; GE_ADS_AUDIT='1';
    GE_SKIPINTRO='1'; GE_ARM_AUDIT='1';
    GE_FRAMES=[string][Math]::Ceiling(8000 * [Math]::Max(1, $TickHz / 60.0)); GE_SEED='1'
}
$previousEnv = @{}
if ($Item -ne 0) { $auditEnv.GE_ADS_AUDIT = [string]$Item }
if ($Transitions) { $auditEnv.GE_ADS_AUDIT_TRANSITIONS = '1' }
if ($KnifeThrows) { $auditEnv.GE_KNIFE_ADS_THROWS = '1' }
if ($Dual) { $auditEnv.GE_ADS_AUDIT_DUAL = '1' }
if ($Wardrobe -ne 'Mission') { $auditEnv.GE_ADS_AUDIT_WARDROBE=[string](@('blue','boiler','jungle','snow','tuxedo','connery').IndexOf($Wardrobe.ToLowerInvariant())) }
if ($StockHands) { $auditEnv.GE_ARM_AUDIT_STOCK='1' }
$expectedCount = if ($Item -eq 0) { if ($Dual) {7} else {21} } else { 1 }
try {
    foreach ($key in $auditEnv.Keys) {
        $previousEnv[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
        [Environment]::SetEnvironmentVariable($key, $auditEnv[$key], 'Process')
    }
    $auditProcess = Start-Process -FilePath (Join-Path $auditDir 'IronSights.exe') `
        -WorkingDirectory $auditDir -WindowStyle Hidden -PassThru
} finally {
    foreach ($key in $previousEnv.Keys) {
        [Environment]::SetEnvironmentVariable($key, $previousEnv[$key], 'Process')
    }
}

Write-Output "Auditing ADS: $auditDir"
try { $auditProcess.WaitForExit() }
finally {
    if ($auditProcess.HasExited -and (Test-Path -LiteralPath $auditExe)) { Remove-Item -LiteralPath $auditExe }
}
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
    3='Throwing Knife';
    4='PP7'; 5='Silenced PP7'; 6='DD44'; 7='Klobb'; 8='KF7 / AK47'; 9='ZMG';
    10='D5K'; 11='Silenced D5K'; 12='Phantom'; 13='AR33'; 14='RC-P90';
    15='Shotgun'; 16='Auto Shotgun'; 17='Sniper'; 18='Cougar Magnum';
    19='Golden Gun'; 20='Silver PP7'; 21='Gold PP7'; 22='Laser'; 24='Grenade Launcher'; 25='Rocket Launcher'
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
# Preserve full captures, plus enlarged long-gun sights with an unambiguous
# center reference. The magenta ticks stop short of the actual aiming point.
$detailCaptures = @($captures | Where-Object { $_.Item -in @(7,8,9,10,11,12,13,14,15,16,17,22,24,25) })
if ($detailCaptures.Count) {
    $columns = [Math]::Min(3, $detailCaptures.Count)
    $sheet = [Drawing.Bitmap]::new(540*$columns, 390*[int][Math]::Ceiling($detailCaptures.Count/$columns))
    $graphics = [Drawing.Graphics]::FromImage($sheet)
    $font = [Drawing.Font]::new('Arial', 12)
    $pen = [Drawing.Pen]::new([Drawing.Color]::Magenta, 1)
    try {
        $graphics.Clear([Drawing.Color]::FromArgb(25,25,25))
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
        for ($i=0; $i -lt $detailCaptures.Count; $i++) {
            $capture = $detailCaptures[$i]
            $x = ($i % $columns)*540; $y = [int][Math]::Floor($i/$columns)*390
            $screenshot = [Drawing.Image]::FromFile((Join-Path $auditDir $capture.File))
            try {
                $source = [Drawing.Rectangle]::new([int]($screenshot.Width/2)-90, [int]($screenshot.Height/2)-45, 180, 120)
                $target = [Drawing.Rectangle]::new($x, $y+30, 540, 360)
                $graphics.DrawImage($screenshot, $target, $source, [Drawing.GraphicsUnit]::Pixel)
            } finally { $screenshot.Dispose() }
            $graphics.DrawString($names[$capture.Item], $font, [Drawing.Brushes]::White, [single]($x+12), [single]($y+5))
            $graphics.DrawLine($pen, $x+225, $y+165, $x+255, $y+165)
            $graphics.DrawLine($pen, $x+285, $y+165, $x+315, $y+165)
            $graphics.DrawLine($pen, $x+270, $y+130, $x+270, $y+150)
        }
        $sheet.Save((Join-Path $auditDir 'sight-details.png'), [Drawing.Imaging.ImageFormat]::Png)
    } finally { $graphics.Dispose(); $sheet.Dispose(); $font.Dispose(); $pen.Dispose() }
    Write-Output "Sight details: $auditDir\sight-details.png"
}
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
if ($HandDetails) {
    foreach ($capture in $captures) {
        $frames=@($capture)
        if ($Transitions) {
            foreach ($direction in @(1,0)) {
                $frames+=@($transitionCaptures | Where-Object { $_.Item -eq $capture.Item -and $_.Aiming -eq $direction -and [double]$_.Blend -gt 0 -and [double]$_.Blend -lt 1 } |
                    Sort-Object { [Math]::Abs([double]$_.Blend-.5) } | Select-Object -First 1)
            }
        }
        $details=[Drawing.Bitmap]::new(672*$frames.Count,376)
        $g=[Drawing.Graphics]::FromImage($details); $f=[Drawing.Font]::new('Arial',12)
        try {
            $g.Clear([Drawing.Color]::FromArgb(25,25,25))
            for ($j=0;$j -lt $frames.Count;$j++) {
                $source=[Drawing.Image]::FromFile((Join-Path $auditDir $frames[$j].File))
                try {
                    $crop=[Drawing.Rectangle]::new([int]($source.Width*.15),[int]($source.Height*.35),[int]($source.Width*.70),[int]($source.Height*.65))
                    $g.DrawImage($source,[Drawing.Rectangle]::new($j*672,25,672,351),$crop,[Drawing.GraphicsUnit]::Pixel)
                } finally { $source.Dispose() }
                $label=if ($j -eq 0 -and $Dual) {'Dual wield'} elseif ($j -eq 0) {'ADS'} elseif ($j -eq 1) {'Raising'} else {'Lowering'}
                $g.DrawString(($names[$capture.Item]+' - '+$label),$f,[Drawing.Brushes]::White,[single]($j*672+8),[single]2)
            }
            $details.Save((Join-Path $auditDir ('hand-details-'+$capture.Item+'.png')),[Drawing.Imaging.ImageFormat]::Png)
        } finally { $g.Dispose(); $f.Dispose(); $details.Dispose() }
    }
    Write-Output "Hand detail crops: $auditDir\hand-details-*.png"
}
