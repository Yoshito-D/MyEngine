#pragma once

#include "GameEngine/Graphics/Renderer/DrawCommand.h"
#include "GameEngine/Graphics/Pipeline/PSOManager.h"
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
class TextRenderer;
class OffscreenRenderTarget;

/// @brief 1フレーム内でRenderPassが共有するデータ
/// Renderer から各パスへ毎フレーム渡される。
/// ポインタはすべて借用（所有権なし）。
struct FrameContext {
   // ----- インフラ -----
   GraphicsDevice* device = nullptr;             ///< GPUリソースや描画コマンドの生成に使うデバイスへの参照。
   const PSOManager* psoManager = nullptr;       ///< パイプラインマネージャーを追加。
   LightManager* lightManager = nullptr;         ///< シーン内ライトとGPU転送データを管理するオブジェクト。
   PostProcessManager* postProcessMgr = nullptr; ///< ポストエフェクトの適用を委譲する管理オブジェクトへの参照。
   /// @brief 各パスの中間画像と入力スナップショットを管理する描画先。
   OffscreenRenderTarget* offscreenRenderTarget = nullptr;
   Material* defaultMaterial = nullptr; ///< 専用指定のない描画へ適用する既定マテリアル。

   // ----- 専門レンダラー -----
   ModelRenderer* modelRenderer = nullptr;       ///< モデル描画コマンドを実行するレンダラー。
   SpriteRenderer* spriteRenderer = nullptr;     ///< スプライト描画コマンドを実行するレンダラー。
   ParticleRenderer* particleRenderer = nullptr; ///< 粒子のGPU更新と描画を実行するレンダラー。
   UIRenderer* uiRenderer = nullptr;             ///< 画面上のUIスプライトを描画するレンダラー。
   TextRenderer* textRenderer = nullptr;         ///< 文字のジオメトリ生成と描画を行うレンダラー。

   // ----- コマンドキュー（レンダーパス別）-----
   std::vector<std::unique_ptr<IDrawCommand>>* opaqueCommands = nullptr;      ///< 不透明オブジェクト。
   std::vector<std::unique_ptr<IDrawCommand>>* transparentCommands = nullptr; ///< 半透明オブジェクト。
   std::vector<std::unique_ptr<IDrawCommand>>* postProcessCommands = nullptr; ///< ポストプロセス後の描画。

   // ----- パイプライン設定関数（Renderer::SetPipeline を経由させる）-----
   /// @brief 指定した名前とブレンド方式のPSOを設定する処理。
   std::function<void(const std::string&, BlendMode)> setPipelineFunc;

   // CSなどがコマンドリストのPSOを直接変更した際にRendererのキャッシュを無効化する。
   /// @brief Compute設定後などに描画PSOのキャッシュを無効にする処理。
   std::function<void()> invalidatePipelineBindingFunc;
};

} // namespace GameEngine
