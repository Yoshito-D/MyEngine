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

/// @brief JSON操作で共通利用するnlohmann::jsonの別名
using json = nlohmann::json;

/// @brief 文字列キーで関連するJSON値をまとめる1グループ分のデータ
class JsonGroup {
public:
   /// @brief 空のJSONオブジェクトとしてグループを構築する
   JsonGroup() : data_(json::object()) {}
   /// @brief 既存JSONをグループの初期内容として構築する
   /// @param data 保持するJSON値
   explicit JsonGroup(const json& data) : data_(data) {}

   /// @brief C++値を指定キーへJSON変換して設定する
   /// @tparam T nlohmann::jsonへ変換可能な値型
   /// @param key 設定先のキー
   /// @param value 保存する値
   template<typename T>
   void Set(const std::string& key, const T& value) {
	  data_[key] = value;
   }

   /// @brief 指定キーのJSON値をC++値へ変換して取得する
   /// @tparam T nlohmann::jsonから変換可能な値型
   /// @param key 取得するキー
   /// @return 変換した値。キーがないか型変換できない場合はstd::nullopt
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

   /// @brief 値を取得し、存在しないか型変換できない場合は既定値を返す
   /// @tparam T nlohmann::jsonから変換可能な値型
   /// @param key 取得するキー
   /// @param defaultValue 取得できなかった場合の値
   /// @return 保存値またはdefaultValue
   template<typename T>
   T GetOr(const std::string& key, const T& defaultValue) const {
	  auto result = Get<T>(key);
	  return result.has_value() ? result.value() : defaultValue;
   }

   /// @brief 指定キーが存在するか調べる
   /// @param key 確認するキー
   /// @return キーが存在する場合はtrue
   bool Has(const std::string& key) const {
	  return data_.contains(key);
   }

   /// @brief 指定キーと値を削除する
   /// @param key 削除するキー
   /// @return キーが存在して削除された場合はtrue
   bool Remove(const std::string& key) {
	  if (!data_.contains(key)) return false;
	  data_.erase(key);
	  return true;
   }

   /// @brief グループに含まれる全キーを取得する
   /// @return JSONの反復順で並んだキー一覧
   std::vector<std::string> GetKeys() const {
	  std::vector<std::string> keys;
	  for (auto it = data_.begin(); it != data_.end(); ++it) {
		 keys.push_back(it.key());
	  }
	  return keys;
   }

   /// @brief グループ内の全キーと値を削除する
   void Clear() {
	  data_.clear();
   }

   /// @brief グループが値を保持していないか調べる
   /// @return 空の場合はtrue
   bool IsEmpty() const {
	  return data_.empty();
   }

   /// @brief グループ直下の要素数を取得する
   /// @return JSON要素数
   size_t Size() const {
	  return data_.size();
   }

   /// @brief 保持するJSONへの読み取り専用参照を取得する
   /// @return 保持中のJSON
   const json& GetJson() const { return data_; }
   /// @brief 保持するJSONへの変更可能な参照を取得する
   /// @return 保持中のJSON
   json& GetJson() { return data_; }

   /// @brief グループ全体を指定JSONで置き換える
   /// @param data 新しく保持するJSON
   void SetJson(const json& data) { data_ = data; }

   /// @brief 指定型へ変換でき、条件を満たす値のキーを検索する
   /// @tparam T 判定前にJSONから変換する値型
   /// @param predicate 変換済みの値を判定する関数
   /// @return 条件を満たしたキー一覧。変換できない要素は除外される
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

   /// @brief 別グループの要素を取り込み、同名キーを上書きする
   /// @param other 取り込むグループ
   void Merge(const JsonGroup& other) {
	  data_.update(other.data_);
   }

private:
   json data_;
};

/// @brief 複数のJsonGroupを名前で管理し、ファイル入出力を提供する
class JsonDataManager {
public:
   /// @brief グループを持たない空のマネージャーを構築する
   JsonDataManager() = default;

   /// @brief 名前に対応するグループを取得し、なければ空で作成する
   /// @param groupName 対象グループ名
   /// @return 常に有効なグループ参照
   JsonGroup& GetOrCreateGroup(const std::string& groupName) {
	  if (!groups_.contains(groupName)) {
		 groups_[groupName] = std::make_shared<JsonGroup>();
	  }
	  return *groups_[groupName];
   }

   /// @brief 既存グループを名前で取得する
   /// @param groupName 対象グループ名
   /// @return グループ参照。存在しない場合はstd::nullopt
   std::optional<std::reference_wrapper<JsonGroup>> GetGroup(const std::string& groupName) {
	  if (!groups_.contains(groupName)) return std::nullopt;
	  return std::ref(*groups_[groupName]);
   }

   /// @brief 指定名のグループが存在するか調べる
   /// @param groupName 対象グループ名
   /// @return グループが存在する場合はtrue
   bool HasGroup(const std::string& groupName) const {
	  return groups_.contains(groupName);
   }

   /// @brief 指定名のグループ全体を削除する
   /// @param groupName 削除するグループ名
   /// @return グループが存在して削除された場合はtrue
   bool RemoveGroup(const std::string& groupName) {
	  if (!groups_.contains(groupName)) return false;
	  groups_.erase(groupName);
	  return true;
   }

   /// @brief 管理している全グループ名を取得する
   /// @return グループ名一覧
   std::vector<std::string> GetGroupNames() const {
	  std::vector<std::string> names;
	  for (const auto& [name, _] : groups_) {
		 names.push_back(name);
	  }
	  return names;
   }

   /// @brief 管理している全グループを削除する
   void Clear() {
	  groups_.clear();
   }

   /// @brief JSONファイルを読み込み、現在の全グループを置き換える
   /// @param filePath 読み込むファイルパス
   /// @return ファイルを開いてJSONを反映できた場合はtrue
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

   /// @brief 現在の全グループをJSONファイルへ保存する
   /// @param filePath 保存先のファイルパス
   /// @param indent JSONのインデント幅。負数なら改行なし
   /// @return ファイルへ書き込めた場合はtrue
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

   /// @brief JSONの直下要素を名前付きグループとして読み込む
   /// @param data 読み込むJSON
   /// @return 全グループを構築できた場合はtrue
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

   /// @brief 全グループをグループ名がキーのJSONへ変換する
   /// @return 現在の全データを表すJSONオブジェクト
   json ToJson() const {
	  json result = json::object();
	  for (const auto& [name, group] : groups_) {
		 result[name] = group->GetJson();
	  }
	  return result;
   }

   /// @brief グループの作成も含めて値を直接設定する
   /// @tparam T nlohmann::jsonへ変換可能な値型
   /// @param groupName 設定先グループ名
   /// @param key 設定先キー
   /// @param value 保存する値
   template<typename T>
   void Set(const std::string& groupName, const std::string& key, const T& value) {
	  GetOrCreateGroup(groupName).Set(key, value);
   }

   /// @brief グループ名とキーを指定して値を直接取得する
   /// @tparam T nlohmann::jsonから変換可能な値型
   /// @param groupName 対象グループ名
   /// @param key 取得するキー
   /// @return 変換した値。グループやキーがないか型変換できない場合はstd::nullopt
   template<typename T>
   std::optional<T> Get(const std::string& groupName, const std::string& key) const {
	  auto it = groups_.find(groupName);
	  if (it == groups_.end()) return std::nullopt;
	  return it->second->Get<T>(key);
   }

   /// @brief 値を直接取得し、取得できない場合は既定値を返す
   /// @tparam T nlohmann::jsonから変換可能な値型
   /// @param groupName 対象グループ名
   /// @param key 取得するキー
   /// @param defaultValue 取得できなかった場合の値
   /// @return 保存値またはdefaultValue
   template<typename T>
   T GetOr(const std::string& groupName, const std::string& key, const T& defaultValue) const {
	  auto result = Get<T>(groupName, key);
	  return result.has_value() ? result.value() : defaultValue;
   }

   /// @brief 指定グループ内にキーが存在するか調べる
   /// @param groupName 対象グループ名
   /// @param key 確認するキー
   /// @return グループとキーの両方が存在する場合はtrue
   bool Has(const std::string& groupName, const std::string& key) const {
	  auto it = groups_.find(groupName);
	  if (it == groups_.end()) return false;
	  return it->second->Has(key);
   }

   /// @brief 指定グループからキーと値を削除する
   /// @param groupName 対象グループ名
   /// @param key 削除するキー
   /// @return グループとキーが存在して削除された場合はtrue
   bool Remove(const std::string& groupName, const std::string& key) {
	  auto it = groups_.find(groupName);
	  if (it == groups_.end()) return false;
	  return it->second->Remove(key);
   }

   /// @brief 別マネージャーの全グループを取り込み、同名キーを上書きする
   /// @param other 取り込むマネージャー
   void MergeGroups(const JsonDataManager& other) {
	  for (const auto& [name, otherGroup] : other.groups_) {
		 GetOrCreateGroup(name).Merge(*otherGroup);
	  }
   }

   /// @brief 条件を満たすグループ名を検索する
   /// @param predicate グループ内容を判定する関数
   /// @return 条件を満たしたグループ名一覧
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
