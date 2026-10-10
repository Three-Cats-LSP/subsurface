param(
    [Parameter(Mandatory)][string]$StagingDir,
    [Parameter(Mandatory)][string]$OutputDir
)
$ErrorActionPreference = 'Stop'
$StagingDir = (Resolve-Path -LiteralPath $StagingDir).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$OutputDir = (Resolve-Path -LiteralPath $OutputDir).Path
$env:QT_QPA_PLATFORM = 'windows'
$env:QT_QUICK_BACKEND = 'software'
# Validate only deployed Qt modules, not the runner's development installation.
Remove-Item Env:QT_PLUGIN_PATH,Env:QML2_IMPORT_PATH,Env:QML_IMPORT_PATH -ErrorAction SilentlyContinue
$env:PATH = "$StagingDir;$env:SystemRoot\System32;$env:SystemRoot"
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
            $deadline = [DateTime]::UtcNow.AddSeconds(60)
            while ([DateTime]::UtcNow -lt $deadline) {
                if ($process.WaitForExit(1000)) {
                    throw "$style/$theme exited during launch with code $($process.ExitCode)"
                }
                if (Select-String -LiteralPath $stderr -Pattern 'qqwindow devicePixelRatio' -Quiet) { break }
            }
            # Hidden windows need not receive ApplicationActive (which triggers
            # finishSetup), but the real QML root must have been instantiated.
            if (-not (Select-String -LiteralPath $stderr -Pattern 'qqwindow devicePixelRatio' -Quiet)) {
                throw "$style/$theme did not create the QML window; check diagnostics and startup dialogs"
            }
            if ($process.WaitForExit(3000)) {
                throw "$style/$theme exited after initialization with code $($process.ExitCode)"
            }
            Write-Output "$style/$theme launch passed at width $env:SUBSURFACE_MOBILE_WIDTH"
        } finally {
            if (-not $process.HasExited) { Stop-Process -Id $process.Id }
            Get-Content -LiteralPath $stdout,$stderr
        }
    }
}
