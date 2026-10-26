#include "GameEngine/pch.h"
#include "GameEngine/Assets/Texture/TextureCacheStartup.h"
#include "GameEngine/Assets/Texture/TextureCacheBuilder.h"
#include "GameEngine/Utility/Logger.h"
#include <cstdlib>
#include <iostream>
#include <shellapi.h>
#include <stdexcept>
#include <string_view>

#pragma comment(lib, "shell32.lib")

namespace GameEngine {
void RunTextureCacheBuildCommandIfRequested() {
   int argumentCount = 0;
   LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);

   if (!arguments) {
      std::exit(1);
   }

   const bool prepareTextures = argumentCount > 1 && std::wstring_view(arguments[1]) == L"--prepare-textures";

   if (!prepareTextures) {
      LocalFree(arguments);
      return;
   }

   if (argumentCount != 3) {
      LocalFree(arguments);
      std::cerr << "Usage: MyEngine.exe --prepare-textures <Resources root>\n";
      std::exit(1);
   }

   const std::filesystem::path resourcesRoot(arguments[2]);
   LocalFree(arguments);

   if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
      std::cerr << "Texture cache: COM initialization failed\n";
      std::exit(1);
   }

   // ビルド時は同じ実行ファイルの生成処理だけを呼び、ウィンドウやGPUを初期化しない。
   int exitCode = 1;

   try {
      const auto result = PrepareTextureCache(resourcesRoot, TextureCacheUpdateMode::ChangedAndMissing);

      if (result.succeeded) {
         std::cout << "Texture cache: " << result.generated << " generated, " << result.reused << " reused\n";
         exitCode = 0;
      } else {
         std::cerr << "Texture cache generation failed: " << result.error << '\n';
      }
   } catch (const std::exception& error) {
      std::cerr << "Texture cache preparation failed: " << error.what() << '\n';
   }

   CoUninitialize();

   // WinMainを変更せず、生成専用プロセスの成否をMSBuildへ伝える。
   // ゲームの初期化前なので、GPUやウィンドウの終了処理は不要。
   std::exit(exitCode);
}

void PrepareStartupTextureCache() {
   const auto textureCache = PrepareTextureCache();

   if (!textureCache.succeeded) {
      throw std::runtime_error("Texture cache generation failed: " + textureCache.error);
   }

   Logger::Info(std::format("Texture cache: {} generated, {} reused, {:.3f} ms", textureCache.generated,
      textureCache.reused, textureCache.milliseconds));
}
}
