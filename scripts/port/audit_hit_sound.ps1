# Record scripted Dam hits with Hit Sounds off and on, then measure them; never writes the user's settings.
param(
    [string]$Executable,
    [string]$Metres = '1,5,10,20,30,40,50,58'
)
$ErrorActionPreference = 'Stop'
$repoDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (!$Executable) { $Executable = Join-Path $repoDir 'build_win\IronSights.exe' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$dir = Join-Path $repoDir ('build_win\hitsound-audit-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
$auditExe = [IO.Path]::GetFullPath((Join-Path $dir 'IronSights.exe'))
if (!$auditExe.StartsWith($repoDir+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Audit executable must stay inside the workspace.' }
New-Item -ItemType Directory -Path $dir | Out-Null
Copy-Item -LiteralPath $Executable -Destination $auditExe
Copy-Item -LiteralPath (Join-Path (Split-Path $Executable) 'SDL2.dll') -Destination $dir
# Music off leaves the hits over Dam's quiet ambience.
Set-Content -LiteralPath (Join-Path $dir 'midas.ini') -Value @('[display]', 'fullscreen=window', 'vsync=0', 'maxfps=60',
    'tickhz=60', 'render_thread=0', 'msaa=0', 'music_volume=0', 'sfx_volume=100', 'hit_sounds=1')
$hits = @($Metres.Split(',')).Count * 2
$auditEnv = @{
    GE_ROM=(Join-Path $repoDir 'baserom.u.z64'); GE_WINDOW='640x480'; GE_NOINPUT='1'; GE_SEED='1';
    GE_STAGE='dam'; GE_STAGE_AFTER='1'; GE_SKIPINTRO='1'; GE_CHEATS='invuln,invis';
    GE_HITSOUND_PROBE=$Metres; GE_AUDIO_CAPTURE=(Join-Path $dir 'capture.wav');
    # Stage load, the 240-tick settle, one hit a second, then a second of tail.
    GE_FRAMES=[string](900 + 60 * $hits)
}
$saved = @{}
try {
    foreach ($key in $auditEnv.Keys) {
        $saved[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
        [Environment]::SetEnvironmentVariable($key, $auditEnv[$key], 'Process')
    }
    $p = Start-Process -FilePath $auditExe -WorkingDirectory $dir -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $dir 'stdout.log') -RedirectStandardError (Join-Path $dir 'stderr.log')
    # Cache the process handle so Windows PowerShell retains the redirected child's exit code.
    $null = $p.Handle
} finally {
    foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key, $saved[$key], 'Process') }
}
Write-Output "Hit sound audit: $dir"
try { $p.WaitForExit() }
finally {
    if ($p.HasExited -and (Test-Path -LiteralPath $auditExe)) { Remove-Item -LiteralPath $auditExe }
}
if ($p.ExitCode -ne 0) { throw "Game exited with code $($p.ExitCode)." }
& python (Join-Path $PSScriptRoot 'hit_sound_capture.py') (Join-Path $dir 'capture.wav') (Join-Path $dir 'stderr.log') --clips $dir
if ($LASTEXITCODE -ne 0) { throw 'Hit sound levels failed.' }
