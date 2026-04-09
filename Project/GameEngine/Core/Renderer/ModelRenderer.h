#pragma once
#include "DrawCommand.h"
#include <d3d12.h>

namespace GameEngine {
class GraphicsDevice;
class PSOManager;
class AssetManager;
class Material;
class LightManager;
class DirectionalLight;
class PointLight;
class SpotLight;
class AreaLight;

/// @brief 3Dモデル描画を担当するクラス
class ModelRenderer {
public:
	/// @brief 初期化
	/// @param device グラフィックスデバイス
	/// @param psoManager パイプライン状態管理
    /// @param assetManager アセット管理
	void Initialize(GraphicsDevice* device, PSOManager* psoManager, AssetManager* assetManager);

	/// @brief モデルを描画
	/// @param modelData モデル描画データ
	/// @param defaultMaterial デフォルトマテリアル
	/// @param lightManager ライトマネージャー
	/// @param setPipelineFunc パイプライン設定関数（外部から渡される）
	void DrawModel(const ModelDrawData& modelData,
		Material* defaultMaterial,
		LightManager* lightManager,
		std::function<void(const std::string&, BlendMode)> setPipelineFunc);

private:
	GraphicsDevice* device_ = nullptr;
	PSOManager* psoManager_ = nullptr;
  AssetManager* assetManager_ = nullptr;
};

} // namespace GameEngine
