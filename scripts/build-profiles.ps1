param(
    [ValidateSet('meshtastic','meshcore')][string]$Protocol = 'meshtastic',
    [ValidateSet(8,16)][int[]]$FlashSizes = @(16,8)
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'idf-env.ps1')
$project = if ($Protocol -eq 'meshcore') { Join-Path $repo 'meshcore' } else { $repo }
$binary = if ($Protocol -eq 'meshcore') { 'MESHBBS-MeshCore.bin' } else { 'MESHBBS.bin' }
$config = Join-Path $project 'sdkconfig'
if (-not (Test-Path -LiteralPath $config)) { throw 'Build the default project once before building profiles.' }
$original = Get-Content -LiteralPath $config -Raw
Push-Location $project
try {
    foreach ($size in $FlashSizes) {
        foreach ($gpio in @(48,38)) {
            $profile = "n${size}r8-gpio$gpio"
            $text = $original.Replace("`r`n", "`n") -replace '(?m)^CONFIG_MESHBBS_RGB_GPIO=.*$', "CONFIG_MESHBBS_RGB_GPIO=$gpio"
            $text = $text -replace '(?m)^(?:# )?CONFIG_ESPTOOLPY_FLASHSIZE_16MB(?:=y| is not set)$', $(if ($size -eq 16) { 'CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y' } else { '# CONFIG_ESPTOOLPY_FLASHSIZE_16MB is not set' })
            $text = $text -replace '(?m)^(?:# )?CONFIG_ESPTOOLPY_FLASHSIZE_8MB(?:=y| is not set)$', $(if ($size -eq 8) { 'CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y' } else { '# CONFIG_ESPTOOLPY_FLASHSIZE_8MB is not set' })
            $text = $text -replace '(?m)^CONFIG_ESPTOOLPY_FLASHSIZE=.*$', "CONFIG_ESPTOOLPY_FLASHSIZE=`"${size}MB`""
            Set-Content -LiteralPath $config -Value $text -Encoding utf8
            Invoke-MeshIdf reconfigure
            if ($LASTEXITCODE) { throw "$profile configuration failed" }
            $actual = Get-Content -LiteralPath $config -Raw
            if ($actual -notmatch "(?m)^CONFIG_ESPTOOLPY_FLASHSIZE=`"${size}MB`"\r?$") { throw "$profile flash size was not applied" }
            if ($actual -notmatch "(?m)^CONFIG_MESHBBS_RGB_GPIO=$gpio\r?$") { throw "$profile LED pin was not applied" }
            Invoke-MeshIdf build
            if ($LASTEXITCODE) { throw "$profile build failed" }
            $dest = Join-Path $repo "build-release\$Protocol\$profile"
            New-Item -ItemType Directory -Path $dest -Force | Out-Null
            Copy-Item -LiteralPath "build\$binary" -Destination (Join-Path $dest 'firmware.bin') -Force
            Copy-Item -LiteralPath 'build\bootloader\bootloader.bin' -Destination $dest -Force
            Copy-Item -LiteralPath 'build\partition_table\partition-table.bin' -Destination $dest -Force
            Copy-Item -LiteralPath $config -Destination (Join-Path $dest 'sdkconfig') -Force
            Write-Output "PROFILE COMPLETE: $Protocol/$profile"
        }
    }
} finally {
    Set-Content -LiteralPath $config -Value $original -Encoding utf8
    Pop-Location
}
