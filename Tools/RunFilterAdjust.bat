@echo off
rem 文字化けを防ぐために文字コードをUTF-8に設定
chcp 65001 >nul

rem バッチファイルの場所を基準にするため、起動時の作業フォルダーには依存しない。
rem 通常のプロジェクトファイル更新に管理者権限は不要。
echo フィルターファイルを生成しています...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0FilterAdjust.ps1"

if errorlevel 1 (
    echo エラー：FilterAdjust.ps1 の実行に失敗しました。
    pause
    exit /b 1
)

echo ファイルの更新に成功しました！
pause
