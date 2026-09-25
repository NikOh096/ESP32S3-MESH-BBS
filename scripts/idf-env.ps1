param([string]$IdfPath = 'C:\esp\v6.1\esp-idf', [string]$ToolsPath = 'C:\Espressif\tools')
$env:IDF_PATH = $IdfPath
$env:IDF_TOOLS_PATH = $ToolsPath
$env:IDF_PYTHON_ENV_PATH = Join-Path $ToolsPath 'python\v6.1\venv'
$env:IDF_COMPONENT_MANAGER = '0'
$env:IDF_CCACHE_ENABLE = '0'
if (-not $env:IDF_PY_BUILD_JOBS) { $env:IDF_PY_BUILD_JOBS = '4' }
$env:PYTHONUTF8 = '1'
# Process-local trust for this installation, without changing global Git settings.
$env:GIT_CONFIG_COUNT = '1'
$env:GIT_CONFIG_KEY_0 = 'safe.directory'
$env:GIT_CONFIG_VALUE_0 = $IdfPath.Replace('\','/')
$env:ESP_ROM_ELF_DIR = Join-Path $ToolsPath 'esp-rom-elfs\20241011'
$bins = @(
    (Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts'),
    (Join-Path $ToolsPath 'cmake\4.0.3\bin'),
    (Join-Path $ToolsPath 'ninja\1.12.1'),
    (Join-Path $ToolsPath 'xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin'),
    (Join-Path $ToolsPath 'esp32ulp-elf\2.38_20240113\esp32ulp-elf\bin')
)
$env:PATH = ($bins -join ';') + ';' + $env:PATH
function global:Invoke-MeshIdf {
    & (Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe') (Join-Path $env:IDF_PATH 'tools\idf.py') @args
}
