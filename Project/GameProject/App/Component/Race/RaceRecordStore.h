#pragma once
#include <string>
#include <vector>
#include <cstddef>

namespace App {
/// @brief レース記録の順位付けとファイル保存を担当する。保存先とコースキーは呼び出し側が指定する。
class RaceRecordStore {
public:
   static constexpr size_t kBestTimeCount = 3; ///< 保存・表示する記録の最大件数。
   /// @brief 正の有限タイムだけを追加し、速い順に最大件数へ制限する。
   void AddBestTime(float timeSeconds);
   /// @brief 指定したファイルとコースの記録を読み込み、失敗時は空にする。
   bool LoadBestTimes(const std::string& file, const std::string& key);
   /// @brief 他コースを保持しながら一時ファイルを完成させて置換する。保存失敗時はfalse。
   bool SaveBestTimes(const std::string& file, const std::string& key) const;

   /// @brief 正規化済みのタイムを速い順に返す。
   const std::vector<float>& GetBestTimes() const { return bestTimes_; }

private:
   static void LogRecordWarning(const std::string& file, const std::string& message);
   std::vector<float> bestTimes_;
};
}
