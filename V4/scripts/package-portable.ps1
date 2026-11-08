param(
    [string]$UcrtRoot = 'C:\msys64\ucrt64',
    [string]$BuildRoot = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build')
)
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path -Parent $PSScriptRoot
$binRoot = Join-Path $UcrtRoot 'bin'
$env:PATH = "$binRoot;$env:PATH"
# Fresh versioned folder avoids stale DLLs without deleting existing distributions.
$distRoot = Join-Path $sourceRoot ('dist\ContextMenuSwapper-4.0.0-win64-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $distRoot -Force | Out-Null
$exe = Join-Path $distRoot 'ContextMenuSwapper.exe'
Copy-Item -LiteralPath (Join-Path $BuildRoot 'ContextMenuSwapper.exe') -Destination $exe
& (Join-Path $binRoot 'windeployqt.exe') --release --no-translations --no-system-d3d-compiler --no-opengl-sw $exe
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed' }
# windeployqt does not always copy transitive MinGW dependencies. Resolve every
# imported UCRT64 DLL recursively, including dependencies of Qt platform plugins.
$queue = [System.Collections.Generic.Queue[string]]::new()
Get-ChildItem -LiteralPath $distRoot -Recurse -File | Where-Object Extension -In '.exe','.dll' | ForEach-Object { $queue.Enqueue($_.FullName) }
$seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
while ($queue.Count -gt 0) {
    $binary = $queue.Dequeue()
    if (-not $seen.Add($binary)) { continue }
    $imports = & (Join-Path $binRoot 'objdump.exe') -p $binary
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect $binary" }
    foreach ($line in $imports) {
        if ($line -match 'DLL Name:\s*(\S+)') {
            $name = $Matches[1]
            $candidate = Join-Path $binRoot $name
            $target = Join-Path $distRoot $name
            if (Test-Path -LiteralPath $candidate) {
                if (-not (Test-Path -LiteralPath $target)) { Copy-Item -LiteralPath $candidate -Destination $target; $queue.Enqueue($target) }
            } elseif ($name -notmatch '^(api-ms-|ext-ms-)' -and -not (Test-Path -LiteralPath (Join-Path $env:WINDIR "System32\$name")) -and -not (Test-Path -LiteralPath $target)) {
                throw "Unresolved dependency: $name ($binary)"
            }
        }
    }
}
Copy-Item -LiteralPath (Join-Path $sourceRoot 'README.md') -Destination (Join-Path $distRoot 'README.md')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'resources\app.ico') -Destination (Join-Path $distRoot 'ContextMenuSwapper.ico')
$licenseRoot = Join-Path $distRoot 'licenses'
New-Item -ItemType Directory -Path $licenseRoot -Force | Out-Null
foreach ($package in @('qt6-base','qt6-svg','gcc-libs','libpng','freetype','harfbuzz','pcre2','zlib','zstd','libb2','brotli','double-conversion','graphite2','glib2','gettext-runtime','icu','libiconv','libmd4c','bzip2','libwinpthread')) {
    $licenseDir = Join-Path $UcrtRoot "share\licenses\$package"
    if (Test-Path -LiteralPath $licenseDir) { Copy-Item -LiteralPath $licenseDir -Destination $licenseRoot -Recurse }
}
$manifest = Get-ChildItem -LiteralPath $distRoot -Recurse -File | ForEach-Object {
    [pscustomobject]@{path=$_.FullName.Substring($distRoot.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
}
$manifest | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $distRoot 'manifest-sha256.json') -Encoding UTF8
# Test with no MSYS/UCRT64/Python directories in PATH: use bundled runtime only.
$savedPath = $env:PATH
try {
    $env:PATH = "$env:WINDIR\System32;$env:WINDIR"
    $reportArgument = '"' + (Join-Path $distRoot 'self-test.json') + '"'
    $test = Start-Process -FilePath $exe -ArgumentList @('--self-test','--report',$reportArgument) -WorkingDirectory $distRoot -WindowStyle Hidden -PassThru
    if (-not $test.WaitForExit(30000)) { $test.Kill(); throw 'Portable self-test timed out' }
    if ($test.ExitCode -ne 0) { throw "Portable self-test failed ($($test.ExitCode))" }
} finally { $env:PATH = $savedPath }
$zip = "$distRoot.zip"
Compress-Archive -LiteralPath $distRoot -DestinationPath $zip
Write-Host "Portable directory: $distRoot"
Write-Host "Portable archive: $zip"
