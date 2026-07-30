#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include "Model/ModelAsset.h"

namespace GameEngine {
class AnimationAssetManager;
class GraphicsDevice;

/// @brief モデルアセットマネージャークラス
class ModelAssetManager {
public:
   using ModelHandle = std::shared_ptr<ModelAsset>;

   /// @brief モデルアセットマネージャーの初期化
   /// @param device グラフィックスデバイス
   /// @param animationAssetManager glTFに含まれるアニメーションの登録先
   void Initialize(GraphicsDevice* device, AnimationAssetManager* animationAssetManager);

   /// @brief モデルをロード
   /// @param modelPath モデルのパス
   /// @param modelName モデルの名前	
   ModelHandle LoadModel(const std::string& modelPath, const std::string& modelName);

   /// @brief resources からの相対 assetId でモデルをロード
   /// @param assetId 例: game/models/cube/AnimatedCube.gltf
   ModelHandle LoadModelByAssetId(const std::string& assetId);

   /// @brief モデルを取得
   /// @param modelName 取得するモデルの名前
   /// @return モデルアセットへのポインタ
   ModelHandle GetModel(const std::string& modelName);

   /// @brief assetId でモデルを取得
   ModelHandle GetModelByAssetId(const std::string& assetId);

   /// @brief モデルアセットを全削除
   void Clear();

   /// @brief 読み込み済みモデル名一覧を取得
   std::vector<std::string> GetModelNames() const;
private:
   static std::string NormalizeAssetId(const std::string& path);
   static std::string BuildAssetId(const std::string& modelPath, const std::string& modelName);
   void RegisterGltfAnimation(const std::string& modelPath, const std::string& modelName);
   ModelHandle LoadModelInternal(const std::string& modelPath, const std::string& modelName, const std::string& assetId);

   GraphicsDevice* device_ = nullptr;
   AnimationAssetManager* animationAssetManager_ = nullptr;
   std::unordered_map<std::string, ModelHandle> modelAssets_;
   std::unordered_map<std::string, ModelHandle> modelAssetsById_;
};
}
