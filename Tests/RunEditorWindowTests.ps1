# Run in a Visual Studio Developer PowerShell after building Debug/x64.
$ErrorActionPreference = 'Stop'
$workspacePath = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$projectPath = Join-Path $workspacePath 'Project'
$objectPath = Join-Path $workspacePath 'Generated/Obj/MyEngine/Debug'
$outputPath = Join-Path $workspacePath 'Generated/WindowMenuTests'
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null

[xml]$project = Get-Content -LiteralPath (Join-Path $projectPath 'MyEngine.vcxproj')
$namespaces = [System.Xml.XmlNamespaceManager]::new($project.NameTable)
$namespaces.AddNamespace('msbuild', $project.DocumentElement.NamespaceURI)
$linkArguments = [System.Collections.Generic.List[string]]::new()
$testObjectPath = Join-Path $outputPath 'EditorWindowTests.obj'
$testExecutablePath = Join-Path $outputPath 'EditorWindowTests.exe'
$linkArguments.Add('/OUT:"' + $testExecutablePath + '"')
$linkArguments.Add('/SUBSYSTEM:CONSOLE /INCREMENTAL:NO /OPT:REF /OPT:ICF /IGNORE:4099')
$linkArguments.Add('"' + $testObjectPath + '"')
foreach ($source in $project.SelectNodes('//msbuild:ClCompile[@Include]', $namespaces)) {
   if ($source.Include -eq 'main.cpp') { continue }
   $excluded = $source.SelectNodes('msbuild:ExcludedFromBuild', $namespaces) | Where-Object {
      $_.InnerText -eq 'true' -and $_.Condition.Contains('Debug|x64')
   }
   if ($excluded) { continue }
   $sourceObjectPath = Join-Path $objectPath ([System.IO.Path]::GetFileNameWithoutExtension($source.Include) + '.obj')
   if (-not (Test-Path -LiteralPath $sourceObjectPath)) { throw "Build Debug/x64 first: missing $sourceObjectPath" }
   $linkArguments.Add('"' + $sourceObjectPath + '"')
}
$linkArguments.Add('"' + (Join-Path $projectPath 'Externals/DirectXTex/Bin/Desktop_2022_Win10/x64/Debug/DirectXTex.lib') + '"')
$linkArguments.Add('/LIBPATH:"' + (Join-Path $projectPath 'Externals/assimp/lib/Debug') + '"')
$linkArguments.Add('assimp-vc143-mtd.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib')
$responsePath = Join-Path $outputPath 'EditorWindowTests.rsp'
[System.IO.File]::WriteAllLines($responsePath, $linkArguments, [System.Text.Encoding]::Unicode)

& cl.exe /nologo /c /EHsc /std:c++latest /utf-8 /MTd /D_DEBUG /DUSE_IMGUI /DNOMINMAX `
   ('/I' + $projectPath) ('/I' + (Join-Path $projectPath 'Externals')) `
   ('/I' + (Join-Path $projectPath 'Externals/imgui')) `
   (Join-Path $PSScriptRoot 'EditorWindowTests.cpp') ('/Fo' + $testObjectPath)
if ($LASTEXITCODE -ne 0) { throw 'EditorWindowTests compilation failed' }
& link.exe /nologo ('@' + $responsePath)
if ($LASTEXITCODE -ne 0) { throw 'EditorWindowTests link failed' }
$assimpPath = Join-Path $projectPath 'Externals/assimp/lib/Debug/assimp-vc143-mtd.dll'
if (Test-Path -LiteralPath $assimpPath) { Copy-Item -LiteralPath $assimpPath -Destination $outputPath -Force }
$compilerDllPath = Join-Path $workspacePath 'Generated/Outputs/Debug/dxcompiler.dll'
if (Test-Path -LiteralPath $compilerDllPath) { Copy-Item -LiteralPath $compilerDllPath -Destination $outputPath -Force }
& $testExecutablePath
if ($LASTEXITCODE -ne 0) { throw 'EditorWindowTests failed' }
