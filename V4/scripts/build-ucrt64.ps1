param(
    [string]$UcrtRoot = 'C:\msys64\ucrt64',
    [string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $sourceRoot 'build'
$binRoot = Join-Path $UcrtRoot 'bin'
$env:PATH = "$binRoot;$env:PATH"
foreach ($tool in @('cmake.exe','ninja.exe','g++.exe','windeployqt.exe','objdump.exe')) {
    if (-not (Test-Path -LiteralPath (Join-Path $binRoot $tool))) { throw "Missing UCRT64 tool: $tool in $binRoot" }
}
function Invoke-Checked([string]$Executable, [string[]]$Arguments) {
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Executable failed ($LASTEXITCODE)" }
}
Invoke-Checked (Join-Path $binRoot 'cmake.exe') @('-S',$sourceRoot,'-B',$buildRoot,'-G','Ninja',"-DCMAKE_BUILD_TYPE=$Configuration","-DCMAKE_PREFIX_PATH=$UcrtRoot", "-DCMAKE_CXX_COMPILER=$binRoot/g++.exe", "-DCMAKE_MAKE_PROGRAM=$binRoot/ninja.exe")
Invoke-Checked (Join-Path $binRoot 'cmake.exe') @('--build',$buildRoot,'--parallel')
Invoke-Checked (Join-Path $binRoot 'ctest.exe') @('--test-dir',$buildRoot,'--output-on-failure')
& (Join-Path $PSScriptRoot 'package-portable.ps1') -UcrtRoot $UcrtRoot -BuildRoot $buildRoot
