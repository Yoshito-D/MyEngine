#pragma once

#include "GameEngine/Graphics/Renderer/DrawCommand.h"
#include "GameEngine/Window/Window.h"
#include <span>

namespace GameEngine {
class PSOManager;
class RenderQueue;

/// @brief 描画要求の生成に必要な登録先と、Facadeが保持する現在の描画設定。
struct DrawSubmissionContext {
   RenderQueue& queue;                               ///< 生成したコマンドの所有先。
   BlendMode defaultBlendMode;                       ///< 引数とマテリアルが未指定の場合のブレンド。
   D3D12_GPU_DESCRIPTOR_HANDLE environmentTexture{}; ///< モデルへ適用する環境テクスチャのSRV。
};

/// @brief モデルのコマンド生成へ同期的に渡す入力。texturesの配列はSubmit中だけ有効ならよい。
struct ModelDrawRequest {
   Model& model;                       ///< 描画対象。生成したコマンドの実行完了まで生存させる。
   std::span<Texture* const> textures; ///< メッシュ順の入力テクスチャ。SRVは生成時に値へ変換する。
   Camera& camera;                     ///< 行列更新と描画に使うカメラ。
   std::optional<BlendMode> blendMode; ///< マテリアルに指定がない場合のブレンド。
   bool applyPostProcess = true;       ///< ポストプロセス前に描画するか。
};

/// @brief ワールド空間スプライトの描画要求。
struct SpriteDrawRequest {
   Sprite& sprite;                     ///< 描画対象への参照。
   Texture& texture;                   ///< 描画するテクスチャ。
   Camera& camera;                     ///< 行列更新と描画に使うカメラ。
   std::optional<BlendMode> blendMode; ///< マテリアル未指定時のブレンド。
   bool applyPostProcess = true;       ///< ポストプロセス前に描画するか。
};

/// @brief 粒子のGPU更新と描画の要求。
struct ParticleDrawRequest {
   ParticleSystem& particleSystem; ///< CPU粒子と前フレームのGPU状態を確認する対象。
   Camera& camera;                 ///< 行列更新と描画に使うカメラ。
};

/// @brief UIスプライトの配置条件。画面寸法とviewportSizeの単位はピクセル。
struct UIDrawRequest {
   Sprite& sprite;                                                 ///< 描画対象への参照。
   Texture& texture;                                               ///< 描画するテクスチャ。
   Camera& camera;                                                 ///< UI専用の正射影カメラ。
   Vector2 viewportSize;                                           ///< 現在の論理ビューポート寸法。
   Sprite::AnchorPoint anchorPoint = Sprite::AnchorPoint::TopLeft; ///< 画面上の基準位置。
   std::optional<BlendMode> blendMode;                             ///< 未指定時はFacadeのブレンドを使用する。
   bool applyPostProcess = true;                                   ///< ポストプロセス前に描画するか。
   std::uint32_t screenWidth = Window::kUiReferenceWidth;          ///< UI配置の基準幅。
   std::uint32_t screenHeight = Window::kUiReferenceHeight;        ///< UI配置の基準高さ。
};

/// @brief テクスチャ数に共通の準備を行い、サブメッシュ単位のモデルコマンドを生成する。
class ModelCommandFactory {
public:
   /// @brief メッシュ・行列・スロットごとのブレンドを解決してキューへ登録する。
   /// @param request 同期的に消費するモデル・テクスチャ・カメラの要求。
   /// @param context コマンドの所有先と既定の描画設定。
   /// @param pipelines スロットのパイプライン契約とブレンド方式の解決先。
   static void Submit(
      const ModelDrawRequest& request, const DrawSubmissionContext& context, const PSOManager& pipelines);
};

/// @brief スプライトの行列とマテリアル優先のブレンドを確定する。
class SpriteCommandFactory {
public:
   /// @brief ワールド空間スプライトを準備し、対応する描画段階へ一件登録する。
   static void Submit(const SpriteDrawRequest& request, const DrawSubmissionContext& context);
};

/// @brief 粒子の残存GPU状態とBloomへの入力条件を考慮して描画要求を生成する。
class ParticleCommandFactory {
public:
   /// @brief CPU粒子が尽きた直後のGPU回収も含め、必要な粒子だけをキューへ登録する。
   static void Submit(const ParticleDrawRequest& request, const DrawSubmissionContext& context);
};

/// @brief UI固有の座標変換を行い、実行時にそのまま使えるスプライト要求を生成する。
class UICommandFactory {
public:
   /// @brief 基準画面への配置を計算し、カメラの基準投影を戻してからキューへ登録する。
   static void Submit(const UIDrawRequest& request, const DrawSubmissionContext& context);
};

/// @brief 登録時の視点と描画関数を所有する線のコマンドを生成する。
class LineCommandFactory {
public:
   /// @brief 一時的な線データを所有する関数とカメラ行列をコマンドへ移す。
   static DrawCommand Create(std::function<void(ID3D12GraphicsCommandList*, const Matrix4x4&)> drawFunc, Camera* camera,
      RenderPass renderPass, std::optional<Vector3> sortPosition = std::nullopt);
};
} // namespace GameEngine
