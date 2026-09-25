param([string]$Version = '0.5.0')
$ErrorActionPreference = 'Stop'
if ($Version -ne '0.5.0') { throw 'Update the version in prepare-release.py before creating a new release.' }
$repo = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'idf-env.ps1')
& (Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts/python.exe') -X utf8 (Join-Path $PSScriptRoot 'prepare-release.py')
if ($LASTEXITCODE) { throw 'Release validation or packaging failed.' }
