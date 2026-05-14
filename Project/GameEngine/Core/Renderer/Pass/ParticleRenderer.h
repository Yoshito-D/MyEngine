#pragma once
#include "DrawCommand.h"
#include <d3d12.h>

namespace GameEngine {
class GraphicsDevice;
class PSOManager;

/// @brief パーティクルシステム描画を担当するクラス
class ParticleRenderer {
public:
	/// @brief 初期化
	/// @param device グラフィックスデバイス
	/// @param psoManager パイプライン状態管理
	void Initialize(GraphicsDevice* device, PSOManager* psoManager);

	/// @brief パーティクルを描画
	/// @param particleData パーティクル描画データ
	/// @param setPipelineFunc パイプライン設定関数（外部から渡される）
	void DrawParticle(const ParticleDrawData& particleData,
		std::function<void(const std::string&, BlendMode)> setPipelineFunc);

private:
	GraphicsDevice* device_ = nullptr;
	PSOManager* psoManager_ = nullptr;
};

} // namespace GameEngine
