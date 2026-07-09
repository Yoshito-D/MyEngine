#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "ShaderManager.h"

namespace GameEngine {

class BindingLayoutResolver {
public:
   /// @brief ルートシグネチャ名に対応する期待されるセマンティクスのリストを取得する
   /// @param rootSignatureName ルートシグネチャ名
   /// @return 固定フォールバックを使わないため空のリスト
   std::vector<std::string> GetExpectedSemanticsForRootSignature(const std::string& rootSignatureName) const;

   /// @brief 固定ルートパラメータテーブルを作らず、出力先をクリアする
   /// @param output 出力先のマップ
   /// @param getReflection 互換用の反射取得関数
   void BuildPipelineRootParameterTables(
      std::unordered_map<std::string, PipelineRootParameterTable>& output,
      const std::function<const ShaderReflectionInfo*(const std::string&, ShaderType)>& getReflection) const;
};

}
