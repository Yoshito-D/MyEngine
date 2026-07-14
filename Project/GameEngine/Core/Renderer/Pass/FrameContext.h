#pragma once
#include "DrawCommand.h"
#include "Pipeline/PSOManager.h"
#include <vector>
#include <memory>
#include <functional>
#include <string>

namespace GameEngine {
class GraphicsDevice;
class PSOManager;
class LightManager;
class PostProcessManager;
class Material;
class ModelRenderer;
class SpriteRenderer;
class ParticleRenderer;
class UIRenderer;
class OffscreenRenderTarget;

/// @brief 1フレーム内でRenderPassが共有するデータ
/// Renderer から各パスへ毎フレーム渡される。
/// ポインタはすべて借用（所有権なし）。
struct FrameContext {
	// ----- インフラ -----
	GraphicsDevice*      device          = nullptr;
	PSOManager*          psoManager      = nullptr;
	LightManager*        lightManager    = nullptr;
	PostProcessManager*  postProcessMgr  = nullptr;
	OffscreenRenderTarget* offscreenRenderTarget = nullptr;
	Material*            defaultMaterial = nullptr;

	// ----- 専門レンダラー -----
	ModelRenderer*    modelRenderer    = nullptr;
	SpriteRenderer*   spriteRenderer   = nullptr;
	ParticleRenderer* particleRenderer = nullptr;
	UIRenderer*       uiRenderer       = nullptr;

	// ----- コマンドキュー（レンダーパス別）-----
	std::vector<std::unique_ptr<IDrawCommand>>* opaqueCommands      = nullptr;
	std::vector<std::unique_ptr<IDrawCommand>>* transparentCommands = nullptr;
	std::vector<std::unique_ptr<IDrawCommand>>* postProcessCommands = nullptr;

	// ----- パイプライン設定関数（Renderer::SetPipeline を経由させる）-----
	std::function<void(const std::string&, BlendMode)> setPipelineFunc;
	// CSなどがコマンドリストのPSOを直接変更した際にRendererのキャッシュを無効化する。
	std::function<void()> invalidatePipelineBindingFunc;
};

} // namespace GameEngine
