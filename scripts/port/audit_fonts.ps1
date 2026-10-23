# Compare real font rendering in isolated folders, leaving player data untouched.
param(
    [string]$Executable,
    [ValidateSet('Video','Difficulty','Hud')][string]$Mode = 'Video',
    [string]$Resolution = '1920x1080',
    [switch]$DefaultEnabled
)
$ErrorActionPreference = 'Stop'
$repoDir = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (!$Executable) { $Executable = Join-Path $repoDir 'build_win/ProjectMidas.exe' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
foreach ($enabled in @(0,1)) {
    $dir = Join-Path $repoDir ('build_win/font-audit-' + $Mode + '-' + $enabled + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
    New-Item -ItemType Directory -Path $dir | Out-Null
    $auditExe = [IO.Path]::GetFullPath((Join-Path $dir 'ProjectMidas.exe'))
    if (!$auditExe.StartsWith($repoDir+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Audit must stay in the workspace.' }
    Copy-Item -LiteralPath $Executable -Destination $auditExe
    Copy-Item -LiteralPath (Join-Path (Split-Path $Executable) 'SDL2.dll') -Destination $dir
    $ini = @('[display]','fullscreen=window','vsync=0','maxfps=120',
        'tickhz=60','msaa=0','filter_2d=0',"enhanced_fonts=$enabled",'mission_timer=1','weapon_hud_icons=0')
    if ($DefaultEnabled -and $enabled) { $ini = @($ini | Where-Object { $_ -notmatch '^enhanced_fonts=' }) }
    Set-Content -LiteralPath (Join-Path $dir 'midas.ini') -Value $ini
    $auditEnv = @{
        GE_ROM=(Join-Path $repoDir 'baserom.u.z64'); GE_WINDOW=$Resolution; GE_NOINPUT='1';
        GE_FONT_AUDIT='1'; GE_SEED='1'; GE_FRAMES='650'; GE_SHOT='500'
    }
    if ($Mode -eq 'Hud') {
        $auditEnv.GE_STAGE='dam'; $auditEnv.GE_STAGE_AFTER='1'; $auditEnv.GE_CHEATS='invuln,invis,guns'
        $auditEnv.GE_HUD_AUDIT='8'; $auditEnv.GE_FRAMES='2100'; $auditEnv.GE_SKIPINTRO='1'
    } else {
        $auditEnv.GE_MENU=$(if ($Mode -eq 'Video') {'video'} else {'jdifficulty'})
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
    $uploads = @(Select-String -LiteralPath (Join-Path $dir 'stderr.txt') -Pattern '^font: enhanced ')
    if ($enabled -and !$uploads.Count) { throw "No enhanced glyphs uploaded: $dir" }
    if (!$enabled -and $uploads.Count) { throw "Enhanced glyphs rendered while disabled: $dir" }
    [PSCustomObject]@{ Mode=$Mode; Enhanced=$enabled; Directory=$dir; Uploads=$uploads.Count; Capture=$captures[-1].FullName }
}
