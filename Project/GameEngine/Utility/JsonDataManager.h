#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <optional>
#include <functional>
#include <fstream>
#include <vector>
#include <memory>
#include "VectorMath.h"

namespace GameEngine {

using json = nlohmann::json;

// グループ単位で JSON データを管理するクラス
class JsonGroup {
public:
   JsonGroup() : data_(json::object()) {}
   explicit JsonGroup(const json& data) : data_(data) {}

   // 値の設定（型安全）
   template<typename T>
   void Set(const std::string& key, const T& value) {
	  data_[key] = value;
   }

   // 値の取得（型安全、存在しない場合は std::nullopt）
   template<typename T>
   std::optional<T> Get(const std::string& key) const {
	  if (!data_.contains(key)) return std::nullopt;
	  try {
		 return data_.at(key).get<T>();
	  }
	  catch (...) {
		 return std::nullopt;
	  }
   }

   // デフォルト値付き取得
   template<typename T>
   T GetOr(const std::string& key, const T& defaultValue) const {
	  auto result = Get<T>(key);
	  return result.has_value() ? result.value() : defaultValue;
   }

   // キーの存在確認
   bool Has(const std::string& key) const {
	  return data_.contains(key);
   }

   // キーの削除
   bool Remove(const std::string& key) {
	  if (!data_.contains(key)) return false;
	  data_.erase(key);
	  return true;
   }

   // すべてのキーを取得
   std::vector<std::string> GetKeys() const {
	  std::vector<std::string> keys;
	  for (auto it = data_.begin(); it != data_.end(); ++it) {
		 keys.push_back(it.key());
	  }
	  return keys;
   }

   // データのクリア
   void Clear() {
	  data_.clear();
   }

   // データが空かチェック
   bool IsEmpty() const {
	  return data_.empty();
   }

   // データサイズ
   size_t Size() const {
	  return data_.size();
   }

   // JSON データを直接取得
   const json& GetJson() const { return data_; }
   json& GetJson() { return data_; }

   // JSON データを設定
   void SetJson(const json& data) { data_ = data; }

   // 条件付き検索
   template<typename T>
   std::vector<std::string> FindKeys(std::function<bool(const T&)> predicate) const {
	  std::vector<std::string> result;
	  for (auto it = data_.begin(); it != data_.end(); ++it) {
		 try {
			T value = it.value().get<T>();
			if (predicate(value)) {
			   result.push_back(it.key());
			}
		 }
		 catch (...) {
			continue;
		 }
	  }
	  return result;
   }

   // マージ（既存のキーは上書きされる）
   void Merge(const JsonGroup& other) {
	  data_.update(other.data_);
   }

private:
   json data_;
};

// グループ単位で管理する JSON データマネージャー
class JsonDataManager {
public:
   JsonDataManager() = default;

   // グループの作成または取得
   JsonGroup& GetOrCreateGroup(const std::string& groupName) {
	  if (!groups_.contains(groupName)) {
		 groups_[groupName] = std::make_shared<JsonGroup>();
	  }
	  return *groups_[groupName];
   }

   // グループの取得（存在しない場合は nullopt）
   std::optional<std::reference_wrapper<JsonGroup>> GetGroup(const std::string& groupName) {
	  if (!groups_.contains(groupName)) return std::nullopt;
	  return std::ref(*groups_[groupName]);
   }

   // グループの存在確認
   bool HasGroup(const std::string& groupName) const {
	  return groups_.contains(groupName);
   }

   // グループの削除
   bool RemoveGroup(const std::string& groupName) {
	  if (!groups_.contains(groupName)) return false;
	  groups_.erase(groupName);
	  return true;
   }

   // すべてのグループ名を取得
   std::vector<std::string> GetGroupNames() const {
	  std::vector<std::string> names;
	  for (const auto& [name, _] : groups_) {
		 names.push_back(name);
	  }
	  return names;
   }

   // 全データのクリア
   void Clear() {
	  groups_.clear();
   }

   // ファイルから読み込み
   bool LoadFromFile(const std::string& filePath) {
	  std::ifstream ifs(filePath);
	  if (!ifs.is_open()) return false;
	  try {
		 json data;
		 ifs >> data;
		 return LoadFromJson(data);
	  }
	  catch (...) {
		 return false;
	  }
   }

   // ファイルへ保存
   bool SaveToFile(const std::string& filePath, int indent = 4) const {
	  std::ofstream ofs(filePath);
	  if (!ofs.is_open()) return false;
	  try {
		 json data = ToJson();
		 ofs << data.dump(indent);
		 return true;
	  }
	  catch (...) {
		 return false;
	  }
   }

   // JSON から読み込み
   bool LoadFromJson(const json& data) {
	  try {
		 groups_.clear();
		 for (auto it = data.begin(); it != data.end(); ++it) {
			groups_[it.key()] = std::make_shared<JsonGroup>(it.value());
		 }
		 return true;
	  }
	  catch (...) {
		 return false;
	  }
   }

   // JSON へ変換
   json ToJson() const {
	  json result = json::object();
	  for (const auto& [name, group] : groups_) {
		 result[name] = group->GetJson();
	  }
	  return result;
   }

   // 便利メソッド：グループ内の値を直接設定
   template<typename T>
   void Set(const std::string& groupName, const std::string& key, const T& value) {
	  GetOrCreateGroup(groupName).Set(key, value);
   }

   // 便利メソッド：グループ内の値を直接取得
   template<typename T>
   std::optional<T> Get(const std::string& groupName, const std::string& key) const {
	  auto it = groups_.find(groupName);
	  if (it == groups_.end()) return std::nullopt;
	  return it->second->Get<T>(key);
   }

   // 便利メソッド：デフォルト値付き取得
   template<typename T>
   T GetOr(const std::string& groupName, const std::string& key, const T& defaultValue) const {
	  auto result = Get<T>(groupName, key);
	  return result.has_value() ? result.value() : defaultValue;
   }

   // 便利メソッド：キーの存在確認
   bool Has(const std::string& groupName, const std::string& key) const {
	  auto it = groups_.find(groupName);
	  if (it == groups_.end()) return false;
	  return it->second->Has(key);
   }

   // 便利メソッド：キーの削除
   bool Remove(const std::string& groupName, const std::string& key) {
	  auto it = groups_.find(groupName);
	  if (it == groups_.end()) return false;
	  return it->second->Remove(key);
   }

   // グループ全体をマージ
   void MergeGroups(const JsonDataManager& other) {
	  for (const auto& [name, otherGroup] : other.groups_) {
		 GetOrCreateGroup(name).Merge(*otherGroup);
	  }
   }

   // 条件に合うグループを検索
   std::vector<std::string> FindGroups(std::function<bool(const JsonGroup&)> predicate) const {
	  std::vector<std::string> result;
	  for (const auto& [name, group] : groups_) {
		 if (predicate(*group)) {
			result.push_back(name);
		 }
	  }
	  return result;
   }

private:
   std::unordered_map<std::string, std::shared_ptr<JsonGroup>> groups_;
};

} // namespace GameEngine
