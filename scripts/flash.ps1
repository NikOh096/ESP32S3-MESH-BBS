param(
    [Parameter(Mandatory=$true)][ValidatePattern('^COM[0-9]+$')][string]$Port,
    [switch]$Backup
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'idf-env.ps1')
$python = Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe'
Push-Location $repo
try {
    # Rebuild the working configuration; release-profile builds may have left
    # a different memory/LED image in this directory.
    Invoke-MeshIdf build
    if ($LASTEXITCODE) { throw 'Build failed. No firmware was written.' }
    $configuration = Get-Content -LiteralPath 'sdkconfig' -Raw
    if ($configuration -notmatch '(?m)^CONFIG_ESPTOOLPY_FLASHSIZE="(8MB|16MB)"') { throw 'Unsupported flash configuration' }
    $flashSize = $Matches[1]
    foreach ($file in @('build\bootloader\bootloader.bin','build\partition_table\partition-table.bin','build\MESHBBS.bin')) {
        if (-not (Test-Path -LiteralPath $file)) { throw "Missing $file. Build the firmware first." }
    }
    & $python -m esptool --chip esp32s3 --port $Port flash-id
    if ($LASTEXITCODE) { throw 'Board identification failed. No firmware was written.' }
    if ($Backup) {
        New-Item -ItemType Directory -Path '.local' -Force | Out-Null
        $backupFile = Join-Path '.local' ("board-before-flash-" + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.bin')
        & $python -m esptool --chip esp32s3 --port $Port --baud 460800 read-flash 0 ALL $backupFile
        if ($LASTEXITCODE) { throw 'Backup failed. No firmware was written.' }
    }
    # Write only bootloader, partition table and app; preserve NVS/BBS partitions.
    & $python -m esptool --chip esp32s3 --port $Port --baud 460800 write-flash --flash-mode dio --flash-size $flashSize --flash-freq 80m 0x0 build\bootloader\bootloader.bin 0x8000 build\partition_table\partition-table.bin 0x10000 build\MESHBBS.bin
    if ($LASTEXITCODE) { throw 'Flash failed; check cable/port and retry.' }
} finally { Pop-Location }
