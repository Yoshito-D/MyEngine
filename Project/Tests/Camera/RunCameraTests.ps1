param(
   [string]$TracePath,
   [string]$ComparePath
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Visual C++ tools are required to run camera tests.' }
$buildRoot = Join-Path ([IO.Path]::GetTempPath()) ('camera-tests-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $buildRoot | Out-Null
try {
   [xml]$project = Get-Content -LiteralPath (Join-Path $projectRoot 'MyEngine.vcxproj')
   $namespaces = [Xml.XmlNamespaceManager]::new($project.NameTable)
   $namespaces.AddNamespace('msb', 'http://schemas.microsoft.com/developer/msbuild/2003')
   $includeNode = $project.SelectSingleNode('//msb:ClCompile/msb:AdditionalIncludeDirectories', $namespaces)
   $arguments = @('/nologo', '/std:c++latest', '/EHsc', '/utf-8', '/W3', '/DNOMINMAX', '/MD', '/Gy', '/fp:precise')
   foreach ($directory in $includeNode.InnerText.Split(';')) {
      if ($directory -and !$directory.StartsWith('%')) {
         $resolved = $directory.Replace('$(ProjectDir)', $projectRoot + '/')
         $arguments += '/I"' + $resolved + '"'
      }
   }
   $arguments += '/I"' + (Join-Path $projectRoot 'GameProject/App/Component/Camera') + '"'
   $sources = @(
      'Tests/Camera/CameraRegressionTests.cpp',
      'Tests/Camera/CameraTestServices.cpp',
      'GameProject/App/Component/Camera/PlayerRearFollowCamera.cpp',
      'GameEngine/Scene/Camera/Core/VirtualCamera.cpp',
      'GameEngine/Scene/Camera/Core/CinemachineBrain.cpp',
      'GameEngine/Utility/Math/Quaternion.cpp',
      'GameEngine/Utility/MathUtils/MatrixOperations.cpp',
      'GameEngine/Utility/MathUtils/QuaternionOperations.cpp'
   )
   $arguments += $sources | ForEach-Object { '"' + (Join-Path $projectRoot $_) + '"' }
   $arguments += Get-ChildItem -LiteralPath (Join-Path $projectRoot 'GameProject/App/Component/Camera') -Filter 'RearCamera*.cpp' | ForEach-Object { '"' + $_.FullName + '"' }
   $arguments += @('/Fe:CameraRegressionTests.exe', '/link /OPT:REF')
   $responsePath = Join-Path $buildRoot 'compile.rsp'
   [IO.File]::WriteAllLines($responsePath, $arguments, [Text.UTF8Encoding]::new($true))
   $commandPath = Join-Path $buildRoot 'compile.cmd'
   $vcvars = Join-Path $installation 'VC/Auxiliary/Build/vcvars64.bat'
   [IO.File]::WriteAllText($commandPath, "@echo off`r`ncall `"$vcvars`" >nul`r`nif errorlevel 1 exit /b 1`r`ncl @compile.rsp`r`n", [Text.Encoding]::Default)
   Push-Location $buildRoot
   try {
      & $env:ComSpec /c $commandPath
      if ($LASTEXITCODE -ne 0) { throw 'Camera test compilation failed.' }
   } finally { Pop-Location }
   Push-Location $projectRoot
   try {
      $testArguments = @()
      if ($TracePath) { $testArguments = @('--trace', $TracePath) }
      elseif ($ComparePath) { $testArguments = @('--compare', $ComparePath) }
      & (Join-Path $buildRoot 'CameraRegressionTests.exe') @testArguments
      if ($LASTEXITCODE -ne 0) { throw 'Camera regression tests failed.' }
   } finally { Pop-Location }
} finally {
   # Only this invocation's generated build folder can be removed.
   $resolvedBuildRoot = [IO.Path]::GetFullPath($buildRoot)
   $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
   if (!$resolvedBuildRoot.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -or
       !(Split-Path $resolvedBuildRoot -Leaf).StartsWith('camera-tests-')) {
      throw 'Refusing to remove a build folder outside the temporary directory.'
   }
   Remove-Item -LiteralPath $resolvedBuildRoot -Recurse -Force
}
