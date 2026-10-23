static const uint INVALID_PARTICLE_INDEX = 0xffffffffu;
static const uint RESERVED_PARTICLE_INDEX = 0xfffffffeu;

struct GpuParticleState {
   float4 positionAndActive;   ///< xyzは粒子の位置、wは生存フラグ。
   float4 velocityAndLifetime; ///< xyzは粒子速度、wは残り寿命（秒）。
   uint ownerParticleIndex;    ///< 対応するCPU粒子番号。UINT_MAXは未割り当て。
   float age;                  ///< 生成または処理開始からの経過時間（秒）。
   float initialLifetime;      ///< 生成時に決めた粒子寿命（秒）。
   uint padding;               ///< GPU転送データの配置境界を揃える未使用領域。
};

struct GpuParticleMotion {
   float4 forceAndDrag;             ///< xyzは外力、wは速度の抵抗係数。
   float4 velocityAndSpeedModifier; ///< xyzは速度加算値、wは速度倍率。
   float4 limitAndGravity;          ///< xは速度上限、yは制限時の減衰率、zは重力倍率。
   float4 noiseParams;              ///< xはノイズ強度、yは空間周波数、zは位相の進行速度。
};

struct GpuSpawnRequest {
   GpuParticleState state;   ///< 生成する粒子の位置・速度・寿命・CPU所有番号。
   GpuParticleMotion motion; ///< GPUで積分する外力・速度倍率・制限・ノイズの設定。
   uint operation;           ///< 0: FreeListから生成、1: 既存所有粒子を上書き。
   uint3 padding;            ///< GPU転送データの配置境界を揃える未使用領域。
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
   uint gPadding;                    ///< GPU転送データの配置境界を揃える未使用領域。
};

StructuredBuffer<GpuSpawnRequest> gSpawnRequests : register(t0);
RWStructuredBuffer<GpuParticleState> gStates : register(u0);
RWStructuredBuffer<GpuParticleMotion> gMotions : register(u1);
RWStructuredBuffer<uint> gAliveFlags : register(u2);
RWStructuredBuffer<uint> gFreeList : register(u3);
RWByteAddressBuffer gFreeCount : register(u4);
RWStructuredBuffer<uint> gOwnerMappings : register(u5);

uint PopFreeParticle() {
   // Update CSとの間のUAV barrierにより、counterが示すslotは必ず公開済みである。
   // 同一Emitter dispatch内ではpushしないため、CASによるcounter予約だけで競合を解決できる。
   uint availableCount = gFreeCount.Load(0u);

   while (availableCount > 0u) {
      uint observedCount;
      gFreeCount.InterlockedCompareExchange(0u, availableCount, availableCount - 1u, observedCount);

      if (observedCount == availableCount) {
         return gFreeList[availableCount - 1u];
      }

      availableCount = observedCount;
   }

   return INVALID_PARTICLE_INDEX;
}

[numthreads(64, 1, 1)] void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
   uint requestIndex = dispatchThreadId.x;

   if (requestIndex >= gSpawnRequestCount) return;

   GpuSpawnRequest request = gSpawnRequests[requestIndex];
   uint ownerIndex = request.state.ownerParticleIndex;

   if (ownerIndex >= gParticleCapacity) return;

   if (request.operation == 1u) {
      uint existingIndex = gOwnerMappings[ownerIndex];

      if (existingIndex < gParticleCapacity && gAliveFlags[existingIndex] != 0u) {
         GpuParticleState state = gStates[existingIndex];
         state.positionAndActive.xyz = request.state.positionAndActive.xyz;
         state.velocityAndLifetime.xyz = request.state.velocityAndLifetime.xyz;
         gStates[existingIndex] = state;
         gMotions[existingIndex] = request.motion;
      }

      return;
   }

   // ownerを先に予約し、同一ownerへの重複要求がFreeListを消費しないようにする。
   uint previousMapping;
   InterlockedCompareExchange(
      gOwnerMappings[ownerIndex], INVALID_PARTICLE_INDEX, RESERVED_PARTICLE_INDEX, previousMapping);

   if (previousMapping != INVALID_PARTICLE_INDEX) return;

   uint particleIndex = PopFreeParticle();

   if (particleIndex == INVALID_PARTICLE_INDEX) {
      uint ignored;
      InterlockedCompareExchange(gOwnerMappings[ownerIndex], RESERVED_PARTICLE_INDEX, INVALID_PARTICLE_INDEX, ignored);
      return;
   }

   uint previousAlive;
   InterlockedCompareExchange(gAliveFlags[particleIndex], 0u, 1u, previousAlive);

   if (previousAlive != 0u) {
      uint ignored;
      InterlockedCompareExchange(gOwnerMappings[ownerIndex], RESERVED_PARTICLE_INDEX, INVALID_PARTICLE_INDEX, ignored);
      return;
   }

   // FreeList破損時にも生存粒子を壊さないよう、aliveの所有権を得てから状態を公開する。
   gStates[particleIndex] = request.state;
   gMotions[particleIndex] = request.motion;
   InterlockedExchange(gOwnerMappings[ownerIndex], particleIndex, previousMapping);
}
