# Capture real text rendering with point-sampled and smoothed glyphs (filter_2d
# 0 and 1) in isolated folders, leaving player data untouched.
param(
    [string]$Executable,
    [ValidateSet('Video','Difficulty','Hud','Credits')][string]$Mode = 'Video',
    [string]$Resolution = '1920x1080',
    # Fonts = Iron Sights (font_style=2): Bank Gothic text from the open title font (FORK-051).
    [switch]$SharpFonts,
    # Fonts = Enhanced (font_style=1) with this folder of Enhanced font files copied into
    # Data/FontPacks/gepd (FORK-052; enhanced_fonts_pack.py install makes one).
    [string]$FontPack = ''
)
$ErrorActionPreference = 'Stop'
$repoDir = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (!$Executable) { $Executable = Join-Path $repoDir 'build_win/IronSights.exe' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
foreach ($filter in @(0,1)) {
    $dir = Join-Path $repoDir ('build_win/font-audit-' + $Mode + '-' + $filter + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
    New-Item -ItemType Directory -Path $dir | Out-Null
    $auditExe = [IO.Path]::GetFullPath((Join-Path $dir 'IronSights.exe'))
    if (!$auditExe.StartsWith($repoDir+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Audit must stay in the workspace.' }
    Copy-Item -LiteralPath $Executable -Destination $auditExe
    Copy-Item -LiteralPath (Join-Path (Split-Path $Executable) 'SDL2.dll') -Destination $dir
    $ini = @('[display]','fullscreen=window','vsync=0','maxfps=120',
        'tickhz=60','render_thread=0','msaa=0',"filter_2d=$filter","font_style=$(if ($SharpFonts) { 2 } elseif ($FontPack) { 1 } else { 0 })",'mission_timer=1','weapon_hud_icons=0')
    Set-Content -LiteralPath (Join-Path $dir 'midas.ini') -Value $ini
    if ($FontPack) {
        $packDir = Join-Path $dir 'Data/FontPacks/gepd'
        New-Item -ItemType Directory -Path $packDir | Out-Null
        Copy-Item -Path (Join-Path (Resolve-Path -LiteralPath $FontPack).Path '*') -Destination $packDir
    }
    $auditEnv = @{
        GE_ROM=(Join-Path $repoDir 'baserom.u.z64'); GE_WINDOW=$Resolution; GE_NOINPUT='1';
        GE_SEED='1'; GE_FRAMES='650'; GE_SHOT='500'
    }
    if ($Mode -eq 'Hud') {
        $auditEnv.GE_STAGE='dam'; $auditEnv.GE_STAGE_AFTER='1'; $auditEnv.GE_CHEATS='invuln,invis,guns'
        $auditEnv.GE_HUD_AUDIT='8'; $auditEnv.GE_FRAMES='2100'; $auditEnv.GE_SKIPINTRO='1'
    } else {
        $auditEnv.GE_MENU=$(if ($Mode -eq 'Video') {'video'} elseif ($Mode -eq 'Credits') {'credits'} else {'jdifficulty'})
        if ($Mode -eq 'Difficulty') {
            $auditEnv.GE_CURSOR='200,216'; $auditEnv.GE_MISSION_CHEAT_AUDIT='0,0,1'
        }
    }
    $saved = @{}
    $process = $null
    try {
        foreach ($key in $auditEnv.Keys) {
            $saved[$key] = [Environment]::GetEnvironmentVariable($key,'Process')
            [Environment]::SetEnvironmentVariable($key,$auditEnv[$key],'Process')
        }
        $process = Start-Process -FilePath $auditExe -WorkingDirectory $dir -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput (Join-Path $dir 'stdout.txt') -RedirectStandardError (Join-Path $dir 'stderr.txt')
        $null = $process.Handle
        if (!$process.WaitForExit(60000)) { $process.Kill(); $process.WaitForExit(); throw "Audit timed out: $dir" }
        if ($process.ExitCode -ne 0) { throw "Audit exited $($process.ExitCode): $dir" }
    } finally {
        foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key,$saved[$key],'Process') }
        if ($process -and $process.HasExited -and (Test-Path -LiteralPath $auditExe)) { Remove-Item -LiteralPath $auditExe }
    }
    $captures = @(Get-ChildItem -LiteralPath $dir -Filter 'shot_*.bmp')
    if (!$captures.Count) { throw "Missing capture: $dir" }
    if ($Mode -eq 'Hud' -and $captures.Count -lt 4) { throw "HUD audit did not reach gameplay: $dir" }
    [PSCustomObject]@{ Mode=$Mode; Filter2D=$filter; Directory=$dir; Capture=$captures[-1].FullName }
}
