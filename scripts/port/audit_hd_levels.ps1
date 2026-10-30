# Compare all campaign environments with the same isolated Original/HD audit.
param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$Pack,
    [string]$Output = 'build_win/hd-levels/campaign-audit.json'
)
$ErrorActionPreference = 'Stop'
$stages = @('dam','facility','runway','surface','bunker1','silo','frigate',
    'surface2','bunker2','statue','archives','streets','depot','train',
    'jungle','control','caverns','cradle','aztec','egypt')
$results = @()
$outputPath = [IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Path (Split-Path $outputPath) -Force | Out-Null
foreach ($stage in $stages) {
    Write-Host "HD campaign audit: $stage"
    $results += & "$PSScriptRoot/audit_hd_textures.ps1" -Executable $Executable -Pack $Pack -Stage $stage -AllowMissingMatches
    # Preserve completed evidence even if a later mission fails.
    $results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $outputPath
}
$missing = @($results | Where-Object { $_.Mode -eq 1 -and $_.Matches -eq 0 })
if ($missing.Count) { throw "No HD matches for: $($missing.Stage -join ', '); evidence: $outputPath" }
Write-Host "HD campaign audit: all $($stages.Count) missions passed; $outputPath"
