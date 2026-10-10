param(
    [Parameter(Mandatory)][string]$StagingDir,
    [Parameter(Mandatory)][string]$OutputDir
)
$ErrorActionPreference = 'Stop'
$StagingDir = (Resolve-Path -LiteralPath $StagingDir).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$OutputDir = (Resolve-Path -LiteralPath $OutputDir).Path
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_QUICK_BACKEND = 'software'
# A unique --user profile isolates both QSettings and the native dive-log path.
$profileName = 'neo-smoke-' + [guid]::NewGuid().ToString('N')
$settingsPath = "HKCU:\Software\Subsurface\Subsurface-$profileName"
foreach ($style in @('Clean', 'Modern')) {
    foreach ($theme in @('Blue', 'Dark')) {
        $appearancePath = "$settingsPath\subsurface-neo\interface"
        $themePath = "$settingsPath\Display\Theme"
        New-Item -Force -Path $appearancePath,$themePath | Out-Null
        New-ItemProperty -Path $appearancePath -Name style -Value $style -PropertyType String -Force | Out-Null
        New-ItemProperty -Path $themePath -Name currentTheme -Value $theme -PropertyType String -Force | Out-Null
        $env:SUBSURFACE_MOBILE_WIDTH = if ($theme -eq 'Blue') { '1100' } else { '390' }
        $env:SUBSURFACE_MOBILE_HEIGHT = '760'
        $stdout = Join-Path $OutputDir "$style-$theme.stdout.log"
        $stderr = Join-Path $OutputDir "$style-$theme.stderr.log"
        $process = Start-Process -FilePath (Join-Path $StagingDir 'subsurface-mobile.exe') `
            -ArgumentList "--user=$profileName" -WorkingDirectory $StagingDir `
            -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        try {
            if ($process.WaitForExit(15000)) {
                throw "$style/$theme exited during launch with code $($process.ExitCode)"
            }
            Write-Output "$style/$theme launch passed at width $env:SUBSURFACE_MOBILE_WIDTH"
        } finally {
            if (-not $process.HasExited) { Stop-Process -Id $process.Id }
            Get-Content -LiteralPath $stdout,$stderr
        }
    }
}
