#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include "Model/ModelAsset.h"

namespace GameEngine {
/// @brief モデルアセットマネージャークラス
class ModelAssetManager {
public:
   /// @brief モデルアセットマネージャーの初期化
   /// @param device グラフィックスデバイス
   void Initialize(ID3D12Device* device);

   /// @brief モデルをロード
   /// @param modelPath モデルのパス
   /// @param modelName モデルの名前	
   void* LoadModel(const std::string& modelPath, const std::string& modelName);

   /// @brief モデルを取得
   /// @param modelName 取得するモデルの名前
   /// @return モデルアセットへのポインタ
   ModelAsset* GetModel(const std::string& modelName);

   /// @brief モデルアセットを全削除
   void Clear();
private:
   ID3D12Device* device_ = nullptr;
   std::unordered_map<std::string, std::unique_ptr<ModelAsset>> modelAssets_;
};
}