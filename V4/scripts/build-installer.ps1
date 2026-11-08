param(
    [Parameter(Mandatory=$true)][string]$PortableDirectory,
    [string]$OutputDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'dist\setup'),
    [string]$Compiler = ''
)
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path -Parent $PSScriptRoot
$payload = (Resolve-Path -LiteralPath $PortableDirectory).Path
foreach ($name in @('ContextMenuSwapper.exe','Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','platforms\qwindows.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $payload $name))) { throw "Missing portable payload: $name" }
}
if (-not $Compiler) {
    $candidates = @(
        "${env:ProgramFiles(x86)}\Inno Setup 7\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 7\ISCC.exe",
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
    )
    $Compiler = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $Compiler -or -not (Test-Path -LiteralPath $Compiler)) { throw 'Install Inno Setup 6.7 or later, or pass -Compiler with the ISCC.exe path.' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$out = (Resolve-Path -LiteralPath $OutputDirectory).Path
$script = Join-Path $sourceRoot 'installer\ContextMenuSwapper.iss'
& $Compiler "/DPayloadDir=$payload" "/DInstallerOutput=$out" $script
if ($LASTEXITCODE -ne 0) { throw "Inno Setup compilation failed ($LASTEXITCODE)" }
$installer = Join-Path $out 'ContextMenuSwapper-v4.0-Setup.exe'
if (-not (Test-Path -LiteralPath $installer)) { throw 'Inno Setup did not create the expected setup EXE.' }
$sumFile = Join-Path $out 'ContextMenuSwapper-v4.0-Setup.sha256'
$sum = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText($sumFile, "$sum  ContextMenuSwapper-v4.0-Setup.exe" + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
Write-Host "Installer: $installer"
Write-Host "SHA-256: $sum"
