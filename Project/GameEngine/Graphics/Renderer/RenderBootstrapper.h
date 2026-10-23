#pragma once

namespace GameEngine {

class GraphicsDevice;
class Window;
class Camera;
class AssetManager;
class CameraManager;
class LightManager;
class Material;
class ShaderManager;
class PSOManager;
class ModelRenderer;
class SpriteRenderer;
class ParticleRenderer;
class UIRenderer;
class TextRenderer;
class OffscreenRenderTarget;
class LineRenderer;
class PostProcessManager;

struct RenderBootstrapContext {
   GraphicsDevice* device = nullptr;       ///< GPUリソースや描画コマンドの生成に使うデバイスへの参照。
   Window* window = nullptr;               ///< 描画と入力の対象となるウィンドウ。
   CameraManager* cameraManager = nullptr; ///< 出力カメラと仮想カメラの管理を行うオブジェクト。
   LightManager* lightManager = nullptr;   ///< シーン内ライトとGPU転送データを管理するオブジェクト。
   AssetManager* assetManager = nullptr;   ///< アセットの読み込みと共有を委譲する管理オブジェクトへの参照。

   Material* defaultMaterial = nullptr; ///< 専用指定のない描画へ適用する既定マテリアル。
   /// @brief 各パスの中間画像と入力スナップショットを管理する描画先。
   OffscreenRenderTarget* offscreenRenderTarget = nullptr;
   ShaderManager* shaderManager = nullptr;           ///< シェーダーのコンパイルと共有を管理するオブジェクト。
   PSOManager* psoManager = nullptr;                 ///< パイプラインマネージャーを追加。
   ModelRenderer* modelRenderer = nullptr;           ///< モデル描画コマンドを実行するレンダラー。
   SpriteRenderer* spriteRenderer = nullptr;         ///< スプライト描画コマンドを実行するレンダラー。
   ParticleRenderer* particleRenderer = nullptr;     ///< 粒子のGPU更新と描画を実行するレンダラー。
   UIRenderer* uiRenderer = nullptr;                 ///< 画面上のUIスプライトを描画するレンダラー。
   TextRenderer* textRenderer = nullptr;             ///< 文字のジオメトリ生成と描画を行うレンダラー。
   LineRenderer* lineRenderer = nullptr;             ///< ギズモや補助線の描画に使うラインレンダラーへの参照。
   LineRenderer* postProcessLineRenderer = nullptr;  ///< ポストエフェクト適用後の補助線を描くレンダラー。
   Camera* uiCamera = nullptr;                       ///< UI描画の基準とする平行投影カメラ。
   PostProcessManager* postProcessManager = nullptr; ///< ポストエフェクトの設定と適用順を管理するオブジェクト。
};

class RenderBootstrapper {
public:
   /// @brief レンダリングに必要な各サブシステムを初期化する
   /// @param context 初期化対象のレンダリングコンテキスト
   /// @return JSON定義を含む必須リソースの初期化に成功した場合はtrue
   bool Initialize(const RenderBootstrapContext& context) const;
};

}
