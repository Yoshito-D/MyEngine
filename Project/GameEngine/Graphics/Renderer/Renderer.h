#pragma once

#include <d3d12.h>
#include "GameEngine/Graphics/Pipeline/PipelineState.h"
#include "GameEngine/Graphics/Device/OffscreenRenderTarget.h"
#include "GameEngine/Graphics/Resources/Texture.h"
#include "GameEngine/Graphics/PostProcess/PostProcessManager.h"
#include "GameEngine/Graphics/Renderer/Pass/LineRenderer.h"
#include "GameEngine/Object/Sprite/Sprite.h"
#include "GameEngine/Scene/Camera/Camera.h"
#include "GameEngine/Scene/Light/PointLight.h"
#include "GameEngine/Scene/Light/SpotLight.h"
#include "GameEngine/Scene/Light/AreaLight.h"
#include "GameEngine/Window/Window.h"
#include "GameEngine/Graphics/Pipeline/ShaderManager.h"
#include "GameEngine/Graphics/Pipeline/PSOManager.h"
#include "GameEngine/Scene/Camera/CameraManager.h"
#include "GameEngine/Graphics/Renderer/Light/LightManager.h"
#include "GameEngine/Graphics/Renderer/DrawCommand.h"
#include "GameEngine/Graphics/Renderer/Pass/ModelRenderer.h"
#include "GameEngine/Object/Model/Model.h"
#include "GameEngine/Graphics/Renderer/Pass/SpriteRenderer.h"
#include "GameEngine/Graphics/Renderer/Pass/ParticleRenderer.h"
#include "GameEngine/Graphics/Renderer/Pass/UIRenderer.h"
#include "GameEngine/Graphics/Renderer/Pass/TextRenderer.h"
#include "GameEngine/Graphics/Renderer/RenderBootstrapper.h"
#include "GameEngine/Graphics/Renderer/Pass/IRenderPass.h"
#include "GameEngine/Graphics/Renderer/Pass/FrameContext.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <optional>
#include <filesystem>
#include <string_view>
#include <cstdint>

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#include "GameEngine/Editor/Renderer/RendererEditorController.h"
#endif

namespace GameEngine {
class GraphicsDevice;
class Model;
class Object;
class DirectionalLight;
class RootSignature;
class OffscreenRenderTarget;
class ParticleSystem;
class Mesh;
class AssetManager;
class Skybox;
class PlayerShadowPass;
struct PlayerShadowFrameData;

/// @brief 描画コマンドの収集・レンダーパス実行・エディター描画を統括する
class Renderer {
public:
   ~Renderer();

   /// @brief レンダラーの初期化
   /// @param device グラフィックスデバイス
   /// @param window ウィンドウ
   /// @param cameraManager カメラ管理
   /// @param lightManager ライト管理
   /// @param assetManager アセット管理（テクスチャマネージャーを取得するため）
   void Initialize(GraphicsDevice* device, Window* window, CameraManager* cameraManager, LightManager* lightManager,
      AssetManager* assetManager = nullptr);

   /// @brief フレームの開始時の処理
   void BeginFrame();

   /// @brief 描画先を現在のバックバッファサイズへ同期
   void SyncRenderTargetSizeToDevice();

   /// @brief フレームの終了時の処理
   void EndFrame();

   /// @brief 今フレームのプレイヤー影の対象を専用パスへ渡す。
   /// @param frameData EndFrameの描画完了まで有効なモデル・カメラとワールド座標情報
   /// @return パスへ渡せた場合true。パス未登録・初期化失敗時はfalse。
   /// @note BeginFrame後、EndFrame前に毎フレーム呼ぶ。1体分を保持し、再設定時は最後の値を使う。
   /// 通常描画と同じカメラのモデル行列とボーンを更新しておくこと。描画可否はパス実行時に検証する。
   bool SetPlayerShadowFrameData(const PlayerShadowFrameData& frameData);

   /// @brief 今フレームの影を取り消し、借用している対象参照を解除する。
   /// @note フレーム途中で対象モデルやシーンを破棄する場合は、破棄前に呼ぶ。
   void ClearPlayerShadowFrameData();

   /// @brief モデルを描画する
   /// @param model 描画するモデル
   /// @param texture テクスチャ
   /// @param blendMode ブレンドモード（std::nulloptの場合は現在設定されているモードを使用）
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   void Draw(
      Model* model, Texture* texture, std::optional<BlendMode> blendMode = std::nullopt, bool applyPostProcess = true);

   /// @brief モデルを描画する（複数テクスチャ）
   /// @param model 描画するモデル
   /// @param textures テクスチャ配列
   /// @param blendMode ブレンドモード（std::nulloptの場合は現在設定されているモードを使用）
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   void Draw(Model* model, const std::vector<Texture*>& textures, std::optional<BlendMode> blendMode = std::nullopt,
      bool applyPostProcess = true);

   /// @brief スプライトを描画する
   /// @param sprite 描画するスプライト
   /// @param texture テクスチャ
   /// @param blendMode ブレンドモード（std::nulloptの場合は現在設定されているモードを使用）
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   void Draw(Sprite* sprite, Texture* texture, std::optional<BlendMode> blendMode = std::nullopt,
      bool applyPostProcess = true);

   /// @brief パーティクルシステムを描画する
   /// @param particleSystem 描画するパーティクルシステム
   void Draw(ParticleSystem* particleSystem);

   /// @brief UI用スプライトを描画する
   /// @param sprite 描画するスプライト
   /// @param texture テクスチャ
   /// @param anchorPoint アンカーポイント（描画基準点）
   /// @param blendMode ブレンドモード（std::nulloptの場合は現在設定されているモードを使用）
   /// @param applyPostProcess ポストプロセスを適用するかどうか
   /// @param screenWidth UI配置に使う論理画面幅（デフォルト：1280）
   /// @param screenHeight UI配置に使う論理画面高（デフォルト：720）
   /// @details 論理画面の縦横比とサイズを保ち、実際の出力中央へ一様拡大して描画する。
   void DrawUI(Sprite* sprite, Texture* texture, Sprite::AnchorPoint anchorPoint = Sprite::AnchorPoint::TopLeft,
      std::optional<BlendMode> blendMode = std::nullopt, bool applyPostProcess = true,
      uint32_t screenWidth = Window::kUiReferenceWidth, uint32_t screenHeight = Window::kUiReferenceHeight);

   /// @brief UTF-8文字列をスクリーンUIとして描画する
   /// @param text UTF-8文字列
   /// @param position 画面アンカーからのピクセル位置
   /// @param style フォントと表示設定
   void DrawUIText(std::string_view text, const Vector2& position, const TextStyle& style);

   /// @brief UTF-8文字列のレイアウトサイズを測定する
   /// @param text UTF-8文字列
   /// @param style フォントとレイアウト設定
   /// @return ピクセル単位の幅と高さ
   Vector2 MeasureText(std::string_view text, const TextStyle& style);

   /// @brief シーン遷移用の全画面暗転率を設定する
   /// @param opacity 0で通常表示、1で完全な黒
   void SetSceneTransitionOpacity(float opacity);

#ifdef USE_IMGUI

   /// @brief シーン管理の初期化後に、所有権を移さずエディタUIへ依存を接続する。
   void SetEditorSceneSession(EditorSceneSession* session);

   bool GetIsSceneHovered() const { return isSceneHovered_; }

   bool GetIsDockSpaceVisible() const { return imGuiManager_->IsDockSpaceVisible(); }

   void SetDockSpaceVisible(bool visible) { imGuiManager_->SetDockSpaceVisible(visible); }

   /// @brief エディタの共通メニューと閉じるボタンで共有する表示状態を取得する。
   /// @param stableId ウィンドウの固定ID。
   /// @param label メニューの日本語・英語ラベル（文字列リテラル）。
   /// @param defaultVisible 初回登録時の表示状態。
   /// @return 現在の表示状態。マネージャーがない場合はfalse。
   bool GetEditorWindowVisibility(const char* stableId, ImGuiHelper::LocalizedText label, bool defaultVisible = true);

   /// @brief 登録済みエディターウィンドウの表示状態を設定する。
   /// @param stableId 登録済みウィンドウの固定ID。
   /// @param visible 表示する場合はtrue。
   /// @return マネージャーがない、またはIDが未登録の場合はfalse。
   bool SetEditorWindowVisibility(const char* stableId, bool visible);
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
   void DrawSpline(const std::vector<Vector3>& controlPoints, const Vector4& color, size_t segmentCount = 10,
      bool applyPostProcess = true);

   /// @brief グリッドを描画する（LineRendererに委任）
   /// @param plane 描画する平面（デフォルト：XZ平面）
   /// @param gridSize グリッドの間隔（デフォルト：1.0f）
   /// @param thickLineInterval 太い線を描画する間隔（デフォルト：10本ごと）
   /// @param range カメラからの範囲（グリッド数、デフォルト：100）
   /// @param enableFade カメラからの距離に応じてフェードアウトするか（デフォルト：true）
   /// @param fadeDistance フェードアウトを開始する距離（デフォルト：50.0f、enableFadeがtrueの場合のみ有効）
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   void DrawGrid(GridPlane plane = GridPlane::XZ, float gridSize = 1.0f, std::int32_t thickLineInterval = 10,
      std::int32_t range = 100, bool enableFade = true, float fadeDistance = 50.0f, bool applyPostProcess = true);

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
   void DrawHemisphere(
      const Vector3& center, float radius, const Vector3& up, const Vector4& color, bool applyPostProcess = true);

   /// @brief 円錐を描画する
   /// @param apex 円錐の頂点
   /// @param radius 円周の半径
   /// @param height 円錐の高さ
   /// @param direction 円錐の向き
   /// @param color 色
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   void DrawCone(const Vector3& apex, float radius, float height, const Vector3& direction, const Vector4& color,
      bool applyPostProcess = true);

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
   void DrawCircle(
      const Vector3& center, float radius, const Vector3& normal, const Vector4& color, bool applyPostProcess = true);

   /// @brief モデルのスケルトンをデバッグ描画する
   /// @param model 描画対象モデル
   /// @param jointRadius ジョイント球の半径
   /// @param jointColor ジョイント球の色
   /// @param boneColor ジョイント接続線の色
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   void DrawSkeleton(Model* model, float jointRadius = 0.03f,
      const Vector4& jointColor = Vector4(1.0f, 0.2f, 0.2f, 1.0f),
      const Vector4& boneColor = Vector4(0.2f, 1.0f, 1.0f, 1.0f), bool applyPostProcess = false);

   /// @brief 外部システムから描画コマンドを投入する
   /// @param command 描画コマンド
   void SubmitDrawCommand(const DrawCommand& command);

   /// @brief スカイボックスを描画する（BeginFrame/EndFrameの間で呼ぶ）
   /// @param skybox スカイボックス
   void DrawSkybox(Skybox* skybox);

   /// @brief モデルに適用する環境テクスチャを設定する
   /// @param texture キューブマップテクスチャ（nullptrで解除）
   void SetEnvironmentTexture(Texture* texture);

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
   const PostProcessManager* GetPostProcessManager() const { return postProcessManager_.get(); }

   /// @brief 編集・更新操作に使用するサービスへのアクセス。
   PostProcessManager* GetPostProcessManager() { return postProcessManager_.get(); }

   /// @brief ShaderManagerを取得
   /// @return ShaderManagerのポインタ
   const ShaderManager* GetShaderManager() const { return shaderManager_.get(); }

   /// @brief PSOManagerを取得
   /// @return PSOManagerのポインタ
   const PSOManager* GetPSOManager() const { return psoManager_.get(); }

   const CameraManager* GetCameraManager() const { return cameraManager_; }

   /// @brief 編集・更新操作に使用するサービスへのアクセス。
   CameraManager* GetCameraManager() { return cameraManager_; }

   const LightManager* GetLightManager() const { return lightManager_; }

   /// @brief 編集・更新操作に使用するサービスへのアクセス。
   LightManager* GetLightManager() { return lightManager_; }

   /// @brief LineRendererを取得
   /// @return LineRendererのポインタ
   const LineRenderer* GetLineRenderer() const { return lineRenderer_.get(); }

   /// @brief 編集・更新操作に使用するサービスへのアクセス。
   LineRenderer* GetLineRenderer() { return lineRenderer_.get(); }

   // ========== 描画パスの構成API ==========

   /// @brief レンダーパスを末尾に追加する
   /// @param pass 追加するパス（所有権を移譲）
   void AddPass(std::unique_ptr<IRenderPass> pass);

   /// @brief 登録済みのレンダーパスをすべてクリアする
   /// @note GPUが既存パスの資源を使い終えたフレーム境界で呼ぶ。
   void ClearPasses();

   /// @brief デフォルトのパス構成（Opaque→PlayerShadow→Transparent→PostEffect）を再構築する
   /// @note GPU完了後に呼ぶ。影用資源の初期化に失敗した場合は影パスだけを省く。
   void BuildDefaultPasses();

private:
   GraphicsDevice* device_ = nullptr;       ///< GPUリソースや描画コマンドの生成に使うデバイスへの参照。
   CameraManager* cameraManager_ = nullptr; ///< 描画に使う出力カメラを取得する管理オブジェクトへの非所有参照。
   LightManager* lightManager_ = nullptr;   ///< 描画で参照するライトとGPU転送バッファの管理先。
   AssetManager* assetManager_ = nullptr;   ///< アセットの読み込みと共有を委譲する管理オブジェクトへの参照。

   // シェーダーとパイプライン管理
   /// @brief シェーダーのコンパイルと共有を管理するオブジェクト。
   std::unique_ptr<ShaderManager> shaderManager_ = std::make_unique<ShaderManager>();
   std::unique_ptr<PSOManager> psoManager_ = std::make_unique<PSOManager>(); ///< パイプラインマネージャーを追加。

   // 専門レンダラー
   /// @brief モデル描画コマンドを実行するレンダラー。
   std::unique_ptr<ModelRenderer> modelRenderer_ = std::make_unique<ModelRenderer>();
   /// @brief スプライト描画コマンドを実行するレンダラー。
   std::unique_ptr<SpriteRenderer> spriteRenderer_ = std::make_unique<SpriteRenderer>();
   /// @brief 粒子のGPU更新と描画を実行するレンダラー。
   std::unique_ptr<ParticleRenderer> particleRenderer_ = std::make_unique<ParticleRenderer>();
   /// @brief 画面上のUIスプライトを描画するレンダラー。
   std::unique_ptr<UIRenderer> uiRenderer_ = std::make_unique<UIRenderer>();
   /// @brief 文字のジオメトリ生成と描画を行うレンダラー。
   std::unique_ptr<TextRenderer> textRenderer_ = std::make_unique<TextRenderer>();

   /// @brief 各パスの中間画像と入力スナップショットを管理する描画先。
   std::unique_ptr<OffscreenRenderTarget> offscreenRenderTarget_ = std::make_unique<OffscreenRenderTarget>();
   /// @brief ギズモや補助線の描画に使うラインレンダラーへの参照。
   std::unique_ptr<LineRenderer> lineRenderer_ = std::make_unique<LineRenderer>();
   /// @brief ポストエフェクト適用後の補助線を描くレンダラー。
   std::unique_ptr<LineRenderer> postProcessLineRenderer_ = std::make_unique<LineRenderer>();

   // PostProcessManagerで置き換え
   /// @brief ポストエフェクトの設定と適用順を管理するオブジェクト。
   std::unique_ptr<PostProcessManager> postProcessManager_ = std::make_unique<PostProcessManager>();

   // UI描画専用カメラ
   std::unique_ptr<Camera> uiCamera_ = std::make_unique<Camera>(); ///< UI描画の基準とする平行投影カメラ。
   /// @brief 奥行きを持つUIモデルを描く透視投影カメラ。
   std::unique_ptr<Camera> perspectiveUiCamera_ = std::make_unique<Camera>();
   /// @brief UIのレイアウト基準となるビューポート寸法。
   Vector2 uiViewportSize_ = {
      static_cast<float>(Window::kUiReferenceWidth), static_cast<float>(Window::kUiReferenceHeight)};

   // 描画コマンドリスト（レンダーパス別）
   std::vector<std::unique_ptr<IDrawCommand>> opaqueCommands_;      // 不透明オブジェクト
   std::vector<std::unique_ptr<IDrawCommand>> transparentCommands_; // 半透明オブジェクト
   std::vector<std::unique_ptr<IDrawCommand>> postProcessCommands_; // ポストプロセス後の描画

   BlendMode currentBlendMode_ = BlendMode::kBlendModeNormal; ///< 現在のブレンドモード。
   std::string currentPipelineName_; ///< 現在のコマンドリストへ設定済みと記録したパイプライン名。
   /// @brief 現在設定済みと記録したパイプラインのブレンド方式。
   BlendMode currentPipelineBlendMode_ = BlendMode::kBlendModeNormal;
   /// @brief 現在の環境マッピング用キューブマップSRV。
   D3D12_GPU_DESCRIPTOR_HANDLE activeEnvironmentTextureSrvHandle_ = {};

   struct SceneTransitionConstants {
      float opacity = 0.0f;        ///< 影の濃さ（0～1）。
      float blurDirection[2] = {}; ///< シーン遷移でぼかしを適用する画面上の方向。
      uint32_t applyComposite = 0; ///< ぼかし結果へ暗転合成も適用するかどうか。
   };

   /// @brief シーン遷移の横方向ぼかし設定を保持するバッファ。
   Microsoft::WRL::ComPtr<ID3D12Resource> sceneTransitionHorizontalConstantBuffer_;
   /// @brief 横方向ぼかし用定数バッファの永続Map先。
   SceneTransitionConstants* sceneTransitionHorizontalConstants_ = nullptr;
   /// @brief 縦方向ぼかしと暗転合成に使う所有定数バッファ。
   Microsoft::WRL::ComPtr<ID3D12Resource> sceneTransitionConstantBuffer_;
   SceneTransitionConstants* sceneTransitionConstants_ = nullptr; ///< 縦方向ぼかし・暗転用定数バッファの永続Map先。
   float sceneTransitionOpacity_ = 0.0f;                          ///< シーン遷移の暗転量。0で通常表示、1で完全な暗転。

   /// @brief パイプライン名+ブレンドモードをキーにした解決済みキャッシュ
   struct PipelineHandle {
      const PipelineState* pso = nullptr; ///< 起動時に解決済みのポインタ
      bool resolved = false;              ///< 一度でも解決を試みたか
   };

   /// @brief 名前から解決した描画パイプラインハンドルのキャッシュ。
   std::unordered_map<std::string, PipelineHandle> pipelineCache_;

   /// @brief pipelineCache_ のキーを生成
   std::string MakePipelineCacheKey(const std::string& name, BlendMode mode) const;

   std::unique_ptr<Material> defaultMaterial_ = nullptr; ///< 専用指定のない描画へ適用する既定マテリアル。
   /// @brief 描画サブシステムの初期化と接続を行う所有オブジェクト。
   std::unique_ptr<RenderBootstrapper> renderBootstrapper_;

   // レンダーパスリスト（Opaque -> PlayerShadow -> Transparent -> PostEffect の順で実行）
   std::vector<std::unique_ptr<IRenderPass>> renderPasses_; ///< 実行順に並べて所有する描画パス。

   // 所有権はrenderPasses_。対象データの受け渡しにだけ使い、ClearPassesで必ず解除する。
   PlayerShadowPass* playerShadowPass_ = nullptr; ///< renderPassesが所有するプレイヤー影パスへの非所有参照。

   // 毎フレーム構築するフレームコンテキスト
   FrameContext frameCtx_; ///< 各描画パスへ渡す現在フレームの共有コンテキスト。

#ifdef USE_IMGUI
   /// @brief ImGuiのフレームとエディターウィンドウを管理する所有オブジェクト。
   std::unique_ptr<ImGuiManager> imGuiManager_ = std::make_unique<ImGuiManager>();
   /// @brief 描画設定やシーン編集UIを制御する所有オブジェクト。
   std::unique_ptr<RendererEditorController> editorController_;
   bool isSceneHovered_ = false; ///< シーンビューポート上にカーソルがあるかどうか。
#endif

private:
   void DrawAutoRegisteredModels();
   void DrawAutoRegisteredSprites();
   void DrawAutoRegisteredSkyboxes();
   void DrawAutoRegisteredParticles();
   void DrawAutoRegisteredTexts();

   /// @brief 指定カメラでモデル描画コマンドを登録する
   void DrawModelWithCamera(
      Model* model, Texture* texture, Camera* camera, std::optional<BlendMode> blendMode, bool applyPostProcess);

   /// @brief 描画コマンドを実行する
   /// @param commands 実行する描画コマンドリスト
   void ExecuteDrawCommands(const std::vector<std::unique_ptr<IDrawCommand>>& commands);

   /// @brief 描画パスに応じてコマンドを振り分ける
   /// @param command 振り分ける描画コマンド
   void RouteDrawCommand(const DrawCommand& command);

   /// @brief ラインの内部描画処理
   /// @param lineData ライン描画データ
   void DrawLineInternal(const LineDrawData& lineData);

   /// @brief フルスクリーントライアングルでテクスチャを画面に描画
   /// @param textureSrvHandle 描画するテクスチャのSRVハンドル
   void DrawFullscreenTriangle(D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle);

   /// @brief UIを含む最終シーン画像へ遷移暗転を適用する
   void ApplySceneTransitionOverlay();

   /// @brief UI描画専用カメラを初期化
   void InitializeUICamera();

   /// @brief UI描画専用カメラの投影設定を描画先サイズへ同期
   void SyncUICameraToRenderTarget(uint32_t screenWidth, uint32_t screenHeight);

   /// @brief 指定したパス用のラインレンダラーを取得
   LineRenderer* SelectLineRenderer(bool applyPostProcess);

   /// @brief ラインレンダラーに蓄積されたコマンドをフラッシュ
   void FlushLineRenderer(LineRenderer* renderer, RenderPass renderPass);

   /// @brief パイプラインを設定する（全レンダラー共通）
   /// @param pipelineName パイプライン名
   /// @param blendMode ブレンドモード
   void SetPipeline(const std::string& pipelineName, BlendMode blendMode);

   /// CSなどによる外部PSO変更後に、次のグラフィックスPSO設定を強制する。
   void InvalidatePipelineBinding();

   /// @brief ブレンドモードとポストプロセス指定から描画パスを決定
   RenderPass DetermineRenderPass(BlendMode blendMode, bool applyPostProcess) const;
};
}
