static const uint INVALID_PARTICLE_INDEX = 0xffffffffu;

struct GpuParticleState {
   float4 positionAndActive;   ///< xyzは粒子の位置、wは生存フラグ。
   float4 velocityAndLifetime; ///< xyzは粒子速度、wは残り寿命（秒）。
   uint ownerParticleIndex;    ///< 対応するCPU粒子番号。UINT_MAXは未割り当て。
   float age;                  ///< 生成または処理開始からの経過時間（秒）。
   float initialLifetime;      ///< 生成時に決めた粒子寿命（秒）。
   uint padding;               ///< GPU転送データの配置境界を揃える未使用領域。
};

cbuffer SimulationSettings : register(b0) {
   float4x4 gViewProjection;         ///< ワールド座標をクリップ空間へ移すビュー・投影合成行列。
   float4 gCameraPosition;           ///< 描画に使うカメラのワールド位置。
   float4 gCameraRight;              ///< カメラのワールド右方向。
   float4 gCameraUp;                 ///< カメラのワールド上方向。
   float4 gCameraForward;            ///< xyzはカメラの前方、wはGPU粒子更新の時間刻み（秒）。
   float4 gRenderParams;             ///< xはビルボード種類、yは速度倍率、zは長さ倍率、wは速度ストレッチ有効値。
   float4 gCameraFadeParams;         ///< xは距離フェードの有効値、yは開始距離、zは終了距離。
   float4 gAttractorPosition;        ///< 粒子を引き寄せる中心のワールド位置。
   float4 gAttractorParams;          ///< xは吸引強度、yは有効半径、zは減衰指数、wは吸引の有効値。
   float4 gSimulationOriginAndLocal; ///< xyzはエミッター原点、wはローカル空間で評価するかのフラグ。
   float4 gSimulationRotation;       ///< ローカル設定をワールド方向へ回転するエミッターのクォータニオン。
   uint gParticleCount;              ///< 描画出力へ詰める有効粒子数。
   uint gParticleCapacity;           ///< GPU粒子プールに確保したスロット数。
   uint gSpawnRequestCount;          ///< 今回のDispatchで処理する生成・衝突補正要求数。
   uint gInitializationStartIndex;   ///< 初期化するGPU粒子スロットの先頭番号。
};

RWStructuredBuffer<GpuParticleState> gStates : register(u0);
RWStructuredBuffer<uint> gAliveFlags : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);
RWByteAddressBuffer gFreeCount : register(u3);
RWStructuredBuffer<uint> gOwnerMappings : register(u4);

[numthreads(64, 1, 1)] void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
   uint localIndex = dispatchThreadId.x;
   uint index = gInitializationStartIndex + localIndex;

   if (index >= gParticleCapacity) return;

   GpuParticleState state = (GpuParticleState)0;
   state.ownerParticleIndex = INVALID_PARTICLE_INDEX;
   gStates[index] = state;

   uint previousValue;
   InterlockedExchange(gAliveFlags[index], 0u, previousValue);
   InterlockedExchange(gOwnerMappings[index], INVALID_PARTICLE_INDEX, previousValue);

   if (gInitializationStartIndex == 0u) {
      // stackの末尾から0,1,2...の順にpopでき、再現性のある初回割り当てになる。
      gFreeList[index] = gParticleCapacity - 1u - index;

      if (index == 0u) {
         gFreeCount.InterlockedExchange(0u, gParticleCapacity, previousValue);
      }
   } else {
      // 実行中の容量拡張では既存状態を保持し、新規slotだけをatomicに公開する。
      uint freeSlot;
      gFreeCount.InterlockedAdd(0u, 1u, freeSlot);

      if (freeSlot < gParticleCapacity) {
         gFreeList[freeSlot] = index;
      } else {
         uint ignored;
         gFreeCount.InterlockedAdd(0u, 0xffffffffu, ignored);
      }
   }
}
