#include "GameEngine/Editor/EditorAssetRegistry.h"

#ifdef USE_IMGUI

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>
#include <system_error>

namespace GameEngine {

namespace {
std::filesystem::path Utf8Path(const std::string& value) {
   return std::filesystem::path(std::u8string(value.begin(), value.end()));
}

std::string ToGenericString(const std::filesystem::path& path) {
   const auto value = path.lexically_normal().generic_u8string();
   return { value.begin(), value.end() };
}

std::string ToLower(std::string value) {
   std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
      return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : static_cast<char>(c);
   });
   return value;
}

bool IsValidFileName(const std::string& name) {
   if (name.empty() || name == "." || name == ".." || name.back() == '.' || name.back() == ' ') return false;
   if (std::any_of(name.begin(), name.end(), [](unsigned char c) {
      return c < 32 || c == 127 || std::strchr("<>:\"/\\|?*", c) != nullptr;
   })) return false;
   const std::string stem = ToLower(name.substr(0, name.find('.')));
   if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul") return false;
   if (stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) && stem[3] >= '1' && stem[3] <= '9') return false;
   return true;
}

bool IsWithinRoot(const std::filesystem::path& relative) {
   if (relative.empty() || relative.is_absolute() || relative.has_root_name()) return false;
   return std::none_of(relative.begin(), relative.end(), [](const auto& part) { return part == ".."; });
}

bool ValidateSource(const EditorAssetEntry& asset, const std::filesystem::path& root, std::string& error) {
   const std::string id = ToLower(asset.assetId);
   if (id == "engine" || id.starts_with("engine/")) {
      error = "Engine resources use built-in names and cannot be changed from Project.";
      return false;
   }
   std::error_code operationError;
   const auto status = std::filesystem::symlink_status(asset.filePath, operationError);
   const bool expectedKind = asset.type == EditorAssetType::Folder
      ? std::filesystem::is_directory(status) : std::filesystem::is_regular_file(status);
   if (operationError || std::filesystem::is_symlink(status) || !expectedKind ||
      EditorAssetRegistry::NormalizeAssetId(asset.filePath, root) != asset.assetId) {
      error = "The source changed, became a link, or resolves outside Resources. Rescan Project.";
      return false;
   }
   return true;
}

void CollectReferences(const nlohmann::json& value, const std::vector<std::string>& names,
   const std::string& location, std::vector<std::string>& references) {
   if (value.is_string()) {
      std::string text = ToLower(value.get<std::string>());
      std::replace(text.begin(), text.end(), '\\', '/');
      if (std::find(names.begin(), names.end(), text) != names.end()) references.push_back(location);
      return;
   }
   // JSONを読むだけの保守的な検出。未管理の形式も検出し、変更不可にして破損を防ぐ。
   if (value.is_object()) {
      for (auto it = value.begin(); it != value.end(); ++it) {
         CollectReferences(it.value(), names, location + "/" + it.key(), references);
      }
   } else if (value.is_array()) {
      for (size_t i = 0; i < value.size(); ++i) {
         CollectReferences(value[i], names, location + "/" + std::to_string(i), references);
      }
   }
}
} // namespace

void EditorAssetRegistry::Scan(const std::filesystem::path& resourcesRoot) {
   resourcesRoot_ = resourcesRoot;
   allAssets_.clear();
   children_.clear();
   scanError_.clear();
   // 同じシーンを再生成しても、旧RegistryのUIキャッシュと世代が衝突しない。
   static std::atomic<uint64_t> nextRevision{ 0 };
   revision_ = nextRevision.fetch_add(1, std::memory_order_relaxed) + 1;
   std::error_code error;
   if (!std::filesystem::is_directory(resourcesRoot_, error)) {
      scanError_ = error ? error.message() : "Resources folder does not exist.";
      return;
   }
   std::filesystem::recursive_directory_iterator it(resourcesRoot_, error), end;
   while (!error && it != end) {
      const auto path = it->path();
      const auto status = it->symlink_status(error);
      if (error) break;
      if (std::filesystem::is_symlink(status)) {
         // リンク先がルート外でもブラウザーの操作対象にならないよう除外する。
         it.disable_recursion_pending();
      } else if (std::filesystem::is_directory(status) || std::filesystem::is_regular_file(status)) {
         EditorAssetEntry entry{};
         entry.type = std::filesystem::is_directory(status) ? EditorAssetType::Folder : ClassifyAsset(path, resourcesRoot_);
         entry.assetId = NormalizeAssetId(path, resourcesRoot_);
         if (entry.assetId.empty() && entry.type == EditorAssetType::Folder) it.disable_recursion_pending();
         if (entry.type != EditorAssetType::Unknown && !entry.assetId.empty()) {
            entry.displayName = ToGenericString(entry.type == EditorAssetType::Folder ? path.filename() : path.stem());
            if (entry.displayName.empty()) entry.displayName = entry.assetId;
            entry.filePath = path;
            allAssets_.push_back(std::move(entry));
         }
      }
      it.increment(error);
   }
   if (error) scanError_ = error.message();
   std::sort(allAssets_.begin(), allAssets_.end(), [](const auto& lhs, const auto& rhs) {
      return lhs.assetId < rhs.assetId;
   });
   for (size_t i = 0; i < allAssets_.size(); ++i) {
      const std::string parent = ToGenericString(Utf8Path(allAssets_[i].assetId).parent_path());
      children_[parent].push_back(i);
   }
   for (auto& [folderId, indices] : children_) {
      (void)folderId;
      std::stable_sort(indices.begin(), indices.end(), [this](size_t lhs, size_t rhs) {
         return allAssets_[lhs].type == EditorAssetType::Folder && allAssets_[rhs].type != EditorAssetType::Folder;
      });
   }
}

const std::vector<size_t>& EditorAssetRegistry::GetChildren(const std::string& folderId) const {
   static const std::vector<size_t> empty;
   auto found = children_.find(folderId);
   return found == children_.end() ? empty : found->second;
}

const EditorAssetEntry* EditorAssetRegistry::FindAsset(const std::string& assetId) const {
   auto found = std::lower_bound(allAssets_.begin(), allAssets_.end(), assetId,
      [](const auto& entry, const auto& id) { return entry.assetId < id; });
   return found == allAssets_.end() || found->assetId != assetId ? nullptr : &(*found);
}

const EditorAssetEntry* EditorAssetRegistry::FindAsset(const std::string& assetId, EditorAssetType expectedType) const {
   const auto* entry = FindAsset(assetId);
   return entry && (expectedType == EditorAssetType::Unknown || entry->type == expectedType) ? entry : nullptr;
}

const EditorAssetEntry* EditorAssetRegistry::ResolveAssetPayload(const void* data, size_t size, EditorAssetType expectedType) const {
   if (!data || size < 2 || size > 4096) return nullptr;
   const auto* text = static_cast<const char*>(data);
   if (text[size - 1] != '\0' || std::memchr(text, '\0', size - 1)) return nullptr;
   return FindAsset(std::string(text, size - 1), expectedType);
}

std::vector<std::string> EditorAssetRegistry::FindReferences(const std::string& assetId) const {
   std::vector<std::string> references;
   const auto* asset = FindAsset(assetId);
   if (!asset) return references;
   std::vector<std::string> names{ ToLower(assetId), ToLower("resources/" + assetId) };
   if (asset->type == EditorAssetType::Texture || asset->type == EditorAssetType::Model || asset->type == EditorAssetType::Scene) {
      names.push_back(ToLower(ToGenericString(Utf8Path(assetId).stem())));
   }
   if (asset->type == EditorAssetType::Texture) names.push_back(ToLower(ToGenericString(Utf8Path(assetId).filename())));
   std::error_code error;
   std::filesystem::recursive_directory_iterator it(resourcesRoot_, error), end;
   while (!error && it != end) {
      const auto status = it->symlink_status(error);
      if (error) break;
      if (std::filesystem::is_symlink(status) || NormalizeAssetId(it->path(), resourcesRoot_).empty()) {
         it.disable_recursion_pending();
      } else if (std::filesystem::is_regular_file(status)) {
         const std::string extension = ToLower(it->path().extension().string());
         const std::string fileId = NormalizeAssetId(it->path(), resourcesRoot_);
         if (asset->type == EditorAssetType::Texture && extension == ".fbx") {
            references.push_back(fileId + " (opaque model dependencies cannot be verified)");
         } else if (asset->type == EditorAssetType::Texture && extension == ".mtl") {
            std::ifstream file(it->path(), std::ios::binary);
            if (!file) references.push_back(fileId + " (unreadable material dependencies)");
            else {
               std::string text{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
               text = ToLower(std::move(text));
               std::replace(text.begin(), text.end(), '\\', '/');
               const std::string filename = ToLower(ToGenericString(Utf8Path(assetId).filename()));
               if (text.find(filename) != std::string::npos || text.find(ToLower(assetId)) != std::string::npos) {
                  references.push_back(fileId + " (external material reference)");
               }
            }
         } else if (extension == ".json" || extension == ".gltf") {
            std::ifstream file(it->path(), std::ios::binary);
            const auto json = file ? nlohmann::json::parse(file, nullptr, false) : nlohmann::json(nlohmann::json::value_t::discarded);
            if (json.is_discarded()) references.push_back(fileId + " (unreadable JSON; references cannot be verified)");
            else CollectReferences(json, names, fileId, references);
         }
      }
      it.increment(error);
   }
   if (error) references.push_back("Resources scan failed: " + error.message());
   return references;
}

bool EditorAssetRegistry::CanRenameOrMove(const std::string& assetId, std::string& reason) const {
   reason.clear();
   const auto* asset = FindAsset(assetId);
   if (!asset) { reason = "The asset no longer exists. Rescan Project."; return false; }
   if (!ValidateSource(*asset, resourcesRoot_, reason)) return false;
   if (asset->type == EditorAssetType::Folder) {
      std::error_code error;
      if (!std::filesystem::is_empty(asset->filePath, error) || error) {
         reason = "Only empty folders can be moved: nested references and external dependencies cannot be updated safely.";
         return false;
      }
      return true;
   }
   if (asset->type != EditorAssetType::Texture && asset->type != EditorAssetType::Audio && asset->type != EditorAssetType::Particle) {
      reason = "This format has scene-name, external-file, or opaque references that cannot be updated safely.";
      return false;
   }
   const auto references = FindReferences(assetId);
   if (!references.empty()) {
      reason = "This path-based ID is referenced by " + references.front() + ". Updating all references is not supported.";
      return false;
   }
   return true;
}

bool EditorAssetRegistry::CanRemove(const std::string& assetId, std::string& reason) const {
   reason.clear();
   const auto* asset = FindAsset(assetId);
   if (!asset) { reason = "The asset no longer exists. Rescan Project."; return false; }
   if (!ValidateSource(*asset, resourcesRoot_, reason)) return false;
   if (asset->type == EditorAssetType::Scene || asset->type == EditorAssetType::Json) {
      reason = "Scene catalog and opaque configuration references cannot be updated safely by deleting this file.";
      return false;
   }
   std::error_code error;
   if (asset->type == EditorAssetType::Folder && (!std::filesystem::is_empty(asset->filePath, error) || error)) {
      reason = "Only empty folders can be deleted. Remove their contents individually after confirming references.";
      return false;
   }
   return true;
}

bool EditorAssetRegistry::CanDuplicate(const std::string& assetId, std::string& reason) const {
   reason.clear();
   const auto* asset = FindAsset(assetId);
   if (!asset) { reason = "The asset no longer exists. Rescan Project."; return false; }
   if (!ValidateSource(*asset, resourcesRoot_, reason)) return false;
   if (asset->type == EditorAssetType::Scene) {
      reason = "Scene duplication requires catalog registration. Use New Scene from the scene menu.";
      return false;
   }
   if (asset->type == EditorAssetType::Folder || asset->type == EditorAssetType::Model) {
      reason = "Folder and model duplication requires copying dependent files; that operation is not supported.";
      return false;
   }
   return true;
}

bool EditorAssetRegistry::ResolveDestination(const std::string& assetId, std::filesystem::path& path, std::string& error) const {
   error.clear();
   const std::string foldedId = ToLower(assetId);
   if (foldedId == "engine" || foldedId.starts_with("engine/")) {
      error = "Engine resource folders use built-in names and cannot be modified.";
      return false;
   }
   std::filesystem::path relative;
   try {
      relative = Utf8Path(assetId);
   } catch (const std::system_error&) {
      error = "The destination contains invalid UTF-8 text.";
      return false;
   }
   if (!IsWithinRoot(relative) || ToGenericString(relative) != assetId) {
      error = "Use a normalized relative path inside Resources.";
      return false;
   }
   for (const auto& part : relative) {
      if (!IsValidName(ToGenericString(part))) { error = "The name contains invalid or reserved characters."; return false; }
   }
   path = resourcesRoot_ / relative;
   if (NormalizeAssetId(path, resourcesRoot_) != assetId) {
      error = "The destination resolves outside Resources.";
      return false;
   }
   std::error_code operationError;
   if (std::filesystem::exists(path, operationError) || operationError) {
      error = operationError ? operationError.message() : "An asset with that name already exists.";
      return false;
   }
   if (!std::filesystem::is_directory(path.parent_path(), operationError) || operationError) {
      error = "The destination folder does not exist.";
      return false;
   }
   return true;
}

bool EditorAssetRegistry::Duplicate(const std::string& assetId, std::string& newId, std::string& error) {
   newId.clear();
   if (!CanDuplicate(assetId, error)) return false;
   const auto* asset = FindAsset(assetId);
   const auto source = asset->filePath;
   const auto relative = Utf8Path(assetId);
   for (size_t suffix = 1; suffix < 10000; ++suffix) {
      const std::string name = ToGenericString(relative.stem()) + " Copy" +
         (suffix == 1 ? "" : " " + std::to_string(suffix)) + ToGenericString(relative.extension());
      const std::string candidate = ToGenericString(relative.parent_path() / Utf8Path(name));
      std::filesystem::path destination;
      std::error_code operationError;
      if (std::filesystem::exists(resourcesRoot_ / Utf8Path(candidate), operationError) && !operationError) continue;
      if (!ResolveDestination(candidate, destination, error)) return false;
      if (!std::filesystem::copy_file(source, destination, std::filesystem::copy_options::none, operationError)) {
         error = operationError ? operationError.message() : "The asset could not be copied.";
         return false;
      }
      newId = candidate;
      Scan(resourcesRoot_);
      return true;
   }
   error = "No unused copy name is available.";
   return false;
}

bool EditorAssetRegistry::RenameOrMove(const std::string& assetId, const std::string& newId, std::string& error) {
   if (!CanRenameOrMove(assetId, error)) return false;
   const auto* asset = FindAsset(assetId);
   if (assetId == newId) { error = "The destination is unchanged."; return false; }
   std::filesystem::path destination;
   if (!ResolveDestination(newId, destination, error)) return false;
   if (asset->type != EditorAssetType::Folder &&
      (ToLower(asset->filePath.extension().string()) != ToLower(destination.extension().string()) ||
         ClassifyAsset(destination, resourcesRoot_) != asset->type)) {
      error = "A move must preserve the extension and the asset's supported resource folder/type.";
      return false;
   }
   std::error_code operationError;
   std::filesystem::rename(asset->filePath, destination, operationError);
   if (operationError) { error = operationError.message(); return false; }
   Scan(resourcesRoot_);
   return true;
}

bool EditorAssetRegistry::Remove(const std::string& assetId, std::string& error) {
   if (!CanRemove(assetId, error)) return false;
   const auto* asset = FindAsset(assetId);
   std::error_code operationError;
   if (!std::filesystem::remove(asset->filePath, operationError)) {
      error = operationError ? operationError.message() : "The asset could not be removed.";
      return false;
   }
   Scan(resourcesRoot_);
   return true;
}

bool EditorAssetRegistry::CreateFolder(const std::string& parentId, const std::string& name,
   std::string& newId, std::string& error) {
   newId.clear();
   if (!IsValidName(name)) { error = "The folder name is invalid or reserved."; return false; }
   if (!parentId.empty() && !FindAsset(parentId, EditorAssetType::Folder)) {
      error = "The parent folder no longer exists.";
      return false;
   }
   const std::string foldedParent = ToLower(parentId);
   if (foldedParent == "engine" || foldedParent.starts_with("engine/")) {
      error = "Engine resource folders cannot be changed from Project.";
      return false;
   }
   std::string candidate;
   try {
      candidate = ToGenericString(Utf8Path(parentId) / Utf8Path(name));
   } catch (const std::system_error&) {
      error = "The folder name contains invalid UTF-8 text.";
      return false;
   }
   std::filesystem::path destination;
   if (!ResolveDestination(candidate, destination, error)) return false;
   std::error_code operationError;
   if (!std::filesystem::create_directory(destination, operationError)) {
      error = operationError ? operationError.message() : "The folder could not be created.";
      return false;
   }
   newId = candidate;
   Scan(resourcesRoot_);
   return true;
}

std::string EditorAssetRegistry::NormalizeAssetId(const std::filesystem::path& path, const std::filesystem::path& resourcesRoot) {
   std::error_code error;
   const auto root = std::filesystem::weakly_canonical(resourcesRoot, error);
   if (error) return {};
   const auto absolute = std::filesystem::weakly_canonical(path, error);
   if (error) return {};
   const auto relative = absolute.lexically_relative(root);
   return IsWithinRoot(relative) && relative != "." ? ToGenericString(relative) : std::string{};
}

bool EditorAssetRegistry::IsValidName(const std::string& name) {
   if (!IsValidFileName(name)) return false;
   try {
      (void)Utf8Path(name);
      return true;
   } catch (const std::system_error&) {
      return false;
   }
}

const char* EditorAssetRegistry::GetAssetTypeLabel(EditorAssetType type) {
   switch (type) {
      case EditorAssetType::Folder: return "Folder";
      case EditorAssetType::Model: return "Model";
      case EditorAssetType::Texture: return "Texture";
      case EditorAssetType::Audio: return "Audio";
      case EditorAssetType::Particle: return "Particle";
      case EditorAssetType::Scene: return "Scene";
      case EditorAssetType::Material: return "Material";
      case EditorAssetType::Prefab: return "Prefab";
      case EditorAssetType::Json: return "Json";
      default: return "Unknown";
   }
}

EditorAssetType EditorAssetRegistry::ClassifyAsset(const std::filesystem::path& path, const std::filesystem::path& resourcesRoot) {
   const std::string extension = ToLower(path.extension().string());
   if (extension == ".obj" || extension == ".gltf" || extension == ".fbx") return EditorAssetType::Model;
   if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".dds") return EditorAssetType::Texture;
   if (extension == ".wav" || extension == ".mp3") return EditorAssetType::Audio;
   if (extension != ".json") return EditorAssetType::Unknown;
   const auto relative = Utf8Path(NormalizeAssetId(path, resourcesRoot));
   auto folder = relative.begin();
   if (folder != relative.end()) {
      std::string category = ToLower(ToGenericString(*folder));
      if (category == "game" && ++folder != relative.end()) category = ToLower(ToGenericString(*folder));
      if (category == "particles") return EditorAssetType::Particle;
      if (category == "scenes") return EditorAssetType::Scene;
      if (category == "materials") return EditorAssetType::Material;
      if (category == "prefabs") return EditorAssetType::Prefab;
   }
   return EditorAssetType::Json;
}

} // namespace GameEngine

#endif
