#pragma once
#include "DrawCommand.h"
#include <d3d12.h>

namespace GameEngine {
class GraphicsDevice;
class PSOManager;
class Camera;
class Material;
class LightManager;
class DirectionalLight;
class PointLight;
class SpotLight;
class AreaLight;
class SpriteRenderer;

/// @brief UI要素描画を担当するクラス
class UIRenderer {
public:
	/// @brief 初期化
	/// @param device グラフィックスデバイス
	/// @param psoManager パイプライン状態管理
	/// @param uiCamera UI描画専用カメラ
	/// @param spriteRenderer スプライトレンダラー（内部で使用）
	/// @param lightManager ライトマネージャー
	void Initialize(GraphicsDevice* device, PSOManager* psoManager, Camera* uiCamera, SpriteRenderer* spriteRenderer, LightManager* lightManager);

	/// @brief UIスプライトを描画
	/// @param uiSpriteData UIスプライト描画データ
	/// @param defaultMaterial デフォルトマテリアル
	/// @param setPipelineFunc パイプライン設定関数（外部から渡される）
	void DrawUISprite(const UISpriteDrawData& uiSpriteData,
		Material* defaultMaterial,
		std::function<void(const std::string&, BlendMode)> setPipelineFunc);

	/// @brief UI描画専用カメラを設定
	/// @param camera UI描画専用カメラ
	void SetUICamera(Camera* camera) { uiCamera_ = camera; }

private:
	GraphicsDevice* device_ = nullptr;
	PSOManager* psoManager_ = nullptr;
	Camera* uiCamera_ = nullptr;
	SpriteRenderer* spriteRenderer_ = nullptr;
	LightManager* lightManager_ = nullptr;
};

} // namespace GameEngine
