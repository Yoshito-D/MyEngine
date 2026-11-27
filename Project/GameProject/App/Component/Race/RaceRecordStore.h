#pragma once
#include <string>
#include <vector>
#include <cstddef>

namespace App {
/// @brief レース記録の順位付けとファイル保存を担当する。保存先とコースキーは呼び出し側が指定する。
class RaceRecordStore {
public:
   static constexpr size_t kBestTimeCount = 3; ///< 保存・表示する記録の最大件数。

   /// @brief 正の有限タイムを追加する。保存を有効にしている場合は、保存できた順位を反映する。
   /// @param timeSeconds 計測した走行時間（秒）。
   /// @param file 記録ファイルのパス。
   /// @param key コースの識別子。空の場合は実行中の順位だけを更新する。
   /// @return 有効なタイムを順位へ反映できた場合はtrue。保存失敗時は既存の順位を維持する。
   bool AddBestTime(float timeSeconds, const std::string& file, const std::string& key);

   /// @brief ファイルからコースの記録を読み込む。ファイルまたはコースが存在しない場合は空の順位にする。
   /// @param file 記録ファイルのパス。
   /// @param key コースの識別子。空の場合は記録を読み込まない。
   /// @return 読み込みに成功した場合はtrue。失敗した場合は既存の順位を維持する。
   bool LoadBestTimes(const std::string& file, const std::string& key);

   /// @brief 正規化済みのタイムを速い順に返す。
   const std::vector<float>& GetBestTimes() const { return bestTimes_; }

private:
   static void LogRecordError(const std::string& file, const std::string& message);
   std::vector<float> bestTimes_;
};
}
