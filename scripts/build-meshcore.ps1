param([switch]$Clean)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'idf-env.ps1')
Push-Location (Join-Path $repo 'meshcore')
try {
    if ($Clean) { Invoke-MeshIdf fullclean; if ($LASTEXITCODE) { throw 'MeshCore clean failed' } }
    Invoke-MeshIdf build
    if ($LASTEXITCODE) { throw 'MeshCore firmware build failed' }
} finally { Pop-Location }
