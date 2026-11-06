#include "GameEngine/pch.h"
#include "GameEngine/Assets/Audio/AudioCacheStartup.h"
#include "GameEngine/Assets/Audio/SoundClipCache.h"
#include "GameEngine/Assets/ResourceAssetPath.h"
#include <cstdlib>
#include <iostream>
#include <mfapi.h>
#include <set>
#include <shellapi.h>
#include <stdexcept>
#include <string_view>

#pragma comment(lib, "shell32.lib")

namespace GameEngine {
void RunAudioCacheBuildCommandIfRequested() {
   int argumentCount = 0;
   LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);

   if (!arguments) {
      std::exit(1);
   }

   const bool prepareAudio = argumentCount > 1 && std::wstring_view(arguments[1]) == L"--prepare-audio";

   if (!prepareAudio) {
      LocalFree(arguments);
      return;
   }

   if (argumentCount != 3) {
      LocalFree(arguments);
      std::cerr << "Usage: MyEngine.exe --prepare-audio <Resources root>\n";
      std::exit(1);
   }

   const std::filesystem::path resourcesRoot(arguments[2]);
   LocalFree(arguments);

   if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
      std::cerr << "Audio cache: COM initialization failed\n";
      std::exit(1);
   }

   if (FAILED(MFStartup(MF_VERSION))) {
      CoUninitialize();
      std::cerr << "Audio cache: Media Foundation initialization failed\n";
      std::exit(1);
   }

   int exitCode = 1;

   try {
      if (!std::filesystem::is_directory(resourcesRoot)) {
         throw std::runtime_error("Resources folder does not exist");
      }

      size_t generated = 0;
      size_t reused = 0;
      size_t removed = 0;

      for (const auto* root : kAudioRoots) {
         const auto soundsRoot = resourcesRoot / root;
         const auto cookedRoot = soundsRoot / kAudioCookedDirectory;

         if (!std::filesystem::exists(soundsRoot)) {
            continue;
         }

         // Windowsの大文字小文字と区切り文字の違いで、有効なキャッシュを削除しないよう比較する。
         const auto comparePaths = [](const std::filesystem::path& lhs, const std::filesystem::path& rhs) {
            const auto left = lhs.generic_wstring();
            const auto right = rhs.generic_wstring();
            return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_LESS_THAN;
         };
         std::set<std::filesystem::path, decltype(comparePaths)> expected(comparePaths);

         for (auto iterator = std::filesystem::recursive_directory_iterator(soundsRoot);
            iterator != std::filesystem::recursive_directory_iterator(); ++iterator) {
            if (iterator->is_symlink()) {
               iterator.disable_recursion_pending();
               continue;
            }

            const auto& path = iterator->path();

            if (iterator->is_directory()) {
               if (CompareStringOrdinal(path.c_str(), -1, cookedRoot.c_str(), -1, TRUE) == CSTR_EQUAL) {
                  iterator.disable_recursion_pending();
               }

               continue;
            }

            if (!iterator->is_regular_file()) {
               continue;
            }

            const auto assetId = ResourcePathToUtf8(path.lexically_relative(resourcesRoot));
            const auto cache = SoundClipCache::GetDiskCachePath(assetId, resourcesRoot);

            if (cache.empty()) {
               continue;
            }

            expected.insert(cache);

            if (SoundClipCache::PrepareDiskCache(assetId, resourcesRoot)) {
               ++generated;
            } else {
               ++reused;
            }
         }

         if (!std::filesystem::is_directory(cookedRoot)) {
            continue;
         }

         // 対象音源の準備が成功してから、削除・改名された元音源のキャッシュだけを整理する。
         for (auto iterator = std::filesystem::recursive_directory_iterator(cookedRoot);
            iterator != std::filesystem::recursive_directory_iterator(); ++iterator) {
            if (iterator->is_symlink()) {
               iterator.disable_recursion_pending();
               continue;
            }

            if (iterator->is_regular_file() && iterator->path().extension() == ".pcmcache" &&
                !expected.contains(iterator->path())) {
               std::filesystem::remove(iterator->path());
               ++removed;
            }
         }
      }

      std::cout << "Audio cache: " << generated << " generated, " << reused << " reused, " << removed << " removed\n";
      exitCode = 0;
   } catch (const std::exception& error) {
      std::cerr << "Audio cache preparation failed: " << error.what() << '\n';
   }

   MFShutdown();
   CoUninitialize();
   std::exit(exitCode);
}
}
