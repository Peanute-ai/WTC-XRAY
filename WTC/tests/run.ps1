$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (!(Test-Path $vswhere)) { throw 'Visual Studio Installer / vswhere.exe not found.' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Install Visual Studio C++ x64 build tools.' }
$vcvars = Join-Path $vs 'VC/Auxiliary/Build/vcvars64.bat'
$project = Split-Path $PSScriptRoot -Parent
$output = Join-Path $project 'build'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source = Join-Path $PSScriptRoot 'ground_prediction_tests.cpp'
$exe = Join-Path $output 'ground_prediction_tests.exe'
$obj = Join-Path $output 'ground_prediction_tests.obj'
$command = 'call "{0}" >nul && cl /nologo /std:c++20 /EHsc /O2 /W4 "{1}" /Fe:"{2}" /Fo:"{3}"' -f $vcvars,$source,$exe,$obj
& $env:COMSPEC /d /c $command
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe
exit $LASTEXITCODE
