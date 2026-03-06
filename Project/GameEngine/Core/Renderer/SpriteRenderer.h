#pragma once
#include "DrawCommand.h"
#include <d3d12.h>

namespace GameEngine {
class GraphicsDevice;
class PSOManager;
class Material;
class LightManager;
class DirectionalLight;
class PointLight;
class SpotLight;
class AreaLight;

/// @brief 2Dスプライト描画を担当するクラス
class SpriteRenderer {
public:
	/// @brief 初期化
	/// @param device グラフィックスデバイス
	/// @param psoManager パイプライン状態管理
	void Initialize(GraphicsDevice* device, PSOManager* psoManager);

	/// @brief スプライトを描画
	/// @param spriteData スプライト描画データ
	/// @param defaultMaterial デフォルトマテリアル
	/// @param lightManager ライトマネージャー
	/// @param setPipelineFunc パイプライン設定関数（外部から渡される）
	void DrawSprite(const SpriteDrawData& spriteData,
		Material* defaultMaterial,
		LightManager* lightManager,
		std::function<void(const std::string&, BlendMode)> setPipelineFunc);

private:
	GraphicsDevice* device_ = nullptr;
	PSOManager* psoManager_ = nullptr;
};

} // namespace GameEngine
