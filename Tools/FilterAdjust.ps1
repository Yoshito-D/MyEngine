<#
.SYNOPSIS
Visual Studio の登録ファイルから、実際のフォルダ構成に合わせたフィルターを生成する。
.PARAMETER ProjectPath
対象の vcxproj。省略時はこのリポジトリの MyEngine.vcxproj を使用する。
#>
param(
    [string]$ProjectPath = "$PSScriptRoot\..\Project\MyEngine.vcxproj"
)

$ErrorActionPreference = 'Stop'

try {
    $projectFullPath = (Resolve-Path -LiteralPath $ProjectPath).Path
    $filtersPath = "$projectFullPath.filters"
    [xml]$projectXml = Get-Content -LiteralPath $projectFullPath -Raw -Encoding UTF8
    $namespace = 'http://schemas.microsoft.com/developer/msbuild/2003'
    $projectNamespaces = [System.Xml.XmlNamespaceManager]::new($projectXml.NameTable)
    $projectNamespaces.AddNamespace('msbuild', $namespace)

    # ディスク全体を走査せず、ビルド対象・表示対象として登録された項目だけを扱う。
    # 外部ライブラリの別プロジェクトや .vs 内の一時ファイルを混入させない。
    $itemTypes = @('ClCompile', 'ClInclude', 'None', 'Text', 'Image', 'FxCompile', 'CustomBuild')
    $items = @($projectXml.SelectNodes('/msbuild:Project/msbuild:ItemGroup/*[@Include]', $projectNamespaces) |
        Where-Object { $_.LocalName -in $itemTypes } |
        ForEach-Object {
            $include = $_.GetAttribute('Include') -replace '/', '\'
            $directory = Split-Path -Path $include -Parent
            # Project 外の Docs / Tools も、ソリューション上では独立したフォルダーで表示する。
            $filter = $directory -replace '^(\.\.[\\/])+', ''
            [pscustomobject]@{ Type = $_.LocalName; Include = $include; Filter = $filter }
        })

    # 再生成のたびに既存フィルターの GUID が変わることを防ぐ。
    $existingIds = @{}
    if (Test-Path -LiteralPath $filtersPath) {
        [xml]$existingXml = Get-Content -LiteralPath $filtersPath -Raw -Encoding UTF8
        $existingNamespaces = [System.Xml.XmlNamespaceManager]::new($existingXml.NameTable)
        $existingNamespaces.AddNamespace('msbuild', $namespace)
        foreach ($node in $existingXml.SelectNodes('/msbuild:Project/msbuild:ItemGroup/msbuild:Filter', $existingNamespaces)) {
            $identifier = $node.SelectSingleNode('msbuild:UniqueIdentifier', $existingNamespaces)
            if ($null -ne $identifier) {
                $existingIds[$node.GetAttribute('Include')] = $identifier.InnerText
            }
        }
    }

    $filterSet = [System.Collections.Generic.HashSet[string]]::new()
    foreach ($item in $items) {
        if ([string]::IsNullOrEmpty($item.Filter)) { continue }
        $parts = $item.Filter.Split('\')
        for ($index = 0; $index -lt $parts.Length; ++$index) {
            $filterSet.Add(($parts[0..$index] -join '\')) | Out-Null
        }
    }

    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add('<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">')
    $lines.Add('  <ItemGroup>')
    foreach ($filter in $filterSet | Sort-Object) {
        $identifier = $existingIds[$filter]
        if ([string]::IsNullOrEmpty($identifier)) { $identifier = '{' + [guid]::NewGuid().ToString() + '}' }
        $escapedFilter = [System.Security.SecurityElement]::Escape($filter)
        $lines.Add("    <Filter Include=""$escapedFilter"">")
        $lines.Add("      <UniqueIdentifier>$identifier</UniqueIdentifier>")
        $lines.Add('    </Filter>')
    }
    $lines.Add('  </ItemGroup>')

    foreach ($itemType in $itemTypes) {
        $typedItems = @($items | Where-Object { $_.Type -eq $itemType } | Sort-Object Include)
        if ($typedItems.Count -eq 0) { continue }
        $lines.Add('  <ItemGroup>')
        foreach ($item in $typedItems) {
            $escapedInclude = [System.Security.SecurityElement]::Escape($item.Include)
            if ([string]::IsNullOrEmpty($item.Filter)) {
                $lines.Add("    <$itemType Include=""$escapedInclude"" />")
            }
            else {
                $escapedFilter = [System.Security.SecurityElement]::Escape($item.Filter)
                $lines.Add("    <$itemType Include=""$escapedInclude"">")
                $lines.Add("      <Filter>$escapedFilter</Filter>")
                $lines.Add("    </$itemType>")
            }
        }
        $lines.Add('  </ItemGroup>')
    }
    $lines.Add('</Project>')

    # ソースと同じ UTF-8 / CRLF で保存する。
    [System.IO.File]::WriteAllText($filtersPath, ($lines -join "`r`n") + "`r`n", [System.Text.UTF8Encoding]::new($false))
    Write-Output "フィルターを更新しました: $filtersPath"
}
catch {
    Write-Error $_.Exception.Message
    exit 1
}
