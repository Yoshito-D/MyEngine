static const uint INVALID_PARTICLE_INDEX = 0xffffffffu;

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

RWStructuredBuffer<GpuParticleState> gStates : register(u0);
RWStructuredBuffer<GpuParticleMotion> gMotions : register(u1);
RWStructuredBuffer<uint> gAliveFlags : register(u2);
RWStructuredBuffer<uint> gFreeList : register(u3);
RWByteAddressBuffer gFreeCount : register(u4);
RWStructuredBuffer<uint> gOwnerMappings : register(u5);

float3 RotateByQuaternion(float3 value, float4 quaternion) {
   float3 quaternionVector = quaternion.xyz;
   return value + 2.0f * cross(quaternionVector, cross(quaternionVector, value) + quaternion.w * value);
}

void RecycleParticle(uint particleIndex, inout GpuParticleState state) {
   uint previousAlive;
   InterlockedCompareExchange(gAliveFlags[particleIndex], 1u, 0u, previousAlive);

   if (previousAlive != 1u) return;

   state.positionAndActive.w = 0.0f;
   uint ignored;

   if (state.ownerParticleIndex < gParticleCapacity) {
      InterlockedCompareExchange(
         gOwnerMappings[state.ownerParticleIndex], particleIndex, INVALID_PARTICLE_INDEX, ignored);
   }

   uint freeSlot;
   gFreeCount.InterlockedAdd(0u, 1u, freeSlot);

   if (freeSlot < gParticleCapacity) {
      // 次のEmitter CSはdispatch後のUAV barrierを待つため、counterとslotを一体として観測する。
      gFreeList[freeSlot] = particleIndex;
   } else {
      // CASで二重解放を防いでいるが、破損時にもcounterを容量内へ戻す。
      gFreeCount.InterlockedAdd(0u, 0xffffffffu, ignored);
   }
}

[numthreads(64, 1, 1)] void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
   uint particleIndex = dispatchThreadId.x;

   if (particleIndex >= gParticleCapacity || gAliveFlags[particleIndex] == 0u) return;

   GpuParticleState state = gStates[particleIndex];
   GpuParticleMotion motion = gMotions[particleIndex];
   float deltaTime = gCameraForward.w;

   if (state.velocityAndLifetime.w <= deltaTime) {
      state.velocityAndLifetime.w = 0.0f;
      RecycleParticle(particleIndex, state);
      gStates[particleIndex] = state;
      return;
   }

   state.velocityAndLifetime.xyz += motion.velocityAndSpeedModifier.xyz * deltaTime;
   state.velocityAndLifetime.xyz *= motion.velocityAndSpeedModifier.w;
   state.velocityAndLifetime.xyz *= exp(-max(motion.forceAndDrag.w, 0.0f) * deltaTime);

   float speed = length(state.velocityAndLifetime.xyz);
   float speedLimit = motion.limitAndGravity.x;

   if (speedLimit >= 0.0f && speed > speedLimit && speed > 0.000001f) {
      float excess = speed - speedLimit;
      state.velocityAndLifetime.xyz *= 1.0f - motion.limitAndGravity.y * excess / speed;
   }

   if (motion.noiseParams.x != 0.0f) {
      float3 samplePosition = state.positionAndActive.xyz;
      bool useLocalSimulation = gSimulationOriginAndLocal.w > 0.5f;

      if (useLocalSimulation) {
         float4 inverseRotation = float4(-gSimulationRotation.xyz, gSimulationRotation.w);
         samplePosition = RotateByQuaternion(samplePosition - gSimulationOriginAndLocal.xyz, inverseRotation);
      }

      float phase = state.age * motion.noiseParams.z;
      float3 noiseVelocity = float3(sin(samplePosition.x * motion.noiseParams.y + phase),
                                sin(samplePosition.y * motion.noiseParams.y + phase * 1.3f),
                                sin(samplePosition.z * motion.noiseParams.y + phase * 0.7f)) *
                             motion.noiseParams.x;

      if (useLocalSimulation) {
         noiseVelocity = RotateByQuaternion(noiseVelocity, gSimulationRotation);
      }

      state.velocityAndLifetime.xyz += noiseVelocity * deltaTime;
   }

   float3 acceleration = motion.forceAndDrag.xyz + float3(0.0f, -9.8f * motion.limitAndGravity.z, 0.0f);

   if (gAttractorParams.w > 0.5f) {
      float3 toTarget = gAttractorPosition.xyz - state.positionAndActive.xyz;
      float distanceToTarget = length(toTarget);

      if (distanceToTarget > 0.0001f && (gAttractorParams.y <= 0.0f || distanceToTarget <= gAttractorParams.y)) {
         float attenuation = 1.0f;

         if (gAttractorParams.z > 0.0f) {
            attenuation = gAttractorParams.y > 0.0f
                             ? pow(max(1.0f - distanceToTarget / gAttractorParams.y, 0.0f), gAttractorParams.z)
                             : 1.0f / pow(max(distanceToTarget, 1.0f), gAttractorParams.z);
         }

         acceleration += toTarget / distanceToTarget * (gAttractorParams.x * attenuation);
      }
   }

   state.velocityAndLifetime.xyz += acceleration * deltaTime;
   state.positionAndActive.xyz += state.velocityAndLifetime.xyz * deltaTime;
   state.velocityAndLifetime.w -= deltaTime;
   state.age += deltaTime;
   gStates[particleIndex] = state;
}
