#pragma once
#include <d3d12.h>
#include "Graphics/PipelineState.h"
#include "Graphics/OffscreenRenderTarget.h"
#include "Graphics/Texture.h"
#include "PostProcess/PostProcessManager.h"
#include "Line/LineRenderer.h"
#include "Sprite/Sprite.h"
#include "Scene/Camera/Camera.h"
#include "PointLight.h"
#include "SpotLight.h"
#include "AreaLight.h"
#include "Window/Window.h"
#include "ShaderManager.h"
#include "PSOManager.h"
#include "CameraManager.h"
#include "LightManager.h"
#include "DrawCommand.h"
#include "Effect/LensFlare.h"
#include "ModelRenderer.h"
#include "SpriteRenderer.h"
#include "ParticleRenderer.h"
#include "UIRenderer.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <optional>

#ifdef USE_IMGUI
#include "UI/ImGuiManager.h"
#endif

namespace GameEngine {
class GraphicsDevice;
class Model;
class DirectionalLight;
class RootSignature;
class OffscreenRenderTarget;
class ParticleSystem;
class Mesh;
class AssetManager;

// @brief レンダラークラス
class Renderer {
public:
   /// @brief レンダラーの初期化
   /// @param device グラフィックスデバイス
   /// @param window ウィンドウ
   /// @param cameraManager カメラ管理
   /// @param lightManager ライト管理
   /// @param assetManager アセット管理（テクスチャマネージャーを取得するため）
   void Initialize(GraphicsDevice* device, Window* window, CameraManager* cameraManager, LightManager* lightManager, AssetManager* assetManager = nullptr);

   /// @brief フレームの開始時の処理
   void BeginFrame();

   /// @brief フレームの終了時の処理
   void EndFrame();

   /// @brief モデルを描画する
   /// @param model 描画するモデル
   /// @param texture テクスチャ
   /// @param blendMode ブレンドモード（std::nulloptの場合は現在設定されているモードを使用）
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   void Draw(Model* model, Texture* texture, std::optional<BlendMode> blendMode = std::nullopt, bool applyPostProcess = true);

   /// @brief モデルを描画する（複数テクスチャ）
   /// @param model 描画するモデル
   /// @param textures テクスチャ配列
   /// @param blendMode ブレンドモード（std::nulloptの場合は現在設定されているモードを使用）
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   void Draw(Model* model, const std::vector<Texture*>& textures, std::optional<BlendMode> blendMode = std::nullopt, bool applyPostProcess = true);

   /// @brief スプライトを描画する
   /// @param sprite 描画するスプライト
   /// @param texture テクスチャ
   /// @param blendMode ブレンドモード（std::nulloptの場合は現在設定されているモードを使用）
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   void Draw(Sprite* sprite, Texture* texture, std::optional<BlendMode> blendMode = std::nullopt, bool applyPostProcess = true);

   /// @brief パーティクルシステムを描画する
   /// @param particleSystem 描画するパーティクルシステム
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   void Draw(ParticleSystem* particleSystem, bool applyPostProcess = true);

   /// @brief UI用スプライトを描画する
   /// @param sprite 描画するスプライト
   /// @param texture テクスチャ
   /// @param anchorPoint アンカーポイント（描画基準点）
   /// @param blendMode ブレンドモード（std::nulloptの場合は現在設定されているモードを使用）
   /// @param applyPostProcess ポストプロセスを適用するかどうか
   /// @param screenWidth 画面幅（デフォルト：1280）
   /// @param screenHeight 画面高さ（デフォルト：720）
   void DrawUI(Sprite* sprite, Texture* texture,
	  Sprite::AnchorPoint anchorPoint = Sprite::AnchorPoint::TopLeft,
	  std::optional<BlendMode> blendMode = std::nullopt,
	  bool applyPostProcess = true,
	  uint32_t screenWidth = Window::kResolutionWidth,
	  uint32_t screenHeight = Window::kResolutionHeight
   );

#ifdef USE_IMGUI
   bool GetIsSceneHovered() const { return isSceneHovered_; }
   bool GetIsDockSpaceVisible() const { return imGuiManager_->IsDockSpaceVisible(); }
   void SetDockSpaceVisible(bool visible) { imGuiManager_->SetDockSpaceVisible(visible); }
#endif

   /// @brief 線を描画する（LineRendererに委任）
   /// @param start 開始点
   /// @param end 終了点
   /// @param color 色
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   void DrawLine(const Vector3& start, const Vector3& end, const Vector4& color, bool applyPostProcess = true);

   /// @brief スプライン曲線を描画する（LineRendererに委任）
   /// @param controlPoints 制御点のリスト
   /// @param color 色
   /// @param segmentCount セグメント数
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   void DrawSpline(const std::vector<Vector3>& controlPoints, const Vector4& color, size_t segmentCount = 10, bool applyPostProcess = true);

   /// @brief グリッドを描画する（LineRendererに委任）
   /// @param plane 描画する平面（デフォルト：XZ平面）
   /// @param gridSize グリッドの間隔（デフォルト：1.0f）
   /// @param thickLineInterval 太い線を描画する間隔（デフォルト：10本ごと）
   /// @param range カメラからの範囲（グリッド数、デフォルト：100）
   /// @param enableFade カメラからの距離に応じてフェードアウトするか（デフォルト：true）
   /// @param fadeDistance フェードアウトを開始する距離（デフォルト：50.0f、enableFadeがtrueの場合のみ有効）
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   void DrawGrid(GridPlane plane = GridPlane::XZ, float gridSize = 1.0f, int thickLineInterval = 10, int range = 100, bool enableFade = true, float fadeDistance = 50.0f, bool applyPostProcess = true);

   /// @brief 球を描画する
   /// @param center 中心
   /// @param radius 半径
   /// @param color 色
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   void DrawSphere(const Vector3& center, float radius, const Vector4& color, bool applyPostProcess = true);

   /// @brief 半球を描画する
   /// @param center 中心
   /// @param radius 半径
   /// @param up 上方向
   /// @param color 色
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   void DrawHemisphere(const Vector3& center, float radius, const Vector3& up, const Vector4& color, bool applyPostProcess = true);

   /// @brief 円錐を描画する
   /// @param apex 円錐の頂点
   /// @param radius 円周の半径
   /// @param height 円錐の高さ
   /// @param direction 円錐の向き
   /// @param color 色
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   void DrawCone(const Vector3& apex, float radius, float height, const Vector3& direction, const Vector4& color, bool applyPostProcess = true);

   /// @brief ボックスを描画する
   /// @param center 中心
   /// @param size サイズ
   /// @param color 色
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   void DrawBox(const Vector3& center, const Vector3& size, const Vector4& color, bool applyPostProcess = true);

   /// @brief 円を描画する
   /// @param center 中心
   /// @param radius 半径
   /// @param normal 法線
   /// @param color 色
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   void DrawCircle(const Vector3& center, float radius, const Vector3& normal, const Vector4& color, bool applyPostProcess = true);

   /// @brief レンダラーの終了処理
   void Finalize();

   /// @brief ブレンドモードを設定する（次の描画に使用）
   /// @param blendMode ブレンドモード
   void SetBlendMode(BlendMode blendMode);

   /// @brief 現在のブレンドモードを取得
   /// @return 現在のブレンドモード
   BlendMode GetCurrentBlendMode() const { return currentBlendMode_; }

   /// @brief PostProcessManagerを取得
   /// @return PostProcessManagerのポインタ
   PostProcessManager* GetPostProcessManager() const { return postProcessManager_.get(); }

   /// @brief ShaderManagerを取得
   /// @return ShaderManagerのポインタ
   ShaderManager* GetShaderManager() const { return shaderManager_.get(); }

   /// @brief PSOManagerを取得
   /// @return PSOManagerのポインタ
   PSOManager* GetPSOManager() const { return psoManager_.get(); }

   /// @brief レンズフレアを取得
   /// @return レンズフレアのポインタ
   LensFlare* GetLensFlare() const { return lensFlare_.get(); }

   /// @brief レンズフレアのオクルージョンクエリを開始
   void BeginLensFlareOcclusionQuery();

   /// @brief レンズフレアのオクルージョンクエリを終了
   void EndLensFlareOcclusionQuery();

   CameraManager* GetCameraManager() const { return cameraManager_; }
   LightManager* GetLightManager() const { return lightManager_; }

private:
   GraphicsDevice* device_ = nullptr;
   CameraManager* cameraManager_ = nullptr;
   LightManager* lightManager_ = nullptr;
   AssetManager* assetManager_ = nullptr;

   // シェーダーとパイプライン管理
   std::unique_ptr<ShaderManager> shaderManager_ = std::make_unique<ShaderManager>();
   std::unique_ptr<PSOManager> psoManager_ = std::make_unique<PSOManager>();

   // 専門レンダラー
   std::unique_ptr<ModelRenderer> modelRenderer_ = std::make_unique<ModelRenderer>();
   std::unique_ptr<SpriteRenderer> spriteRenderer_ = std::make_unique<SpriteRenderer>();
   std::unique_ptr<ParticleRenderer> particleRenderer_ = std::make_unique<ParticleRenderer>();
   std::unique_ptr<UIRenderer> uiRenderer_ = std::make_unique<UIRenderer>();

   std::unique_ptr<OffscreenRenderTarget> offscreenRenderTarget_ = std::make_unique<OffscreenRenderTarget>();
   std::unique_ptr<LineRenderer> lineRenderer_ = std::make_unique<LineRenderer>();

   // PostProcessManagerで置き換え
   std::unique_ptr<PostProcessManager> postProcessManager_ = std::make_unique<PostProcessManager>();

   // UI描画専用カメラ（平行投影）
   std::unique_ptr<Camera> uiCamera_ = std::make_unique<Camera>();

   // レンズフレア
   std::unique_ptr<LensFlare> lensFlare_ = std::make_unique<LensFlare>();
   Vector3 lensFlareSourcePos_ = Vector3(0.0f, 0.0f, 0.0f);
   bool lensFlareQueryActive_ = false;
   bool lensFlareTrackingEnabled_ = false;
   std::vector<Vector3> lensFlareTrackedPositions_;

   // 描画コマンドリスト（レンダーパス別）
   // 不透明オブジェクトは即時描画するため、コマンドリストは不要
   std::vector<DrawCommand> transparentCommands_;  // 半透明オブジェクト
   std::vector<DrawCommand> postProcessCommands_;  // ポストプロセス後の描画

   BlendMode currentBlendMode_ = BlendMode::kBlendModeNormal;
   std::string currentPipelineName_;
   BlendMode currentPipelineBlendMode_ = BlendMode::kBlendModeNormal;

   std::unique_ptr<Material> defaultMaterial_ = nullptr;

#ifdef USE_IMGUI
   std::unique_ptr<ImGuiManager> imGuiManager_ = std::make_unique<ImGuiManager>();
   bool isSceneHovered_ = false;
#endif

private:
   /// @brief 描画コマンドを実行する
   /// @param commands 実行する描画コマンドリスト
   void ExecuteDrawCommands(const std::vector<DrawCommand>& commands);

   /// @brief ラインの内部描画処理
   /// @param lineData ライン描画データ
   void DrawLineInternal(const LineDrawData& lineData);

   /// @brief フルスクリーントライアングルでテクスチャを画面に描画
   /// @param textureSrvHandle 描画するテクスチャのSRVハンドル
   void DrawFullscreenTriangle(D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle);

   /// @brief UI描画専用カメラを初期化
   void InitializeUICamera();

   /// @brief パイプラインを設定する（全レンダラー共通）
   /// @param pipelineName パイプライン名
   /// @param blendMode ブレンドモード
   void SetPipeline(const std::string& pipelineName, BlendMode blendMode);
};
}