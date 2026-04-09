#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include "Model/ModelAsset.h"

namespace GameEngine {
class GraphicsDevice;

/// @brief モデルアセットマネージャークラス
class ModelAssetManager {
public:
   using ModelHandle = std::shared_ptr<ModelAsset>;

   /// @brief モデルアセットマネージャーの初期化
   /// @param device グラフィックスデバイス
   void Initialize(GraphicsDevice* device);

   /// @brief モデルをロード
   /// @param modelPath モデルのパス
   /// @param modelName モデルの名前	
   ModelHandle LoadModel(const std::string& modelPath, const std::string& modelName);

   /// @brief モデルを取得
   /// @param modelName 取得するモデルの名前
   /// @return モデルアセットへのポインタ
   ModelHandle GetModel(const std::string& modelName);

   /// @brief モデルアセットを全削除
   void Clear();

   /// @brief 読み込み済みモデル名一覧を取得
   std::vector<std::string> GetModelNames() const;
private:
   GraphicsDevice* device_ = nullptr;
   std::unordered_map<std::string, ModelHandle> modelAssets_;
};
}