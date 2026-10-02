#pragma once

#include <filesystem>
#include <nlohmann/json_fwd.hpp>

namespace GameEngine {

/// @brief JSONを一時ファイルへ書き、書き込み・同期・クローズの成功後に保存先を置換する
/// @param filePath 保存先。同じディレクトリに一時ファイルを作成する
/// @param data 保存するJSON
/// @param indent JSONのインデント幅
/// @return 置換に成功した場合はtrue。失敗時は既存ファイルを保持する
bool SaveJsonFileAtomically(const std::filesystem::path& filePath, const nlohmann::json& data, int indent = 3);

} // namespace GameEngine
