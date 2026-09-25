$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'idf-env.ps1')
Push-Location $repo
try {
    Invoke-MeshIdf build
    if ($LASTEXITCODE) { throw 'ESP-IDF build failed' }
} finally { Pop-Location }
