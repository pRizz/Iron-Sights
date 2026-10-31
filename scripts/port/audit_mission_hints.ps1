param(
    [string]$Executable = '',
    [ValidateSet('960x540','800x600')][string]$Resolution = '960x540',
    [switch]$Remapped,
    [switch]$Unbound,
    [switch]$LegacyAds
)
$ErrorActionPreference = 'Stop'
$repoDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (!$Executable) { $Executable = Join-Path $repoDir 'build_win\IronSights.exe' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$dir = Join-Path $repoDir ('build_win\mission-hints-audit-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
$auditExe = [IO.Path]::GetFullPath((Join-Path $dir 'IronSights.exe'))
if (!$auditExe.StartsWith($repoDir+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Audit executable must stay inside the workspace.' }
New-Item -ItemType Directory -Path $dir | Out-Null
Copy-Item -LiteralPath $Executable -Destination $auditExe
Copy-Item -LiteralPath (Join-Path (Split-Path $Executable) 'SDL2.dll') -Destination $dir
$ini = @('[display]', 'fullscreen=window', 'vsync=0', 'maxfps=60', 'tickhz=60', 'msaa=0', 'mission_timer=1', '[input]', 'mouse=1')
if ($Remapped) { $ini += @('next_inventory=G,Q', 'r=Mouse X2') }
if ($Unbound) { $ini += @('next_inventory=', 'r=', 'aim_down_sights=') }
Set-Content -LiteralPath (Join-Path $dir 'midas.ini') -Value $ini
if ($LegacyAds) {
    $deviceDir = Join-Path $dir 'Data\Devices'
    New-Item -ItemType Directory -Path $deviceDir -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $deviceDir 'kbm.gedevice') -Value @('[device]', 'id=kbm', 'version=1', '[binds]', 'r=Mouse Right', 'aim_down_sights=Mouse Middle,Mouse X2')
}
Set-Content -LiteralPath (Join-Path $dir 'input.txt') -Value @('P 0 0 0 0',
    'P 80 8000 0 0', 'P 82 0 0 0', 'P 120 8000 0 0', 'P 122 0 0 0',
    'P 160 8000 0 0', 'P 162 0 0 0', 'P 1100 0 0 0')
$auditEnv = @{
    GE_ROM=(Join-Path $repoDir 'baserom.u.z64'); GE_WINDOW=$Resolution;
    GE_NOINPUT='1'; GE_STAGE='dam'; GE_DIFF='0'; GE_STAGE_AFTER='1';
    GE_CHEATS='invuln,invis'; GE_FRAMES='1000'; GE_SEED='1'; GE_SHOT='120';
    GE_NOINISAVE='1'; GE_INPUTPLAY=(Join-Path $dir 'input.txt')
    GE_CONCMD='get r;get aim_down_sights'
}
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
Write-Output "Mission hints audit: $dir"
try { $p.WaitForExit() }
finally {
    if ($p.HasExited -and (Test-Path -LiteralPath $auditExe)) { Remove-Item -LiteralPath $auditExe }
}
if ($p.ExitCode -ne 0) { throw "Game exited with code $($p.ExitCode)." }
$captures = @(Get-ChildItem -LiteralPath $dir -Filter 'shot_*.bmp')
if ($captures.Count -lt 8) { throw "Missing mission hint captures ($($captures.Count))." }
$captures | Select-Object -ExpandProperty FullName
