#pragma once
#include "GameEngine/Graphics/Renderer/Pass/IRenderPass.h"
#include "GameEngine/Math/VectorMath.h"
#include <cstdint>
#include <d3d12.h>
#include <memory>
#include <string>
#include <vector>
#include <wrl.h>

namespace GameEngine {
class Camera;
class GraphicsDevice;
class Model;
class PipelineState;
class SrvDescriptorAllocation;

/// @brief プレイヤーのシルエット影に使用する初期設定。距離の単位はワールド座標と同じ。
/// @note 現在は不透明なプレイヤー1体と、変形しない球形惑星1個を対象とする。
struct PlayerShadowSettings {
   uint32_t maskWidth = 256;       ///< マスク画像の幅
   uint32_t maskHeight = 256;      ///< マスク画像の高さ
   float projectionWidth = 4.0f;  ///< 正射影の横幅。全ポーズが収まる値へ調整する
   float projectionHeight = 4.0f; ///< 正射影の縦幅
   float cameraDistance = 4.0f;   ///< プレイヤーから影用カメラまでの距離
   float nearClip = 0.1f;         ///< 影用カメラの近クリップ距離
   float farClip = 1000.0f;       ///< プレイヤーと地面の両方を含める遠クリップ距離
   float opacity = 0.4f;          ///< 影の濃さ（0～1）
   float receiverRange = 8.0f;    ///< 地表の投影中心から影を許可する範囲
};

/// @brief ゲーム側が毎フレーム渡す影の対象。ポインターは描画完了まで有効な借用参照。
struct PlayerShadowFrameData {
   Model* player = nullptr;   ///< 影を作るモデル
   Model* receiver = nullptr; ///< 影を受ける惑星のモデル
   Camera* camera = nullptr;  ///< 通常シーンと同じカメラ
   Vector3 playerPosition{};  ///< プレイヤーのワールド位置
   Vector3 planetCenter{};    ///< 球形惑星のワールド中心
   float planetRadius = 0.0f; ///< 球形惑星の地表半径
};

/// @brief プレイヤーのマスク生成と惑星への影合成を行う描画パス。
/// @details OpaquePassとTransparentPassの間で実行する。通常描画と同じカメラで更新済みの
/// TransformComponent、専用シェーダー/PSO、実装済みのBindPreservingContents()が必要。
/// @note 単一フレームのGPU完了を待つ現在のRenderer向け。1フレームに1回だけ実行する。
/// パスの破棄はGPU完了後、GraphicsDeviceの破棄前に行う。
class PlayerShadowPass final : public IRenderPass {
public:
   /// @brief 未初期化の描画パスを作る。
   PlayerShadowPass() = default;
   /// @brief GPU完了後に専用資源とSRVスロットを解放する。
   ~PlayerShadowPass() override;
   /// @brief Map済み領域とGPU資源を二重所有しないためコピーを禁止する。
   PlayerShadowPass(const PlayerShadowPass&) = delete;
   /// @brief Map済み領域とGPU資源を二重所有しないためコピー代入を禁止する。
   PlayerShadowPass& operator=(const PlayerShadowPass&) = delete;

   /// @brief マスク画像・専用定数バッファを初期化する。
   /// @param device パスより長く生存するグラフィックスデバイス
   /// @param settings マスク解像度と投影設定
   /// @return 初期化成功時true。不正設定・資源作成失敗・二重初期化時はfalse。
   bool Initialize(GraphicsDevice* device, const PlayerShadowSettings& settings = {});

   /// @brief 今フレームの影対象を保存する。ゲーム側から毎フレーム呼ぶ。
   /// @param frameData 描画完了まで有効な対象と位置情報
   void SetFrameData(const PlayerShadowFrameData& frameData);

   /// @brief 保存した借用参照を破棄し、影対象なしへ戻す。
   void ClearFrameData();

   /// @brief マスク生成と影合成を実行し、成功・失敗のどちらでも今フレームの参照を解除する。
   /// @param ctx 通常描画と共有するフレームコンテキスト
   void Execute(FrameContext& ctx) override;

   /// @copydoc IRenderPass::GetName
   std::string_view GetName() const override { return "PlayerShadowPass"; }
   /// @brief 不透明シーンへの追加描画として分類する。実行順はRendererの登録順で決まる。
   RenderPassType GetPassType() const override { return RenderPassType::Opaque; }
   /// @brief デバッグ表示用のマスクSRVを返す。初回のマスク描画前は未定義画像を読ませないためptrが0。
   D3D12_GPU_DESCRIPTOR_HANDLE GetMaskSRVHandleGPU() const { return maskReady_ ? maskSrvHandle_ : D3D12_GPU_DESCRIPTOR_HANDLE{}; }

private:
   // PlayerShadow.hlsliと同じ配列順・サイズ。CBVの確保サイズは別途256バイトへ切り上げる。
   struct MaskConstants {
      Matrix4x4 playerShadowWVP{};
   };
   struct ProjectConstants {
      Matrix4x4 worldViewProjection{};
      Matrix4x4 world{};
      Matrix4x4 worldInverseTranspose{};
      Matrix4x4 shadowViewProjection{};
   };
   struct ShadowParameters {
      Vector4 shadowUpAndOpacity{};      // xyz: 惑星の外向き、w: 濃さ
      Vector4 groundPositionAndRange{};  // xyz: 投影中心、w: 受け面の許可範囲
   };
   static_assert(sizeof(MaskConstants) == 64);
   static_assert(sizeof(ProjectConstants) == 256);
   static_assert(sizeof(ShadowParameters) == 32);

   // 事前に描画可能性を検証し、RTV変更後はGPUへの設定とDrawだけを行う。
   struct DrawGeometry {
      D3D12_VERTEX_BUFFER_VIEW vertexView{};
      D3D12_INDEX_BUFFER_VIEW indexView{};
      UINT indexCount = 0;
      size_t meshIndex = 0;
   };
   struct ShadowBindings {
      PipelineState* maskPipeline = nullptr;
      PipelineState* projectPipeline = nullptr;
      UINT maskConstants = 0;
      UINT projectConstants = 0;
      UINT parameters = 0;
      UINT maskTexture = 0;
   };

   /// @brief 初期化失敗時またはGPU完了後に専用資源を破棄する。
   void ReleaseResources();
   /// @brief 同じ失敗が続く場合のログ重複を抑制し、falseを返す。
   bool ReportFailure(const std::string& reason);
   /// @brief GPU状態を変更する前に専用PSOと各Root indexを解決する。
   bool PrepareBindings(FrameContext& ctx);
   /// @brief プリミティブまたはアセットの描画情報を収集・検証する。
   bool CollectGeometry(Model* model, std::vector<DrawGeometry>& geometry);
   /// @brief 検証済みの頂点・インデックスを設定し、全サブメッシュを描く。
   void DrawGeometryList(const std::vector<DrawGeometry>& geometry);

   /// @brief 黒でクリアするRGBA8テクスチャとRTV/SRVを確保する。
   bool CreateMaskResources();
   /// @brief 通常描画の行列を上書きしない専用定数バッファを確保する。
   bool CreateConstantBuffers();
   /// @brief 対象・有限値・半径・投影設定が描画に使用できるか確認する。
   bool ValidateFrameData() const;
   /// @brief 重力方向、地表位置、連続した接線方向と正射影行列を求める。
   bool UpdateShadowProjection();
   /// @brief 今フレームのスキニング済み頂点をマスク描画から参照可能にする。
   bool PreparePlayerVertices(FrameContext& ctx);
   /// @brief CPU側の行列と影設定を各GPU定数バッファへ転送する。
   bool UpdateConstantBuffers();
   /// @brief マスクを黒でクリアし、全サブメッシュのシルエットを描画する。
   void DrawPlayerMask(FrameContext& ctx);
   /// @brief シーンの色・深度を保持して描画先、ビューポート、シザーを戻す。
   void RestoreSceneTarget(FrameContext& ctx);
   /// @brief 通常シーンと同じ惑星メッシュに、深度EQUAL・深度書込なしで影を合成する。
   void DrawReceiverShadow(FrameContext& ctx);

   GraphicsDevice* device_ = nullptr;
   PlayerShadowSettings settings_{};
   PlayerShadowFrameData frameData_{};
   bool initialized_ = false;
   bool hasFrameData_ = false;
   bool maskReady_ = false;
   std::string lastFailure_;
   std::vector<DrawGeometry> playerGeometry_;
   std::vector<DrawGeometry> receiverGeometry_;
   ShadowBindings bindings_{};

   Vector3 shadowUp_{};
   Vector3 groundPosition_{};
   Vector3 previousTangentUp_{ 0.0f, 0.0f, 1.0f };
   Matrix4x4 shadowViewProjection_{};

   Microsoft::WRL::ComPtr<ID3D12Resource> maskTexture_;
   Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> maskRtvHeap_;
   std::shared_ptr<SrvDescriptorAllocation> maskSrvAllocation_;
   D3D12_CPU_DESCRIPTOR_HANDLE maskRtvHandle_{};
   D3D12_GPU_DESCRIPTOR_HANDLE maskSrvHandle_{};
   D3D12_RESOURCE_STATES maskState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

   Microsoft::WRL::ComPtr<ID3D12Resource> maskConstantBuffer_;
   Microsoft::WRL::ComPtr<ID3D12Resource> projectConstantBuffer_;
   Microsoft::WRL::ComPtr<ID3D12Resource> parameterConstantBuffer_;
   MaskConstants* mappedMaskConstants_ = nullptr;
   ProjectConstants* mappedProjectConstants_ = nullptr;
   ShadowParameters* mappedParameters_ = nullptr;
};

} // namespace GameEngine
