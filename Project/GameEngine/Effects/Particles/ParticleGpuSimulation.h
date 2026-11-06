#pragma once

#include "GameEngine/Effects/Particles/ParticleSystem.h"
#include <array>
#include <span>

namespace GameEngine {

/// @brief 粒子Computeの資源と生成要求・同期・描画出力を一括所有する。
class ParticleGpuSimulation final {
public:
   /// @brief 未生成のGPU実行系を作成する。
   ParticleGpuSimulation();

   /// @brief 永続Mapと借用ディスクリプタを解除する。
   ~ParticleGpuSimulation();

   /// @brief 資源とディスクリプタの二重所有を禁止する。
   ParticleGpuSimulation(const ParticleGpuSimulation&) = delete;

   /// @brief 資源とディスクリプタの二重所有を禁止する。
   ParticleGpuSimulation& operator=(const ParticleGpuSimulation&) = delete;

   /// @brief 指定デバイス上に固定上限の粒子プールを生成する。
   void Create(GraphicsDevice& device);

   /// @brief 次回の実行でプール全体を初期化し、未反映要求を破棄する。
   void Reset();

   /// @brief CPUプール拡張分を次回のGPU初期化範囲へ追加する。
   void Grow(uint32_t previousCapacity);

   /// @brief 生成または衝突補正要求を容量内で受理する。
   void Queue(const ParticleSystem::GpuSpawnRequest& request);

   /// @brief カメラ・描画属性のフレーム設定を確定する。
   void PrepareFrame(const ParticleSystem::GpuSimulationSettings& settings);

   /// @brief 初期化・生成・運動更新・必要時の状態コピーを順に記録する。
   /// @return 全段の記録が完了した場合はtrue。
   bool DispatchSimulation(const PSOManager* psoManager, bool needsCpuState);

   /// @brief 更新済み状態をソート順の描画インスタンスへ変換する。
   void DispatchRender(const PSOManager* psoManager);

   /// @brief 必須資源をすべて生成済みか取得する。
   bool IsReady() const;

   /// @brief 前フレームの状態を待機せず参照する。コピー記録前は空。
   std::span<const ParticleSystem::GpuParticleState> Readback() const;

   /// @brief 今フレームのソート済み描画属性の書込み先を取得する。
   std::span<ParticleSystem::GpuParticleAttributes> Attributes();

   /// @brief 次のGPU更新時間（秒）を設定する。
   void SetDeltaTime(float deltaTime) { gpuDeltaTime_ = deltaTime; }

   /// @brief 設定済みのGPU更新時間（秒）を取得する。
   float GetDeltaTime() const { return gpuDeltaTime_; }

   /// @brief 描画出力数を無効化する。
   void ClearDrawCount() { gpuRenderParticleCount_ = 0; }

   /// @brief 描画出力の粒子数を取得する。
   uint32_t GetDrawCount() const { return gpuRenderParticleCount_; }

   /// @brief 描画出力SRVを取得する。
   D3D12_GPU_DESCRIPTOR_HANDLE GetOutputHandle() const { return gpuOutputSrvHandleGPU_; }

private:
   using ParticleForGPU = ParticleSystem::ParticleForGPU;
   using GpuParticleState = ParticleSystem::GpuParticleState;
   using GpuParticleMotion = ParticleSystem::GpuParticleMotion;
   using GpuParticleAttributes = ParticleSystem::GpuParticleAttributes;
   using GpuSpawnRequest = ParticleSystem::GpuSpawnRequest;
   using GpuSimulationSettings = ParticleSystem::GpuSimulationSettings;
   static constexpr uint32_t kMaxParticles = ParticleSystem::kMaxParticles;

   void Release();
   bool BindPipeline(const PSOManager& psoManager, const char* pipelineName) const;

   GraphicsDevice* device_ = nullptr; ///< 資源とディスクリプタの所有元への非所有参照。

   enum GpuDescriptorSlot : size_t {
      kGpuStateUavDescriptor,
      kGpuStateSrvDescriptor,
      kGpuAttributesSrvDescriptor,
      kGpuOutputUavDescriptor,
      kGpuOutputSrvDescriptor,
      kGpuMotionUavDescriptor,
      kGpuSpawnRequestSrvDescriptor,
      kGpuAliveUavDescriptor,
      kGpuFreeListUavDescriptor,
      kGpuFreeCountUavDescriptor,
      kGpuOwnerMappingUavDescriptor,
      kGpuOwnerMappingSrvDescriptor,
      kGpuDescriptorCount
   };

   /// @brief GPU粒子の位置・速度・寿命を保持する所有GPUリソース。
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuStateResource_ = nullptr;
   /// @brief GPU粒子の力・速度制限・ノイズ設定を保持する所有GPUリソース。
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuMotionResource_ = nullptr;
   /// @brief CPUからGPUへ渡す粒子生成・衝突補正要求を保持する所有GPUリソース。
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuSpawnRequestResource_ = nullptr;
   /// @brief GPU粒子スロットの生存フラグを保持する所有GPUリソース。
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuAliveResource_ = nullptr;
   /// @brief 未使用GPU粒子スロットのフリーリストを保持する所有GPUリソース。
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuFreeListResource_ = nullptr;
   /// @brief GPU粒子フリーリストの空き数を保持する所有GPUリソース。
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuFreeCountResource_ = nullptr;
   /// @brief CPU粒子番号からGPUスロットへの対応表を保持する所有GPUリソース。
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuOwnerMappingResource_ = nullptr;
   /// @brief ソート順に並べた粒子の色・大きさ・回転・UVを保持する所有GPUリソース。
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuAttributesResource_ = nullptr;
   /// @brief 描画用に変換した粒子インスタンスを保持する所有GPUリソース。
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuOutputResource_ = nullptr;
   /// @brief GPU粒子シミュレーションの共通設定を保持する所有GPUリソース。
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuSettingsResource_ = nullptr;

   // CPUソートは前フレームのGPU位置を非同期Readbackし、描画中のGPU待機を発生させない。
   /// @brief 前フレームのGPU粒子状態をCPUから読むためのコピーを保持する所有GPUリソース。
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuStateReadbackResource_ = nullptr;
   /// @brief CPUからGPUへ渡す粒子生成・衝突補正要求を参照するCPU側の永続Map先。対応リソースがメモリを所有する。
   GpuSpawnRequest* gpuSpawnRequestData_ = nullptr;
   /// @brief ソート順に並べた粒子の色・大きさ・回転・UVを参照するCPU側の永続Map先。
   /// 対応リソースがメモリを所有する。
   GpuParticleAttributes* gpuAttributesData_ = nullptr;
   /// @brief GPU粒子シミュレーションの共通設定を参照するCPU側の永続Map先。対応リソースがメモリを所有する。
   GpuSimulationSettings* gpuSettingsData_ = nullptr;
   /// @brief 前フレームのGPU粒子状態をCPUから読むためのコピーを参照するCPU側の永続Map先。
   /// 対応リソースがメモリを所有する。
   GpuParticleState* gpuStateReadbackData_ = nullptr;
   D3D12_GPU_DESCRIPTOR_HANDLE gpuStateUavHandleGPU_{}; ///< GPU粒子の位置・速度・寿命を書き込むUAVのGPU側ハンドル。
   D3D12_GPU_DESCRIPTOR_HANDLE gpuStateSrvHandleGPU_{}; ///< GPU粒子の位置・速度・寿命を読み出すSRVのGPU側ハンドル。
   /// @brief GPU粒子の力・速度制限・ノイズ設定を書き込むUAVのGPU側ハンドル。
   D3D12_GPU_DESCRIPTOR_HANDLE gpuMotionUavHandleGPU_{};
   /// @brief CPUからGPUへ渡す粒子生成・衝突補正要求を読み出すSRVのGPU側ハンドル。
   D3D12_GPU_DESCRIPTOR_HANDLE gpuSpawnRequestSrvHandleGPU_{};
   /// @brief GPU粒子スロットの生存フラグを書き込むUAVのGPU側ハンドル。
   D3D12_GPU_DESCRIPTOR_HANDLE gpuAliveUavHandleGPU_{};
   /// @brief 未使用GPU粒子スロットのフリーリストを書き込むUAVのGPU側ハンドル。
   D3D12_GPU_DESCRIPTOR_HANDLE gpuFreeListUavHandleGPU_{};
   /// @brief GPU粒子フリーリストの空き数を書き込むUAVのGPU側ハンドル。
   D3D12_GPU_DESCRIPTOR_HANDLE gpuFreeCountUavHandleGPU_{};
   /// @brief CPU粒子番号からGPUスロットへの対応表を書き込むUAVのGPU側ハンドル。
   D3D12_GPU_DESCRIPTOR_HANDLE gpuOwnerMappingUavHandleGPU_{};
   /// @brief CPU粒子番号からGPUスロットへの対応表を読み出すSRVのGPU側ハンドル。
   D3D12_GPU_DESCRIPTOR_HANDLE gpuOwnerMappingSrvHandleGPU_{};
   /// @brief ソート順に並べた粒子の色・大きさ・回転・UVを読み出すSRVのGPU側ハンドル。
   D3D12_GPU_DESCRIPTOR_HANDLE gpuAttributesSrvHandleGPU_{};
   /// @brief 描画用に変換した粒子インスタンスを書き込むUAVのGPU側ハンドル。
   D3D12_GPU_DESCRIPTOR_HANDLE gpuOutputUavHandleGPU_{};
   /// @brief 描画用に変換した粒子インスタンスを読み出すSRVのGPU側ハンドル。
   D3D12_GPU_DESCRIPTOR_HANDLE gpuOutputSrvHandleGPU_{};
   /// @brief 粒子GPU処理用に借りたディスクリプタ番号。未割り当てはUINT_MAX。
   std::array<UINT, kGpuDescriptorCount> gpuDescriptorIndices_{};
   /// @brief GPU粒子の位置・速度・寿命の現在のリソース遷移状態。
   D3D12_RESOURCE_STATES gpuStateResourceState_ = D3D12_RESOURCE_STATE_COMMON;
   /// @brief CPU粒子番号からGPUスロットへの対応表の現在のリソース遷移状態。
   D3D12_RESOURCE_STATES gpuOwnerMappingResourceState_ = D3D12_RESOURCE_STATE_COMMON;
   /// @brief 描画用に変換した粒子インスタンスの現在のリソース遷移状態。
   D3D12_RESOURCE_STATES gpuOutputResourceState_ = D3D12_RESOURCE_STATE_COMMON;
   uint32_t gpuRenderParticleCount_ = 0;      ///< 描画変換する粒子数。
   uint32_t gpuPendingSpawnRequestCount_ = 0; ///< 未反映の生成・補正要求数。
   bool gpuStateReadbackAvailable_ = false;   ///< 前フレーム状態を取得可能かどうか。
   uint32_t gpuReadbackParticleCount_ = 0;    ///< 最後にコピーを記録した状態の要素数。
   bool gpuNeedsInitialize_ = true;           ///< GPUプールの初期化要求。
   uint32_t gpuInitializationStartIndex_ = 0; ///< 追加初期化する先頭番号。
   float gpuDeltaTime_ = 0.0f;                ///< GPU更新時間（秒）。
};

} // namespace GameEngine
