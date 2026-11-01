# Record isolated Project Midas PR #104 gameplay without touching Iron Sights data.
param(
    [ValidateSet('Auto', 'Manual')][string]$Mode = 'Auto',
    [string[]]$Stages = @('dam', 'facility'),
    [ValidateRange(30, 900)][int]$Seconds = 90,
    [ValidateRange(1, 60)][int]$CaptureFps = 30,
    [string]$RomPath,
    [string]$Worktree,
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$expectedHead = '05a8d8359baa2091b7c442c7156e34d9c41acf7b'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (!$Worktree) { $Worktree = Join-Path (Split-Path $repo) 'Project-Midas-ambient' }
if (!$RomPath) { $RomPath = Join-Path $repo 'baserom.u.z64' }
$Worktree = (Resolve-Path -LiteralPath $Worktree).Path
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
$head = (& git -C $Worktree rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -ne $expectedHead) {
    throw "PR #104 worktree must be at $expectedHead (found $head)."
}
if ((& git -C $Worktree status --porcelain=v1).Count) {
    throw 'PR #104 worktree has tracked or untracked changes; use a clean checkout.'
}
if (!(Get-Command ffmpeg -ErrorAction SilentlyContinue) -or
    !(Get-Command ffprobe -ErrorAction SilentlyContinue)) {
    throw 'ffmpeg and ffprobe are required on PATH.'
}
foreach ($stage in $Stages) {
    if ($stage -cnotmatch '^[a-z][a-z0-9]*$') { throw "Invalid stage name: $stage" }
}
if (!$Stages.Count) { throw 'At least one stage is required.' }

$buildDir = Join-Path $Worktree 'build_win\pr104-gameplay'
$sourceExe = Join-Path $buildDir 'ProjectMidas.exe'
$buildStamp = Join-Path $buildDir 'audit-build-commit.txt'
if (!$SkipBuild) {
    $inUse = @(Get-CimInstance Win32_Process | Where-Object {
        $_.Name -ieq 'ProjectMidas.exe' -and
        $_.ExecutablePath -ieq $sourceExe
    })
    if ($inUse.Count) { throw "The isolated PR #104 build is running: $sourceExe" }
    $mount = '{0}:/home/dev/project' -f $Worktree.Replace('\', '/')
    $assetMount = '{0}:/home/dev/project/assets:ro' -f (Join-Path $repo 'assets').Replace('\', '/')
    Write-Host 'Configuring isolated PR #104 Windows build...'
    & docker run --rm -v $mount -v $assetMount -w /home/dev/project goldeneye cmake -B build_win/pr104-gameplay `
        '-DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake' '-DCMAKE_BUILD_TYPE=Debug' `
        '-DGE_VIDEO=ON' '-DGE_ROMDATA_FALLBACK=OFF' `
        '-DSDL2_MINGW_ROOT=/home/dev/sdl2-mingw/SDL2-2.30.11/i686-w64-mingw32'
    if ($LASTEXITCODE -ne 0) { throw 'PR #104 CMake configuration failed.' }
    Write-Host 'Building isolated PR #104 Windows executable...'
    & docker run --rm -v $mount -v $assetMount -w /home/dev/project goldeneye cmake --build build_win/pr104-gameplay -j8
    if ($LASTEXITCODE -ne 0) { throw 'PR #104 Windows build failed.' }
    Set-Content -LiteralPath $buildStamp -Value $head
} elseif (!(Test-Path -LiteralPath $buildStamp) -or
    (Get-Content -LiteralPath $buildStamp -Raw).Trim() -ne $head) {
    throw 'The isolated build has no matching PR #104 commit stamp. Run without -SkipBuild.'
}
foreach ($name in @('ProjectMidas.exe', 'SDL2.dll', 'gamecontrollerdb.txt', 'gamecontrollerdefault.txt')) {
    if (!(Test-Path -LiteralPath (Join-Path $buildDir $name))) {
        throw "The isolated PR #104 build is missing $name."
    }
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$runRoot = Join-Path $repo ("build_win\pr104-gameplay-runs\$stamp-$([guid]::NewGuid().ToString('N').Substring(0, 8))")
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
Write-Host "Local PR #104 evidence: $runRoot"

foreach ($stage in $Stages) {
    $runDir = Join-Path $runRoot $stage
    New-Item -ItemType Directory -Path (Join-Path $runDir 'Data') -Force | Out-Null
    foreach ($name in @('ProjectMidas.exe', 'SDL2.dll', 'gamecontrollerdb.txt', 'gamecontrollerdefault.txt')) {
        Copy-Item -LiteralPath (Join-Path $buildDir $name) -Destination $runDir
    }
    $auditExe = Join-Path $runDir 'ProjectMidas.exe'
    $stdout = Join-Path $runDir 'stdout.log'
    $stderr = Join-Path $runDir 'stderr.log'
    $csv = Join-Path $runDir 'process.csv'
    $videoPath = Join-Path $runDir 'gameplay.mp4'
    $metadataPath = Join-Path $runDir 'session.json'
    Set-Content -LiteralPath $csv -Value 'utc,elapsed_s,cpu_total_s,cpu_pct_one_core,working_set_mb,private_mb,handles,threads'
    Set-Content -LiteralPath (Join-Path $runDir 'midas.ini') -Value @(
        '[display]', 'fullscreen=window', 'maxfps=60', 'vsync=0', 'look_updown=1'
    )
    $frameCap = if ($Mode -eq 'Auto') { $Seconds * 120 } else { $null }
    $envValues = @{
        GE_ROM = $RomPath; GE_NOINISAVE = '1'; GE_WINDOW = '960x540'
        GE_STAGE = $stage; GE_STAGE_AFTER = '1'
        GE_SKIPINTRO = if ($Mode -eq 'Auto') { '1' } else { $null }
        GE_SEED = '1'; GE_FRAMES = if ($null -ne $frameCap) { [string]$frameCap } else { $null }
        GE_NOINPUT = $null; GE_DRIVE = $null; GE_INPUTPLAY = $null
        GE_MP = $null; GE_MENU = $null; GE_NOSLEEP = $null; GE_SHOT = $null
    }
    if ($Mode -eq 'Auto') { $envValues.GE_NOINPUT = '1'; $envValues.GE_DRIVE = '1' }
    $previous = @{}
    $game = $null
    $capture = $null
    $started = [datetime]::UtcNow
    $failure = $null
    $captureExit = $null
    $gameExit = $null
    $captureStarted = $null
    $closeRequested = $false
    $closeDeadline = $null
    try {
        foreach ($key in $envValues.Keys) {
            $previous[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
            if ($null -eq $envValues[$key]) {
                Remove-Item -LiteralPath "Env:$key" -ErrorAction SilentlyContinue
            } else {
                [Environment]::SetEnvironmentVariable($key, $envValues[$key], 'Process')
            }
        }
        try {
            Write-Host "Starting $Mode gameplay in $stage (at least $Seconds seconds)..."
            $game = Start-Process -FilePath $auditExe -WorkingDirectory $runDir -PassThru `
                -RedirectStandardOutput $stdout -RedirectStandardError $stderr -WindowStyle Normal
        } finally {
            foreach ($key in $envValues.Keys) {
                if ($null -eq $previous[$key]) {
                    Remove-Item -LiteralPath "Env:$key" -ErrorAction SilentlyContinue
                } else {
                    [Environment]::SetEnvironmentVariable($key, $previous[$key], 'Process')
                }
            }
        }
        $window = [intptr]::Zero
        $deadline = (Get-Date).AddSeconds(60)
        while ((Get-Date) -lt $deadline) {
            $game.Refresh()
            if ($game.HasExited) { throw "The game exited before opening a window (code $($game.ExitCode))." }
            $window = $game.MainWindowHandle
            if ($window -ne [intptr]::Zero) { break }
            Start-Sleep -Milliseconds 250
        }
        if ($window -eq [intptr]::Zero) { throw 'The game did not open a capture window within 60 seconds.' }
        $game.Refresh()
        $windowTitle = $game.MainWindowTitle
        if (!$windowTitle) { throw 'The game capture window has no title.' }

        $ffmpeg = (Get-Command ffmpeg).Source
        $psi = [Diagnostics.ProcessStartInfo]::new()
        $psi.FileName = $ffmpeg
        $psi.WorkingDirectory = $runDir
        $psi.UseShellExecute = $false
        $psi.CreateNoWindow = $true
        $psi.RedirectStandardInput = $true
        foreach ($arg in @('-hide_banner', '-loglevel', 'warning', '-nostats', '-report', '-y',
                '-f', 'gdigrab', '-draw_mouse', '0', '-framerate', [string]$CaptureFps,
                '-i', "title=$windowTitle", '-an', '-vf',
                'scale=trunc(iw/2)*2:trunc(ih/2)*2', '-c:v', 'libx264',
                '-preset', 'veryfast', '-crf', '23', '-pix_fmt', 'yuv420p',
                '-movflags', '+faststart', $videoPath)) { [void]$psi.ArgumentList.Add($arg) }
        $capture = [Diagnostics.Process]::Start($psi)
        $captureStarted = [datetime]::UtcNow
        $sampleStart = $captureStarted
        $previousCpu = $null
        $previousTime = $null
        $culture = [Globalization.CultureInfo]::InvariantCulture
        $limit = [Math]::Max(120, $Seconds * 4)
        while (!$game.HasExited) {
            if ($capture.HasExited) { throw "ffmpeg exited during gameplay (code $($capture.ExitCode))." }
            $sample = Get-Process -Id $game.Id -ErrorAction SilentlyContinue
            if ($sample) {
                $now = [datetime]::UtcNow
                $elapsed = ($now - $sampleStart).TotalSeconds
                $cpu = [double]$sample.CPU
                $cpuPct = if ($null -ne $previousCpu) {
                    100 * ($cpu - $previousCpu) / ($now - $previousTime).TotalSeconds
                } else { 0 }
                $row = @($now.ToString('o'), $elapsed.ToString('F2', $culture),
                    $cpu.ToString('F2', $culture), $cpuPct.ToString('F1', $culture),
                    ($sample.WorkingSet64 / 1MB).ToString('F2', $culture),
                    ($sample.PrivateMemorySize64 / 1MB).ToString('F2', $culture),
                    $sample.HandleCount, $sample.Threads.Count) -join ','
                Add-Content -LiteralPath $csv -Value $row
                $previousCpu = $cpu
                $previousTime = $now
            }
            if (([datetime]::UtcNow - $started).TotalSeconds -gt $limit) {
                throw "Game exceeded the $limit-second audit limit."
            }
            if ($Mode -eq 'Manual' -and !$closeRequested -and
                ([datetime]::UtcNow - $captureStarted).TotalSeconds -ge $Seconds) {
                if (!$game.CloseMainWindow()) { throw 'Could not request a clean game-window close.' }
                $closeRequested = $true
                $closeDeadline = (Get-Date).AddSeconds(15)
                Write-Host "Requested clean $stage exit after $Seconds seconds."
            }
            if ($closeRequested -and (Get-Date) -gt $closeDeadline) {
                throw 'The game did not close within 15 seconds of the exit request.'
            }
            Start-Sleep -Seconds 1
            $game.Refresh()
        }
        $gameExit = $game.ExitCode
    } catch {
        $failure = $_.Exception.Message
    } finally {
        if ($game -and !$game.HasExited) {
            $owned = Get-CimInstance Win32_Process -Filter "ProcessId = $($game.Id)" -ErrorAction SilentlyContinue
            if ($owned -and $owned.ExecutablePath -ieq $auditExe) {
                Stop-Process -Id $game.Id -Force
            }
        }
        if ($capture) {
            if (!$capture.HasExited) {
                $capture.StandardInput.WriteLine('q')
                $capture.StandardInput.Flush()
                if (!$capture.WaitForExit(15000)) { $capture.Kill() }
            }
            $capture.WaitForExit()
            $captureExit = $capture.ExitCode
        }
        if ($game) { $game.WaitForExit() }
        if (Test-Path -LiteralPath $auditExe) { Remove-Item -LiteralPath $auditExe -Force }
    }

    $stdoutText = if (Test-Path -LiteralPath $stdout) { Get-Content -LiteralPath $stdout -Raw } else { '' }
    $videoInfo = $null
    if ($captureExit -eq 0 -and (Test-Path -LiteralPath $videoPath)) {
        $probe = & ffprobe -v error -show_entries format=duration,size:stream=width,height,nb_frames `
            -of json $videoPath | ConvertFrom-Json
        if ($LASTEXITCODE -eq 0) { $videoInfo = $probe }
    }
    $sampleCount = (Get-Content -LiteralPath $csv | Measure-Object -Line).Lines - 1
    $recordedSeconds = if ($videoInfo) {
        [double]::Parse($videoInfo.format.duration, [Globalization.CultureInfo]::InvariantCulture)
    } else { 0 }
    $result = [ordered]@{
        schema = 1; pr = 104; commit = $head; stage = $stage; mode = $Mode
        windows = [Environment]::OSVersion.VersionString
        started_utc = $started.ToString('o')
        capture_started_utc = if ($captureStarted) { $captureStarted.ToString('o') } else { $null }
        ended_utc = [datetime]::UtcNow.ToString('o')
        requested_capture_seconds = $Seconds; frame_cap = $frameCap
        configured_maxfps = 60
        configured_look_updown = 1
        scripted_drive = if ($Mode -eq 'Auto') { 1 } else { $null }
        wallclock_close_requested = $closeRequested
        game_exit_code = $gameExit; ffmpeg_exit_code = $captureExit
        process_samples = $sampleCount; recorded_seconds = $recordedSeconds
        stage_launch_logged = $stdoutText.Contains('GE_STAGE direct launch:')
        frame_limit_logged = $stdoutText.Contains('frame limit reached:')
        crash_log_present = Test-Path -LiteralPath (Join-Path $runDir 'Data\Logs\crash.log')
        visual_review = 'pending'; video = $videoInfo; failure = $failure
    }
    $result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $metadataPath
    Write-Host "$stage result: game=$gameExit capture=$captureExit stage=$($result.stage_launch_logged) frames=$($result.frame_limit_logged) failure=$failure"
    if ($failure -or $gameExit -ne 0 -or $captureExit -ne 0 -or !$videoInfo -or
        $recordedSeconds -lt $Seconds -or $sampleCount -lt 1 -or
        !$result.stage_launch_logged -or
        ($Mode -eq 'Auto' -and !$result.frame_limit_logged) -or $result.crash_log_present) {
        throw "PR #104 $stage audit is incomplete. Inspect $metadataPath and the local logs."
    }
}
Write-Host "PR #104 local capture evidence is complete: $runRoot"
