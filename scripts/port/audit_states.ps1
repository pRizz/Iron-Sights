# Exercise real mission save/restore in an isolated directory, including relaunch.
param([string]$Executable,[switch]$CrossProcessOnly,[switch]$RenderThread)
$ErrorActionPreference='Stop'
$repoDir=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (!$Executable) { $Executable=Join-Path $repoDir 'build_win/IronSights.exe' }
$dir=Join-Path $repoDir ('build_win/state-audit-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$auditExe=[IO.Path]::GetFullPath((Join-Path $dir 'IronSights.exe'))
if (!$auditExe.StartsWith($repoDir+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Audit executable must stay inside the workspace.' }
New-Item -ItemType Directory -Path $dir | Out-Null
Copy-Item -LiteralPath $Executable -Destination (Join-Path $dir 'IronSights.exe')
Copy-Item -LiteralPath (Join-Path (Split-Path $Executable) 'SDL2.dll') -Destination $dir
Set-Content -LiteralPath (Join-Path $dir 'midas.ini') -Value @('[display]','fullscreen=window','vsync=0','maxfps=60','tickhz=60','msaa=0','[input]','mouse=1')
Add-Content -LiteralPath (Join-Path $dir 'midas.ini') -Value @('[display]',"render_thread=$(if ($RenderThread) {1} else {0})")
Set-Content -LiteralPath (Join-Path $dir 'input.txt') -Value @('P 0 0 0 0','P 80 8000 0 0','P 82 0 0 0','P 120 8000 0 0','P 122 0 0 0','P 160 8000 0 0','P 162 0 0 0','P 200 2000 20 30','P 240 0 0 0','P 400 2000 20 30','P 440 0 0 0','P 5000 0 0 0')
# Repeated short strafes exercise modern keyboard movement after each restore,
# independently of the legacy stick in P records and the intro's duration.
for ($tick=0;$tick -le 5000;$tick+=37) {
    $axis=if (($tick/37)%2 -eq 0) {1} else {-1}
    Add-Content -LiteralPath (Join-Path $dir 'input.txt') -Value "D $tick $axis 0"
}
$auditEnv=@{ GE_ROM=(Join-Path $repoDir 'baserom.u.z64'); GE_WINDOW='960x540'; GE_NOINPUT='1'; GE_STAGE_AFTER='1'; GE_CHEATS='invuln,invis,guns'; GE_FRAMES='5000'; GE_SEED='1'; GE_INPUTPLAY=(Join-Path $dir 'input.txt') }
$saved=@{}
try {
    foreach ($key in $auditEnv.Keys) { $saved[$key]=[Environment]::GetEnvironmentVariable($key,'Process'); [Environment]::SetEnvironmentVariable($key,$auditEnv[$key],'Process') }
    $modes=if ($CrossProcessOnly) {@('write','roundtrip','read')} else {@('write','roundtrip','watch','read','title','reject')}
    foreach ($mode in $modes) {
        $env:GE_STATE_AUDIT=$mode
        $env:GE_STAGE= if ($mode -eq 'read') {'facility'} else {'dam'}
        if ($mode -eq 'title') { Remove-Item Env:GE_STAGE; $env:GE_MENU='hud' }
        else { Remove-Item Env:GE_MENU -ErrorAction SilentlyContinue }
        if ($mode -eq 'reject') {
            $quick=Join-Path $dir 'Data/SaveStates/profile-0-slot-5.mstate'
            $bytes=[IO.File]::ReadAllBytes($quick)
            [IO.File]::WriteAllBytes($quick,$bytes[0..($bytes.Length-32)])
        }
        $proof=Join-Path $dir 'state-audit-ok.txt'
        if (Test-Path -LiteralPath $proof) { Remove-Item -LiteralPath $proof }
        $p=Start-Process -FilePath (Join-Path $dir 'IronSights.exe') -WorkingDirectory $dir -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $dir "$mode.stdout.txt") -RedirectStandardError (Join-Path $dir "$mode.stderr.txt")
        $null=$p.Handle
        if (!$p.WaitForExit(90000)) { $p.Kill(); throw "State audit timed out: $mode ($dir)" }
        if ($p.ExitCode -ne 0 -or !(Test-Path -LiteralPath $proof)) { throw "State audit failed: $mode, exit $($p.ExitCode) ($dir)" }
        Get-Content -LiteralPath $proof
    }
} finally {
    foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key,$saved[$key],'Process') }
    Remove-Item Env:GE_STATE_AUDIT -ErrorAction SilentlyContinue
    Remove-Item Env:GE_STAGE -ErrorAction SilentlyContinue
    Remove-Item Env:GE_MENU -ErrorAction SilentlyContinue
    # The copy is temporary; never retain old executables to preserve states.
    if ($p -and $p.HasExited -and (Test-Path -LiteralPath $auditExe)) {
        Remove-Item -LiteralPath $auditExe
    }
}
Write-Output "State audit: $dir"
