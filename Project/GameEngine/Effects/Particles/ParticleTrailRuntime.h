#pragma once

#include "GameEngine/Effects/Particles/ParticleSystem.h"
#include <array>
#include <deque>
#include <span>

namespace GameEngine {

/// @brief 生存粒子の履歴と死亡後の巻き取り、およびリボン生成資源を管理する。
class ParticleTrailRuntime final {
public:
   /// @brief 未生成のトレイル実行系を作成する。
   ParticleTrailRuntime() = default;

   /// @brief 永続Mapとリボン用ディスクリプタを解除する。
   ~ParticleTrailRuntime();

   /// @brief 履歴・GPU資源・ディスクリプタの二重所有を禁止する。
   ParticleTrailRuntime(const ParticleTrailRuntime&) = delete;

   /// @brief 履歴・GPU資源・ディスクリプタの二重所有を禁止する。
   ParticleTrailRuntime& operator=(const ParticleTrailRuntime&) = delete;

   /// @brief 単位行列インスタンスとリボン用定数を生成する。
   void Create(GraphicsDevice& device);

   /// @brief 履歴の方式と距離閾値に応じて粒子の位置を記録する。
   /// @param emitterPosition エミッターのワールド位置（m）。
   void UpdatePoints(Particle& particle, const TrailModule& module, const Vector3& emitterPosition);

   /// @brief 方式変更時に旧方式の履歴を破棄する。
   void Synchronize(std::span<Particle> particles, const TrailModule& module, const Vector3& emitterPosition);

   /// @brief 死亡した粒子の履歴を巻き取り用に引き取る。
   void Detach(Particle& particle, const TrailModule& module);

   /// @brief 切り離された履歴の時間（秒）を進め、期限切れを破棄する。
   void UpdateDetached(float deltaTime, const TrailModule& module);

   /// @brief 生存中と巻き取り中の履歴をGPUへの線分入力へ変換する。
   void Build(Camera* camera, std::span<Particle> particles, const TrailModule& module, const Vector3& emitterPosition);

   /// @brief リボンComputeの書込みと描画への遷移を記録する。
   void Dispatch(const PSOManager* psoManager);

   /// @brief 切り離し済み履歴と描画数を消去する。
   void Clear();

   /// @brief GPUへの今回の描画数を無効化する。
   void ClearDrawCount();

   /// @brief 必須リソースを生成済みか取得する。
   bool IsReady() const;

   /// @brief 巻き取り中の履歴が存在するか取得する。
   bool HasDetached() const { return !detachedRibbons_.empty(); }

   /// @brief 描画用の頂点ビューを取得する。
   const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return gpuRibbonVertexBufferView_; }

   /// @brief 描画用のインデックスビューを取得する。
   const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { return gpuRibbonIndexBufferView_; }

   /// @brief 描画用のインデックス数を取得する。
   uint32_t GetIndexCount() const { return gpuRibbonIndexCount_; }

   /// @brief ワールド空間の生成済み頂点へ使う単位行列SRVを取得する。
   D3D12_GPU_DESCRIPTOR_HANDLE GetInstancingHandle() const { return instancingSrvHandleGPU_; }

private:
   using ParticleForGPU = ParticleSystem::ParticleForGPU;
   using GpuSimulationSettings = ParticleSystem::GpuSimulationSettings;
   using GpuRibbonSegment = ParticleSystem::GpuRibbonSegment;
   static constexpr uint32_t kMaxParticles = ParticleSystem::kMaxParticles;

   void Release();
   void EnsureResources(uint32_t requiredSegmentCount);

   /// @brief 親粒子の消滅後に末尾から巻き取るリボン履歴
   struct DetachedRibbon {
      std::vector<Vector3> points;     ///< リボン形状を構成する位置履歴。
      float width = 0.5f;              ///< 対象領域の横幅。
      float age = 0.0f;                ///< 生成または処理開始からの経過時間（秒）。
      float retractionDuration = 0.5f; ///< 死亡後のリボンを巻き取って消す時間（秒）。
   };

   std::deque<DetachedRibbon> detachedRibbons_; ///< 親粒子の死亡後も保持する所有履歴。
   TrailModule::TrailMode lastTrailMode_ = TrailModule::TrailMode::ParticlePath;
   GraphicsDevice* device_ = nullptr; ///< リボン資源の生成元への非所有参照。
   // ワールド空間で生成するリボンを単一インスタンスとして描画する。
   /// @brief リボン用の単位行列と表示色を保持する所有GPUリソース。
   Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_ = nullptr;
   /// @brief リボン描画設定の永続Map先。対応リソースがメモリを所有する。
   ParticleForGPU* instancingData_ = nullptr;
   /// @brief ワールド空間で生成済みのリボン頂点を描画するための単位行列SRV。
   D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};
   UINT instancingSrvIndex_ = UINT_MAX; // 解放用に記録

   /// @brief リボンを構築する線分と端点の入力を保持する所有GPUリソース。
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuRibbonInputResource_ = nullptr;
   /// @brief GPUリボン生成の共通設定を保持する所有GPUリソース。
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuRibbonSettingsResource_ = nullptr;
   /// @brief GPUで生成したリボン頂点を保持する所有GPUリソース。
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuRibbonVertexResource_ = nullptr;
   /// @brief GPUで生成したリボン頂点の参照番号を保持する所有GPUリソース。
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuRibbonIndexResource_ = nullptr;
   /// @brief リボンを構築する線分と端点の入力を参照するCPU側の永続Map先。対応リソースがメモリを所有する。
   GpuRibbonSegment* gpuRibbonInputData_ = nullptr;
   /// @brief GPUリボン生成の共通設定を参照するCPU側の永続Map先。対応リソースがメモリを所有する。
   GpuSimulationSettings* gpuRibbonSettingsData_ = nullptr;
   /// @brief リボンを構築する線分と端点の入力を読み出すSRVのGPU側ハンドル。
   D3D12_GPU_DESCRIPTOR_HANDLE gpuRibbonInputSrvHandleGPU_{};
   /// @brief GPUで生成したリボン頂点を書き込むUAVのGPU側ハンドル。
   D3D12_GPU_DESCRIPTOR_HANDLE gpuRibbonVertexUavHandleGPU_{};
   /// @brief GPUで生成したリボン頂点の参照番号を書き込むUAVのGPU側ハンドル。
   D3D12_GPU_DESCRIPTOR_HANDLE gpuRibbonIndexUavHandleGPU_{};
   /// @brief リボン入力SRVと出力UAVに借りたディスクリプタ番号。
   std::array<UINT, 3> gpuRibbonDescriptorIndices_{UINT_MAX, UINT_MAX, UINT_MAX};
   D3D12_VERTEX_BUFFER_VIEW gpuRibbonVertexBufferView_{}; ///< リボン描画用GPU頂点バッファビュー。
   D3D12_INDEX_BUFFER_VIEW gpuRibbonIndexBufferView_{};   ///< リボン描画用GPUインデックスバッファビュー。
   /// @brief GPUで生成したリボン頂点の現在のリソース遷移状態。
   D3D12_RESOURCE_STATES gpuRibbonVertexState_ = D3D12_RESOURCE_STATE_COMMON;
   /// @brief GPUで生成したリボン頂点の参照番号の現在のリソース遷移状態。
   D3D12_RESOURCE_STATES gpuRibbonIndexState_ = D3D12_RESOURCE_STATE_COMMON;
   uint32_t gpuRibbonSegmentCapacity_ = 0; ///< 再確保せずに扱えるリボン線分数の上限。
   uint32_t gpuRibbonSegmentCount_ = 0;    ///< 今回GPUへ渡すリボン線分数。
   uint32_t gpuRibbonIndexCount_ = 0;      ///< リボン描画用インデックス数。
};

} // namespace GameEngine
