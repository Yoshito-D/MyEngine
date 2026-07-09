#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <unordered_map>
#include <memory>

namespace GameEngine {

class PipelineState;

struct ComputePipelineDefinition {
   std::string name;
   std::string rootSignatureName;
   std::string computeShaderName;
   Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
};

class PipelineLibrary {
public:
   // @brief グラフィックスパイプラインを保存する
   // @param name パイプラインの名前
   // @param pipeline パイプラインステートのユニークポインタ
   void StoreGraphicsPipeline(const std::string& name, std::unique_ptr<PipelineState> pipeline);

   // @brief グラフィックスパイプラインを取得する
   // @param name パイプラインの名前
   // @return パイプラインステートのポインタ（存在しない場合はnullptr）
   PipelineState* GetGraphicsPipeline(const std::string& name) const;

   // @brief コンピュートパイプラインを保存する
   // @param definition コンピュートパイプラインの定義
   void StoreComputePipeline(const ComputePipelineDefinition& definition);

   // @brief コンピュートパイプラインを取得する
   // @param name コンピュートパイプラインの名前
   // @return コンピュートパイプラインの定義のポインタ（存在しない場合はnullptr）
   const ComputePipelineDefinition* GetComputePipeline(const std::string& name) const;

   // @brief パイプラインライブラリをクリアする
   void Clear();

private:
   std::unordered_map<std::string, std::unique_ptr<PipelineState>> graphicsPipelines_;
   std::unordered_map<std::string, ComputePipelineDefinition> computePipelines_;
};

}
