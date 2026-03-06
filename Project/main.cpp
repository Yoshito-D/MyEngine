#include "GameProject/Game.h"
#include <Windows.h>
#include <memory>

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
   std::unique_ptr <GameEngine::Framework> game = std::make_unique<Game>();
   game->Run();
   return 0;
}
