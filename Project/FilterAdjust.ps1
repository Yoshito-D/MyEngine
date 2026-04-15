# ============================
# MyEngine.vcxproj.filters 生成スクリプト（完全版）
# ============================

$proj    = (Resolve-Path "MyEngine.vcxproj").Path
$filters = "$proj.filters"

try {
    # プロジェクトルート
    $root = (Split-Path $proj).TrimEnd('\')

    # 対象ファイル取得
    $files = Get-ChildItem $root -Recurse -File |
             Where-Object {
                 $_.Name -notlike '*.vcxproj' -and
                 $_.Name -notlike '*.vcxproj.filters' -and
                 $_.Name -notlike '*.user' -and
                 $_.Extension -match '^\.(cpp|c|h|hpp|json|xml|txt|ini|csv|md)$'
             }

    # すべてのフィルター（中間階層含む）を収集
    $filterSet = New-Object System.Collections.Generic.HashSet[string]

    foreach ($f in $files) {
        $rel = $f.FullName.Substring($root.Length + 1) -replace '/', '\'
        $dir = Split-Path $rel

        if ($dir -and $dir -ne ".") {
            $parts = $dir.Split('\')
            for ($i = 1; $i -le $parts.Length; $i++) {
                $filterSet.Add(($parts[0..($i - 1)] -join '\')) | Out-Null
            }
        }
    }

    # ===== XML 構築 =====
    $xml = @()
    $xml += '<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">'

    # --- Filter 定義（全階層） ---
    $xml += '  <ItemGroup>'
    foreach ($filter in $filterSet | Sort-Object) {
        $guid = [guid]::NewGuid().ToString()
        $xml += "    <Filter Include=""$filter"">"
        $xml += "      <UniqueIdentifier>{$guid}</UniqueIdentifier>"
        $xml += "    </Filter>"
    }
    $xml += '  </ItemGroup>'

    # --- None (json / xml / txt / ini / csv / md) ---
    $xml += '  <ItemGroup>'
    foreach ($f in $files | Where-Object { $_.Extension -match '\.(json|xml|txt|ini|csv|md)$' }) {
        $rel = $f.FullName.Substring($root.Length + 1) -replace '/', '\'
        $dir = Split-Path $rel

        if ($dir -and $dir -ne ".") {
            $xml += "    <None Include=""$rel""><Filter>$dir</Filter></None>"
        }
        else {
            $xml += "    <None Include=""$rel"" />"
        }
    }
    $xml += '  </ItemGroup>'

    # --- ClCompile (.cpp / .c) ---
    $xml += '  <ItemGroup>'
    foreach ($f in $files | Where-Object { $_.Extension -match '\.(cpp|c)$' }) {
        $rel = $f.FullName.Substring($root.Length + 1) -replace '/', '\'
        $dir = Split-Path $rel

        if ($dir -and $dir -ne ".") {
            $xml += "    <ClCompile Include=""$rel""><Filter>$dir</Filter></ClCompile>"
        }
        else {
            $xml += "    <ClCompile Include=""$rel"" />"
        }
    }
    $xml += '  </ItemGroup>'

    # --- ClInclude (.h / .hpp) ---
    $xml += '  <ItemGroup>'
    foreach ($f in $files | Where-Object { $_.Extension -match '\.(h|hpp)$' }) {
        $rel = $f.FullName.Substring($root.Length + 1) -replace '/', '\'
        $dir = Split-Path $rel

        if ($dir -and $dir -ne ".") {
            $xml += "    <ClInclude Include=""$rel""><Filter>$dir</Filter></ClInclude>"
        }
        else {
            $xml += "    <ClInclude Include=""$rel"" />"
        }
    }
    $xml += '  </ItemGroup>'

    $xml += '</Project>'

    # UTF-8 で保存
    $xml | Set-Content -LiteralPath $filters -Encoding UTF8 -Force
}
catch {
    Write-Error $_.Exception.Message
    exit 1
}
