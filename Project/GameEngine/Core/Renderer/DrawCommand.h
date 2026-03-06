#pragma once
#include <d3d12.h>
#include <vector>
#include <functional>
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
   ParticleDrawData particleData;
   LineDrawData lineData;

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
   /// @brief sprite 描画するスプライト
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
   /// @param renderPass 描画パス
   static DrawCommand CreateLine(std::function<void(ID3D12GraphicsCommandList*, const Matrix4x4&)> drawFunc, Camera* camera,
	  RenderPass renderPass);
};

} // namespace GameEngine
