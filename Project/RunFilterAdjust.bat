@echo off
rem 文字化けを防ぐために文字コードをUTF-8に設定
chcp 65001 >nul

rem --- 管理者権限のチェックと自動昇格 ---
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo 管理者権限が必要です。自動で昇格して再実行します...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)
rem --------------------------------------

rem バッチファイルが置かれているフォルダに移動（必須）
cd /d "%~dp0"

echo フィルターファイルを生成しています...

rem ps1ファイルを実行（スクリプト側で .filters を保存）
powershell -NoProfile -ExecutionPolicy Bypass -File ".\FilterAdjust.ps1"

if %errorLevel% neq 0 (
    echo エラー：FilterAdjust.ps1 の実行に失敗しました。
) else if exist "MyEngine.vcxproj.filters" (
    echo ファイルの更新に成功しました！
) else (
    echo エラー：MyEngine.vcxproj.filters が生成されませんでした。
)

echo.
echo 処理が完了しました。
pause