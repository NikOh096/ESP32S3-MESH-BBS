$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$msvc = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.32.31326'
$sdk = 'C:\Program Files (x86)\Windows Kits\10'
$sdkVer = '10.0.19041.0'
$env:PATH = (Join-Path $msvc 'bin\Hostx64\x64') + ';' + $env:PATH
$env:LIB = @((Join-Path $msvc 'lib\x64'), (Join-Path $sdk "Lib\$sdkVer\ucrt\x64"), (Join-Path $sdk "Lib\$sdkVer\um\x64")) -join ';'
$includes = @((Join-Path $msvc 'include'))
foreach ($part in @('ucrt','shared','um')) { $includes += (Join-Path $sdk "Include\$sdkVer\$part") }
$env:INCLUDE = $includes -join ';'
New-Item -ItemType Directory -Force -Path (Join-Path $repo 'build-host-meshcore') | Out-Null
Push-Location $repo
try {
    & (Join-Path $msvc 'bin\Hostx64\x64\cl.exe') /nologo /std:c11 /W3 /Od /Z7 /D_CRT_SECURE_NO_WARNINGS /DMESHBBS_MESHCORE=1 /Imain /Imeshcore/main tests/test_meshcore.c tests/test_threads.c tests/test_node_scan.c main/node_scan.c main/rules.c main/bulletins.c main/bans.c meshcore/main/meshcore_protocol.c meshcore/main/meshcore_ids.c /Febuild-host-meshcore/test_meshcore.exe /Fobuild-host-meshcore/ /link /INCREMENTAL:NO
    if ($LASTEXITCODE) { throw 'MeshCore test compilation failed' }
    & '.\build-host-meshcore\test_meshcore.exe'
    if ($LASTEXITCODE) { throw 'MeshCore tests failed' }
} finally { Pop-Location }
