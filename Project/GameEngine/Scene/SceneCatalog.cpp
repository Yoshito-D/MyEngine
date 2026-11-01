#include "GameEngine/pch.h"
#include "GameEngine/Scene/SceneCatalog.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameEngine/Utility/JsonFile.h"
#include <fstream>
#include <cctype>
#include <unordered_set>

namespace GameEngine {
namespace {
std::filesystem::path Utf8Path(const std::string& text) {
   return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

std::string Fold(std::string text) {
   // UTF-8の非ASCIIバイトは保持し、ASCII英字の大文字小文字だけを同一視する。
   for (auto& c : text) {
      if (c >= 'A' && c <= 'Z') {
         c = static_cast<char>(c + ('a' - 'A'));
      }
   }
   return text;
}

bool ValidName(const std::string& name) {
   if (name.empty() || name == "." || name == ".." || name.back() == '.' || name.back() == ' ') {
      return false;
   }

   if (name.find_first_of("<>:\"/\\|?*") != std::string::npos) {
      return false;
   }

   for (unsigned char c : name) {
      if (c < 32) {
         return false;
      }
   }

   // Windowsの予約デバイス名は拡張子を付けてもファイル名に使えないため、最初のピリオドより前で調べる。
   const auto stem = Fold(name.substr(0, name.find('.')));
   return stem != "con" && stem != "prn" && stem != "aux" && stem != "nul" &&
          !(stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) && stem[3] >= '1' &&
             stem[3] <= '9');
}
}

bool SceneCatalog::Load(const std::filesystem::path& path) {
   try {
      std::ifstream file(path);

      if (!file) {
         throw std::runtime_error("Could not open scene catalog: " + path.generic_string());
      }
      nlohmann::json data;
      file >> data;

      if (!data.is_object() || !data.contains("scenes") || !data.at("scenes").is_object()) {
         throw std::runtime_error("Scene catalog requires a scenes object");
      }
      const auto initial = data.value("initialScene", std::string{});
      std::unordered_map<std::string, SceneDefinition> entries;
      std::unordered_set<std::string> names;

      // シーン名はファイル名にも使うので、ASCII英字の大文字小文字だけが異なる登録も拒否する。
      for (const auto& [name, value] : data.at("scenes").items()) {
         if (!ValidName(name) || !value.is_string() || !names.insert(Fold(name)).second) {
            throw std::runtime_error("Invalid or duplicate scene name: " + name);
         }
         auto relative = Utf8Path(value.get<std::string>()).lexically_normal();

         if (relative.empty() || relative.is_absolute() || relative.has_root_path()) {
            throw std::runtime_error("Scene path must be relative to resources");
         }

         // 正規化後にも残る親参照は、resourcesの外へ抜けるパスになるため許可しない。
         for (const auto& part : relative) {
            if (part == "..") {
               throw std::runtime_error("Scene path escapes resources");
            }
         }
         entries.emplace(name, SceneDefinition{name, std::filesystem::path("resources") / relative});
      }

      if (!entries.contains(initial)) {
         throw std::runtime_error("Startup scene is not registered");
      }

      // 全項目の検証が済むまでは公開中の登録内容を変更せず、読込・検証失敗時も旧カタログを使えるようにする。
      scenes_ = std::move(entries);
      initialSceneName_ = initial;
      catalogData_ = std::move(data);
      catalogPath_ = path;
      lastError_.clear();
      return true;
   } catch (const std::exception& error) {
      lastError_ = error.what();
      Logger::Error(lastError_);
      return false;
   }
}

bool SceneCatalog::Reload() {
   return Load(catalogPath_);
}

const SceneDefinition* SceneCatalog::Find(const std::string& name) const {
   auto it = scenes_.find(name);
   return it == scenes_.end() ? nullptr : &it->second;
}

std::vector<std::string> SceneCatalog::GetNames() const {
   std::vector<std::string> result;

   for (const auto& [name, definition] : scenes_) {
      (void)definition;
      result.push_back(name);
   }
   std::sort(result.begin(), result.end());
   return result;
}

nlohmann::json SceneCatalog::MakeEmptyScene(const std::string& name) {
   return {{"version", SceneWorld::kFormatVersion}, {"sceneName", name}, {"objects", nlohmann::json::array()},
      {"hierarchyOrder", nlohmann::json::array()},
      {"cameras", {{"brain", {{"defaultBlendTime", 0.0f}}}, {"virtualCameras", nlohmann::json::array()}}},
      {"renderSettings", {{"postProcessStack", nlohmann::json::array()}}}};
}

bool SceneCatalog::SaveCatalog(const nlohmann::json& data) {
   try {
      // ファイル置換より先に必要なメモリーを確保し、保存成功後は例外を送出しない操作で状態を確定する。
      auto updated = data;
      auto initial = data.at("initialScene").get<std::string>();
      std::unordered_map<std::string, SceneDefinition> entries;

      for (const auto& [name, path] : data.at("scenes").items()) {
         entries.emplace(
            name, SceneDefinition{name, std::filesystem::path("resources") / Utf8Path(path.get<std::string>())});
      }

      if (!SaveJsonFileAtomically(catalogPath_, data, 3)) {
         throw std::runtime_error("Could not save scene catalog");
      }
      scenes_.swap(entries);
      initialSceneName_.swap(initial);
      catalogData_ = std::move(updated);
      lastError_.clear();
      return true;
   } catch (const std::exception& error) {
      lastError_ = error.what();
      return false;
   }
}

bool SceneCatalog::CreateScene(const std::string& name) {
   // エディター外で追加された登録も取り込み、古いカタログで上書きしないようにする。
   if (!Reload()) {
      return false;
   }

   if (!ValidName(name)) {
      lastError_ = "Invalid scene name";
      return false;
   }

   for (const auto& [existing, definition] : scenes_) {
      (void)definition;

      if (Fold(existing) == Fold(name)) {
         lastError_ = "Scene already exists";
         return false;
      }
   }
   const auto relative = "game/scenes/" + name + ".json";
   const auto path = std::filesystem::path("resources") / Utf8Path(relative);
   try {
      if (std::filesystem::exists(path)) {
         throw std::runtime_error("Scene file already exists");
      }

      if (!SaveJsonFileAtomically(path, MakeEmptyScene(name), 3)) {
         throw std::runtime_error("Could not create scene file");
      }
      auto updated = catalogData_;
      updated["scenes"][name] = relative;

      if (SaveCatalog(updated)) {
         return true;
      }

      // カタログへの登録が失敗した場合は、この操作で作った空シーンだけを取り消す。
      std::error_code error;
      std::filesystem::remove(path, error);

      if (error) {
         lastError_ += "; could not remove unregistered scene: " + path.generic_string();
      }
   } catch (const std::exception& error) {
      lastError_ = error.what();
   }
   return false;
}

bool SceneCatalog::SetInitialScene(const std::string& name) {
   if (!Reload()) {
      return false;
   }

   if (!Contains(name)) {
      lastError_ = "Startup scene is not registered";
      return false;
   }
   auto updated = catalogData_;
   updated["initialScene"] = name;
   return SaveCatalog(updated);
}
}
