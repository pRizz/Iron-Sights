# Compare Original/HD in isolated processes; never touch the running player build.
param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [string]$Pack,
    [string]$Stage = 'dam',
    [switch]$AllowMissingMatches,
    [switch]$Menu,
    [switch]$Gameplay,
    [switch]$Reload
)
$ErrorActionPreference = 'Stop'
$repoDir = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$Executable = (Resolve-Path -LiteralPath $Executable).Path
if (!$Pack) { $Pack = Join-Path (Split-Path $Executable) 'Data/TexturePacks/hd.istex' }
$Pack = (Resolve-Path -LiteralPath $Pack).Path
foreach ($mode in @(0,1)) {
    $dir = Join-Path $repoDir ('build_win/hd-audit-' + $mode + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
    New-Item -ItemType Directory -Path $dir | Out-Null
    $auditExe = [IO.Path]::GetFullPath((Join-Path $dir 'IronSights.exe'))
    if (!$auditExe.StartsWith($repoDir+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Audit must stay inside the workspace.' }
    Copy-Item -LiteralPath $Executable -Destination $auditExe
    Copy-Item -LiteralPath (Join-Path (Split-Path $Executable) 'SDL2.dll') -Destination $dir
    New-Item -ItemType Directory -Path (Join-Path $dir 'Data/TexturePacks') -Force | Out-Null
    Copy-Item -LiteralPath $Pack -Destination (Join-Path $dir 'Data/TexturePacks/hd.istex')
    Set-Content -LiteralPath (Join-Path $dir 'midas.ini') -Value @('[display]','fullscreen=window','vsync=0',
        'maxfps=120','tickhz=60','render_thread=0','msaa=0','filtering=bilinear','weapon_hud_icons=0',"texture_pack=$mode")
    $auditEnv = @{
        GE_ROM=(Join-Path $repoDir 'baserom.u.z64'); GE_WINDOW='1280x720'; GE_NOINPUT='1';
        GE_HD_TRACE='1'; GE_HD_DUMP='1'; GE_SEED='1'; GE_FRAMES='650'; GE_SHOT='500'; GE_SKIPINTRO='1'
    }
    if ($Menu) { $auditEnv.GE_MENU='portopts' }
    else { $auditEnv.GE_STAGE=$Stage; $auditEnv.GE_STAGE_AFTER='1'; $auditEnv.GE_CHEATS='invuln,invis' }
    if ($Gameplay) {
        Set-Content -LiteralPath (Join-Path $dir 'input.txt') -Value @('P 0 0 0 0',
            'P 80 8000 0 0','P 82 0 0 0','P 120 8000 0 0','P 122 0 0 0',
            'P 160 8000 0 0','P 162 0 0 0','P 1800 0 0 0')
        $auditEnv.GE_INPUTPLAY = Join-Path $dir 'input.txt'
        $auditEnv.GE_FRAMES = '1650'; $auditEnv.GE_SHOT = '1500'
    }
    if ($Reload) { $auditEnv.GE_FRAMES = '2200'; $auditEnv.GE_SHOT = '1900' }
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
        if ($Reload -and $mode) {
            $trace = Join-Path $dir 'stderr.txt'
            $waitForTrace = {
                param([scriptblock]$condition)
                $deadline = [DateTime]::UtcNow.AddSeconds(15)
                while (!(& $condition)) {
                    if ($process.HasExited -or [DateTime]::UtcNow -gt $deadline) { throw "Reload transition did not finish: $dir" }
                    Start-Sleep -Milliseconds 100
                }
            }
            & $waitForTrace { (Get-Content -LiteralPath $trace -Raw) -match 'hd: match ' }
            $activePack = Join-Path $dir 'Data/TexturePacks/hd.istex'
            $inactivePack = Join-Path $dir 'Data/TexturePacks/disabled.istex'
            # Only the audit's copied pack moves; Windows must permit replacing
            # the snapshot currently held by the render thread.
            Move-Item -LiteralPath $activePack -Destination $inactivePack
            & $waitForTrace { (Get-Content -LiteralPath $trace -Raw) -match 'not installed; original textures retained' }
            Move-Item -LiteralPath $inactivePack -Destination $activePack
            & $waitForTrace { @(Select-String -LiteralPath $trace -Pattern '^hd: loaded ').Count -ge 2 }
        }
        if (!$process.WaitForExit(60000)) { $process.Kill(); $process.WaitForExit(); throw "Audit timed out: $dir" }
        if ($process.ExitCode -ne 0) { throw "Audit exited $($process.ExitCode): $dir" }
    } finally {
        foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key,$saved[$key],'Process') }
        if ($process -and !$process.HasExited) { $process.Kill(); $process.WaitForExit() }
        if ((!$process -or $process.HasExited) -and (Test-Path -LiteralPath $auditExe)) { Remove-Item -LiteralPath $auditExe }
    }
    $capture = @(Get-ChildItem -LiteralPath $dir -Filter 'shot_*.bmp')
    if (!$capture.Count) { throw "Missing capture: $dir" }
    $matches = @(Select-String -LiteralPath (Join-Path $dir 'stderr.txt') -Pattern '^hd: match ')
    if (!$Menu -and $mode -and !$matches.Count -and !$AllowMissingMatches) { throw "No HD matches: $dir" }
    if ($Reload -and $mode -and @(Select-String -LiteralPath (Join-Path $dir 'stderr.txt') -Pattern '^hd: loaded ').Count -lt 2) { throw 'Pack did not reload' }
    [PSCustomObject]@{ Stage=$Stage; Mode=$mode; Directory=$dir; Matches=$matches.Count; Capture=$capture[-1].FullName }
}
