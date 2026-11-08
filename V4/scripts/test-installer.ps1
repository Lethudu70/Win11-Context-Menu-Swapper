param(
    [Parameter(Mandatory=$true)][string]$Installer,
    [Parameter(Mandatory=$true)][string]$PortableDirectory,
    [Parameter(Mandatory=$true)][string]$ReportDirectory
)
$ErrorActionPreference = 'Stop'
$setup = (Resolve-Path -LiteralPath $Installer).Path
$payload = (Resolve-Path -LiteralPath $PortableDirectory).Path
New-Item -ItemType Directory -Path $ReportDirectory -Force | Out-Null
$reportRoot = (Resolve-Path -LiteralPath $ReportDirectory).Path
$uninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{786E729A-5920-4855-A954-A209C9227904}_is1'
$userHive = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::CurrentUser, [Microsoft.Win32.RegistryView]::Registry64)
$previous = $userHive.OpenSubKey($uninstallKey)
if ($previous) { $previous.Dispose(); $userHive.Dispose(); throw 'An existing installation uses this AppId. Refusing to alter it for a test.' }
$token = [Guid]::NewGuid().ToString('N')
$smokeRoot = Join-Path $reportRoot "i-$token"
$appDir = Join-Path $smokeRoot 'app files'
$groupName = 'LethTools\Context Menu Swapper'
$groupDir = Join-Path ([Environment]::GetFolderPath('Programs')) $groupName
if (Test-Path -LiteralPath $appDir) { throw 'Test destination already exists' }
if (Test-Path -LiteralPath $groupDir) { throw 'Test shortcut group already exists' }
New-Item -ItemType Directory -Path $smokeRoot -Force | Out-Null
$checks = [System.Collections.Generic.List[object]]::new()
function Check([string]$Name, [bool]$Passed) {
    $checks.Add([pscustomobject]@{name=$Name;passed=$Passed})
    if (-not $Passed) { throw "Installer verification failed: $Name" }
}
function Run-Hidden([string]$File, [string[]]$Arguments, [int]$Timeout = 60000) {
    $process = Start-Process -FilePath $File -ArgumentList $Arguments -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit($Timeout)) { $process.Kill(); throw "Timed out: $File" }
    $process.Refresh()
    if ($process.ExitCode -ne 0) { throw "Process failed ($($process.ExitCode)): $File" }
}
$uninstaller = Join-Path $appDir 'unins000.exe'
$failure = $null
try {
    $installLog = Join-Path $smokeRoot 'install.log'
    Run-Hidden $setup @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-','/LANG=french',
        ('/DIR="'+$appDir+'"'),('/GROUP="'+$groupName+'"'),'/TASKS=""',('/LOG="'+$installLog+'"'))
    Check 'installed executable exists' (Test-Path -LiteralPath (Join-Path $appDir 'ContextMenuSwapper.exe'))
    $files = @(Get-ChildItem -LiteralPath $payload -Recurse -File | Where-Object Name -NotIn @('self-test.json','manifest-sha256.json'))
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($payload.Length+1)
        $installed = Join-Path $appDir $relative
        if (-not (Test-Path -LiteralPath $installed) -or
            (Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash) {
            throw "Payload mismatch: $relative"
        }
    }
    Check 'all installed payload hashes match the published portable' ($files.Count -gt 20)
    $entry = $userHive.OpenSubKey($uninstallKey)
    Check 'registered in current-user Apps list' ($null -ne $entry)
    Check 'registered version is 4.0.0' ($entry.GetValue('DisplayVersion') -eq '4.0.0')
    Check 'Apps list uses the application icon' ($entry.GetValue('DisplayIcon') -like '*ContextMenuSwapper.exe*')
    $entry.Dispose()
    $shortcutPath = Join-Path $groupDir 'Context Menu Swapper v4.0.lnk'
    Check 'Start menu shortcut exists' (Test-Path -LiteralPath $shortcutPath)
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($shortcutPath)
    Check 'shortcut targets the installed application' ($shortcut.TargetPath -eq (Join-Path $appDir 'ContextMenuSwapper.exe'))
    Check 'shortcut uses the embedded icon' ($shortcut.IconLocation -like '*ContextMenuSwapper.exe*')
    $savedPath = $env:PATH
    try {
        $env:PATH = "$env:WINDIR\System32;$env:WINDIR"
        $testReport = Join-Path $smokeRoot 'installed-self-test.json'
        Run-Hidden (Join-Path $appDir 'ContextMenuSwapper.exe') @('--self-test','--report',('"'+$testReport+'"')) 30000
        $result = Get-Content -LiteralPath $testReport -Raw | ConvertFrom-Json
        Check 'installed application passes self-test using bundled DLLs' ([bool]$result.passed)
    } finally { $env:PATH = $savedPath }
    Check 'uninstaller exists' (Test-Path -LiteralPath $uninstaller)
} catch { $failure = $_.Exception.Message }
finally {
    if (Test-Path -LiteralPath $uninstaller) {
        try { Run-Hidden $uninstaller @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/LOG="'+(Join-Path $smokeRoot 'uninstall.log')+'"')) }
        catch { if (-not $failure) { $failure = $_.Exception.Message } }
    }
    $entryAfter = $userHive.OpenSubKey($uninstallKey)
    $checks.Add([pscustomobject]@{name='uninstall registration removed';passed=($null -eq $entryAfter)})
    if ($entryAfter) { $entryAfter.Dispose() }
    $userHive.Dispose()
    $checks.Add([pscustomobject]@{name='installed application removed';passed=(-not (Test-Path -LiteralPath (Join-Path $appDir 'ContextMenuSwapper.exe')))})
    $checks.Add([pscustomobject]@{name='test Start menu shortcut removed';passed=(-not (Test-Path -LiteralPath (Join-Path $groupDir 'Context Menu Swapper v4.0.lnk')))})
    $report = [pscustomobject]@{passed=(-not $failure -and @($checks | Where-Object {-not $_.passed}).Count -eq 0);error=$failure;checks=$checks;logDirectory=$smokeRoot}
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $reportRoot 'installer-verification.json') -Encoding UTF8
    $report | ConvertTo-Json -Depth 5 | Write-Output
}
if (-not $report.passed) { throw "Installer test failed: $failure" }
