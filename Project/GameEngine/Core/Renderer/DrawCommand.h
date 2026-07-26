#pragma once
#include <d3d12.h>
#include <vector>
#include <functional>
#include <optional>
#include <memory>
#include "Graphics/PipelineState.h"
#include "Sprite/Sprite.h"
#include "Window/Window.h"

// Forward declarations
struct ID3D12GraphicsCommandList;

namespace GameEngine {
class Model;
class Sprite;
class ParticleSystem;
class Texture;
class Camera;

/// @brief 描画コマンドの種類
enum class DrawCommandType {
   Model,
   Sprite,
   Text,
   Particle,
   Line,
   Shape
};

/// @brief 描画パスの種類
enum class RenderPass {
   Opaque,          // 不透明オブジェクト（ポストプロセス前）
   Transparent,     // 半透明オブジェクト（ポストプロセス前）
   PostProcess,     // ポストプロセス後
};

/// @brief モデル描画用データ
struct ModelDrawData {
   Model* model;
   std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> textures;
   D3D12_GPU_DESCRIPTOR_HANDLE environmentTextureSrvHandle = {}; // 環境テクスチャSRV (ptr==0なら無効)
   Camera* camera;  // 描画時のカメラを保存
   BlendMode blendMode;  // ブレンドモード
};

/// @brief スプライト描画用データ
struct SpriteDrawData {
   Sprite* sprite;
   Texture* texture;
   D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle;
   Camera* camera;  // 描画時のカメラを保存
   BlendMode blendMode;  // ブレンドモード
};

/// @brief UI用スプライト描画用データ
struct UISpriteDrawData {
   Sprite* sprite;
   Texture* texture;
   D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle;
   Sprite::AnchorPoint anchorPoint;
   uint32_t screenWidth;
   uint32_t screenHeight;
   BlendMode blendMode;  // ブレンドモード
};

/// @brief UIテキスト描画用データ
struct TextDrawData {
   D3D12_GPU_DESCRIPTOR_HANDLE atlasSrv = {};
   uint32_t indexCount = 0;
   uint32_t startIndex = 0;
   int32_t baseVertex = 0;
   int32_t sortingOrder = 0;
};

/// @brief パーティクル描画用データ
struct ParticleDrawData {
   ParticleSystem* particleSystem;
   Camera* camera;  // 描画時のカメラを保存
};

/// @brief ライン描画用データ
struct LineDrawData {
   std::function<void(ID3D12GraphicsCommandList*, const Matrix4x4&)> drawFunc;  // ViewProjection行列を受け取る
   Camera* camera;  // 描画時のカメラを保存
   Matrix4x4 viewProjectionMatrix;  // 描画時のカメラのビュープロジェクション行列
   std::optional<Vector3> sortPosition; // 透過ソート用の位置ヒント
};

/// @brief 描画コマンド
struct DrawCommand {
   DrawCommandType type;
   BlendMode blendMode;
   RenderPass renderPass;
   bool isUISprite = false;  // UIスプライトかどうかを識別するフラグ

   // 各種データ（該当するものだけ使用）
   ModelDrawData modelData;
   SpriteDrawData spriteData;
   UISpriteDrawData uiSpriteData;
   TextDrawData textData;
   ParticleDrawData particleData;
   LineDrawData lineData;

   /// @brief 後からファクトリ関数と同じ形式で値を設定できる空コマンドを生成する
   DrawCommand() = default;

   /// @brief モデル描画コマンドを作成
   /// @param model 描画するモデル
   /// @param textures モデルに使用するテクスチャのSRVハンドルリスト
   /// @param camera 描画時のカメラ
   /// @param blendMode ブレンドモード
   /// @param renderPass 描画パス
   static DrawCommand CreateModel(Model* model, const std::vector<D3D12_GPU_DESCRIPTOR_HANDLE>& textures,
	  Camera* camera, BlendMode blendMode, RenderPass renderPass);

   /// @brief スプライト描画コマンドを作成
   /// @param sprite 描画するスプライト
   /// @param texture スプライトに使用するテクスチャ
   /// @param textureSrvHandle スプライトに使用するテクスチャのSRVハンドル
   /// @param camera 描画時のカメラ
   /// @param blendMode ブレンドモード
   /// @param renderPass 描画パス
   static DrawCommand CreateSprite(Sprite* sprite, Texture* texture, D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle,
	  Camera* camera, BlendMode blendMode, RenderPass renderPass);

   /// @brief UI用スプライト描画コマンドを作成
   /// @param sprite 描画するスプライト
   /// @param texture スプライトに使用するテクスチャ
   /// @param textureSrvHandle スプライトに使用するテクスチャのSRVハンドル
   /// @param anchorPoint アンカーポイント
   /// @param screenWidth 画面幅
   /// @param screenHeight 画面高さ
   /// @param blendMode ブレンドモード
   /// @param renderPass 描画パス
   static DrawCommand CreateUISprite(Sprite* sprite, Texture* texture, D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle,
	  Sprite::AnchorPoint anchorPoint, uint32_t screenWidth, uint32_t screenHeight,
	  BlendMode blendMode, RenderPass renderPass);

   /// @brief UIテキスト描画コマンドを作成
   /// @param textData 共有テキストバッファ上の描画範囲
   /// @param renderPass 描画パス
   static DrawCommand CreateText(const TextDrawData& textData, RenderPass renderPass);

   /// @brief パーティクル描画コマンドを作成
   /// @param particleSystem 描画するパーティクルシステム
   /// @param camera 描画時のカメラ
   /// @param blendMode ブレンドモード
   /// @param renderPass 描画パス
   static DrawCommand CreateParticle(ParticleSystem* particleSystem, Camera* camera,
	  BlendMode blendMode, RenderPass renderPass);

   /// @brief ライン描画コマンドを作成
   /// @param drawFunc ラインを描画する関数（ID3D12GraphicsCommandList*とViewProjection行列を引数に取る）
   /// @param camera 描画時のカメラ
   /// @param sortPosition 透過ソート用の位置ヒント
   /// @param renderPass 描画パス
   static DrawCommand CreateLine(std::function<void(ID3D12GraphicsCommandList*, const Matrix4x4&)> drawFunc, Camera* camera,
     RenderPass renderPass, std::optional<Vector3> sortPosition = std::nullopt);
};

// ============================================================
// IDrawCommand — 型消去ベースの描画コマンドインターフェース
// DrawCommand の内部データを直接持ちながら、
// Renderer の Execute/Sort ロジックに必要な情報を仮想関数で公開する。
// ============================================================

/// @brief 型消去ベース描画コマンドインターフェース
class IDrawCommand {
public:
   /// @brief 派生コマンドを基底ポインター経由で安全に破棄する
   virtual ~IDrawCommand() = default;

   /// @brief 描画パスを取得
   virtual RenderPass GetRenderPass() const = 0;

   /// @brief ブレンドモードを取得
   virtual BlendMode GetBlendMode() const = 0;

   /// @brief 描画種別のソート優先度を取得（大きいほど先に描画）
   virtual int GetTypePriority() const = 0;

   /// @brief 透過ソート用のワールド座標を取得（取得不可の場合 nullopt）
   virtual std::optional<Vector3> GetSortPosition() const = 0;

   /// @brief 描画時に使用するカメラを取得（取得不可の場合 nullptr）
   virtual Camera* GetCamera() const = 0;

   /// @brief 描画コマンドを DrawCommand 形式で取得（Renderer の Execute に渡すため）
   virtual const DrawCommand& GetDrawCommand() const = 0;
};

/// @brief DrawCommand をラップする IDrawCommand の標準実装
class DrawCommandWrapper final : public IDrawCommand {
public:
   /// @brief 値型の描画コマンドを型消去インターフェースで所有する
   explicit DrawCommandWrapper(DrawCommand cmd) : cmd_(std::move(cmd)) {}

   /// @copydoc IDrawCommand::GetRenderPass
   RenderPass GetRenderPass() const override { return cmd_.renderPass; }
   /// @copydoc IDrawCommand::GetBlendMode
   BlendMode GetBlendMode() const override { return cmd_.blendMode; }

   /// @copydoc IDrawCommand::GetTypePriority
   int GetTypePriority() const override {
      switch (cmd_.type) {
         case DrawCommandType::Model:    return 4;
         case DrawCommandType::Sprite:   return 3;
         case DrawCommandType::Text:     return 3;
         case DrawCommandType::Particle: return 2;
         case DrawCommandType::Line:     return 1;
         default:                        return 0;
      }
   }

   /// @copydoc IDrawCommand::GetSortPosition
   std::optional<Vector3> GetSortPosition() const override;
   /// @copydoc IDrawCommand::GetCamera
   Camera* GetCamera() const override;
   /// @copydoc IDrawCommand::GetDrawCommand
   const DrawCommand& GetDrawCommand() const override { return cmd_; }

private:
   DrawCommand cmd_;
};

} // namespace GameEngine
