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
New-Item -ItemType Directory -Force -Path (Join-Path $repo 'build-host') | Out-Null
$sources = @('tests/test_core.c','tests/test_threads.c','tests/test_node_scan.c','main/node_scan.c','main/rules.c','main/bulletins.c','main/bans.c','main/bbs.c','main/mesh_protocol.c','main/setup_protocol.c','main/activity.c',
    'components/nanopb/pb_common.c','components/nanopb/pb_encode.c','components/nanopb/pb_decode.c')
$sources += (Get-ChildItem -LiteralPath (Join-Path $repo 'components/meshtastic/meshtastic') -Filter '*.c').FullName
$exe = Join-Path $repo 'build-host/test_core.exe'
Push-Location $repo
try {
    & (Join-Path $msvc 'bin\Hostx64\x64\cl.exe') /nologo /std:c11 /W3 /Od /Z7 /D_CRT_SECURE_NO_WARNINGS /Imain /Icomponents/nanopb /Icomponents/meshtastic $sources "/Febuild-host\test_core.exe" "/Fobuild-host\\" /link /INCREMENTAL:NO
    if ($LASTEXITCODE) { throw 'Native test compilation failed' }
    & $exe
    if ($LASTEXITCODE) { throw 'Native tests failed' }
} finally { Pop-Location }
