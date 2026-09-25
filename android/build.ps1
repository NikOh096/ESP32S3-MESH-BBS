param(
    [string]$SdkPath = "$env:LOCALAPPDATA\Android\Sdk",
    [string]$JavaPath = 'C:\Program Files\Android\Android Studio\jbr'
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$repo = Split-Path $root -Parent
$tools = Join-Path $SdkPath 'build-tools\36.1.0'
$platform = Join-Path $SdkPath 'platforms\android-36.1\android.jar'
$output = Join-Path $root 'build'
$work = Join-Path $output (Get-Date -Format 'yyyyMMdd-HHmmss')
foreach ($directory in @($output,$work,"$work\generated","$work\classes","$work\dex","$work\tests")) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}
function Invoke-Checked([string]$program, [string[]]$arguments) {
    & $program @arguments
    if ($LASTEXITCODE) { throw "$program failed with exit code $LASTEXITCODE" }
}
$java = Join-Path $JavaPath 'bin\java.exe'
$javac = Join-Path $JavaPath 'bin\javac.exe'
$jar = Join-Path $JavaPath 'bin\jar.exe'
$app = Join-Path $root 'app\src\main'
Invoke-Checked (Join-Path $tools 'aapt2.exe') @('compile','--dir',"$app\res",'-o',"$work\resources.zip")
Invoke-Checked (Join-Path $tools 'aapt2.exe') @('link','-o',"$work\base.apk",'-I',$platform,'--manifest',"$app\AndroidManifest.xml",'-A',"$app\assets",'--java',"$work\generated","$work\resources.zip")
$sources = @(Get-ChildItem -LiteralPath "$app\java","$work\generated" -Recurse -Filter '*.java' | Select-Object -ExpandProperty FullName)
Invoke-Checked $javac (@('-encoding','UTF-8','-source','8','-target','8','-classpath',$platform,'-d',"$work\classes") + $sources)
Invoke-Checked $javac @('-encoding','UTF-8','-d',"$work\tests","$app\java\com\meshbbs\setup\SetupCodec.java","$app\java\com\meshbbs\setup\SetupDraft.java","$app\java\com\meshbbs\setup\DeviceDiscovery.java","$app\java\com\meshbbs\setup\ActivityLog.java","$root\tests\SetupCodecTest.java","$root\tests\SetupFlowTest.java","$app\java\com\meshbbs\setup\OwnerProof.java","$root\tests\OwnerProofTest.java","$app\java\com\meshbbs\setup\NodePairing.java","$root\tests\NodePairingTest.java","$app\java\com\meshbbs\setup\RadioProfile.java","$root\tests\RadioProfileTest.java")
Invoke-Checked $java @('-cp',"$work\tests",'SetupCodecTest')
Invoke-Checked $java @('-cp',"$work\tests",'SetupFlowTest')
Invoke-Checked $java @('-cp',"$work\tests",'OwnerProofTest')
Invoke-Checked $java @('-cp',"$work\tests",'NodePairingTest')
Invoke-Checked $java @('-cp',"$work\tests",'RadioProfileTest')
Invoke-Checked $jar @('cf',"$work\classes.jar",'-C',"$work\classes",'.')
Invoke-Checked $java @('-cp',"$tools\lib\d8.jar",'com.android.tools.r8.D8','--lib',$platform,'--min-api','26','--output',"$work\dex","$work\classes.jar")
Copy-Item -LiteralPath "$work\base.apk" -Destination "$work\unaligned.apk"
Invoke-Checked $jar @('uf',"$work\unaligned.apk",'-C',"$work\dex",'classes.dex')
Invoke-Checked (Join-Path $tools 'zipalign.exe') @('-p','-f','4',"$work\unaligned.apk","$work\aligned.apk")
$privateDir = Join-Path $repo '.local'
New-Item -ItemType Directory -Path $privateDir -Force | Out-Null
$key = Join-Path $privateDir 'android-test-signing.p12'
if (-not (Test-Path -LiteralPath $key)) {
    Invoke-Checked (Join-Path $JavaPath 'bin\keytool.exe') @('-genkeypair','-keystore',$key,'-storetype','PKCS12','-storepass','android','-keypass','android','-alias','meshbbs-test','-keyalg','RSA','-keysize','2048','-validity','10000','-dname','CN=MESHBBS Development')
}
$apk = Join-Path $output 'MESHBBS-0.5.0.apk'
Invoke-Checked $java @('-jar',"$tools\lib\apksigner.jar",'sign','--ks',$key,'--ks-key-alias','meshbbs-test','--ks-pass','pass:android','--out',$apk,"$work\aligned.apk")
Invoke-Checked $java @('-jar',"$tools\lib\apksigner.jar",'verify','--verbose',$apk)
Invoke-Checked (Join-Path $tools 'aapt.exe') @('dump','badging',$apk)
Write-Output "APK: $apk"
