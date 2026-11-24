#include "GameProject/Game.h"
#include "GameEngine/Utility/Logger.h"
#include <cstdlib>
#include <exception>
#include <memory>
#include <Windows.h>

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
   try {
      std::unique_ptr<GameEngine::Framework> game = std::make_unique<Game>();
      game->Run();
      return EXIT_SUCCESS;
   } catch (const std::exception& error) {
      try {
         Logger::Critical(error.what());
      } catch (const std::exception&) {
         // ログファイルを開けない起動障害でも、元の障害をデバッグ出力へ残す。
         OutputDebugStringA(error.what());
      }

      MessageBoxW(nullptr, Logger::ConvertString(error.what()).c_str(), L"MyEngine", MB_OK | MB_ICONERROR);
      return EXIT_FAILURE;
   }
}
