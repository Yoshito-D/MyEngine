#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <unordered_map>
#include <memory>

namespace GameEngine {

class PipelineState;

/// @brief コンピュートパイプラインの識別情報と生成済みPSOをまとめた定義
struct ComputePipelineDefinition {
   std::string name;                                             ///< パイプラインの一意名
   std::string rootSignatureName;                                ///< 使用するルートシグネチャ名
   std::string computeShaderName;                                ///< 使用するコンピュートシェーダー名
   Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;    ///< 生成済みパイプラインステート
};

/// @brief 名前からグラフィックス・コンピュートPSOを参照する所有コンテナ
class PipelineLibrary {
public:
   /// @brief グラフィックスパイプラインを保存する
   /// @param name パイプラインの名前
   /// @param pipeline パイプラインステートのユニークポインタ
   void StoreGraphicsPipeline(const std::string& name, std::unique_ptr<PipelineState> pipeline);

   /// @brief グラフィックスパイプラインを取得する
   /// @param name パイプラインの名前
   /// @return パイプラインステートのポインタ（存在しない場合はnullptr）
   PipelineState* GetGraphicsPipeline(const std::string& name) const;

   /// @brief コンピュートパイプラインを保存する
   /// @param definition コンピュートパイプラインの定義
   void StoreComputePipeline(const ComputePipelineDefinition& definition);

   /// @brief コンピュートパイプラインを取得する
   /// @param name コンピュートパイプラインの名前
   /// @return コンピュートパイプラインの定義のポインタ（存在しない場合はnullptr）
   const ComputePipelineDefinition* GetComputePipeline(const std::string& name) const;

   /// @brief 所有する全パイプラインを解放する
   void Clear();

private:
   std::unordered_map<std::string, std::unique_ptr<PipelineState>> graphicsPipelines_;
   std::unordered_map<std::string, ComputePipelineDefinition> computePipelines_;
};

}
