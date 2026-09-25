param([Parameter(Mandatory=$true)][ValidatePattern('^COM[0-9]+$')][string]$Port)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'idf-env.ps1')
Push-Location (Join-Path $repo 'meshcore')
try {
    Invoke-MeshIdf build
    if ($LASTEXITCODE) { throw 'Build failed. No firmware was written.' }
    $configuration = Get-Content -LiteralPath 'sdkconfig' -Raw
    if ($configuration -notmatch '(?m)^CONFIG_ESPTOOLPY_FLASHSIZE="(8MB|16MB)"') { throw 'Unsupported flash configuration' }
    $flashSize = $Matches[1]
    foreach ($file in @('build\bootloader\bootloader.bin','build\partition_table\partition-table.bin','build\MESHBBS-MeshCore.bin')) {
        if (-not (Test-Path -LiteralPath $file)) { throw "Build MeshCore firmware first; missing $file" }
    }
    & (Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe') -m esptool --chip esp32s3 --port $Port --baud 460800 write-flash --flash-mode dio --flash-size $flashSize --flash-freq 80m 0x0 build\bootloader\bootloader.bin 0x8000 build\partition_table\partition-table.bin 0x10000 build\MESHBBS-MeshCore.bin
    if ($LASTEXITCODE) { throw 'MeshCore BBS flash failed' }
} finally { Pop-Location }
