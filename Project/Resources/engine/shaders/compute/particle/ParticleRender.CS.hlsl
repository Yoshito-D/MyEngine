static const uint INVALID_PARTICLE_INDEX = 0xffffffffu;

struct GpuParticleState {
   float4 positionAndActive;   ///< xyzは粒子の位置、wは生存フラグ。
   float4 velocityAndLifetime; ///< xyzは粒子速度、wは残り寿命（秒）。
   uint ownerParticleIndex;    ///< 対応するCPU粒子番号。UINT_MAXは未割り当て。
   float age;                  ///< 生成または処理開始からの経過時間（秒）。
   float initialLifetime;      ///< 生成時に決めた粒子寿命（秒）。
   uint padding;               ///< GPU転送データの配置境界を揃える未使用領域。
};

struct GpuParticleAttributes {
   float4x4 uvTransform;      ///< UV変換行列。
   float4 color;              ///< 表示色のRGBA成分。
   float4 sizeAndRotation;    ///< xyzは粒子の各軸サイズ、wはZ軸回転角（ラジアン）。
   float4 rotationQuaternion; ///< 姿勢を表す回転クォータニオン。
   float4 customData;         ///< シェーダー連携用（年齢、乱数、カメラフェード、速度）。
   uint stateIndex;           ///< 圧縮済み描画属性から永続GPU状態を引くための元粒子番号。
   uint3 padding;             ///< GPU転送データの配置境界を揃える未使用領域。
};

struct ParticleForGPU {
   float4x4 wVP;         ///< ワールド・ビュー・プロジェクション行列。
   float4x4 world;       ///< ローカル頂点をワールド座標系へ移す変換行列。
   float4x4 uvTransform; ///< UV変換行列。
   float4 color;         ///< 表示色のRGBA成分。
   float4 customData;    ///< シェーダー連携用（年齢、乱数、カメラフェード、速度）。
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

StructuredBuffer<GpuParticleAttributes> gAttributes : register(t0);
StructuredBuffer<GpuParticleState> gStates : register(t1);
StructuredBuffer<uint> gOwnerMappings : register(t2);
RWStructuredBuffer<ParticleForGPU> gOutputParticles : register(u0);

float3 SafeNormalize(float3 value, float3 fallbackValue) {
   float lengthSquared = dot(value, value);
   return lengthSquared > 0.000001f ? value * rsqrt(lengthSquared) : fallbackValue;
}

float3 RotateByQuaternion(float3 value, float4 quaternion) {
   float3 quaternionVector = quaternion.xyz;
   return value + 2.0f * cross(quaternionVector, cross(quaternionVector, value) + quaternion.w * value);
}

void RotateBillboardPlane(inout float3 right, inout float3 up, float angle) {
   float sine;
   float cosine;
   sincos(angle, sine, cosine);
   float3 originalRight = right;
   right = originalRight * cosine + up * sine;
   up = -originalRight * sine + up * cosine;
}

ParticleForGPU MakeInactiveParticle() {
   ParticleForGPU particle;
   particle.wVP = (float4x4)0;
   particle.world = (float4x4)0;
   particle.uvTransform = (float4x4)0;
   particle.color = 0.0f;
   particle.customData = 0.0f;
   return particle;
}

[numthreads(64, 1, 1)] void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
   uint outputIndex = dispatchThreadId.x;

   if (outputIndex >= gParticleCount) return;

   GpuParticleAttributes attributes = gAttributes[outputIndex];
   uint ownerIndex = attributes.stateIndex;
   uint stateIndex = ownerIndex < gParticleCapacity ? gOwnerMappings[ownerIndex] : INVALID_PARTICLE_INDEX;

   if (stateIndex >= gParticleCapacity) {
      gOutputParticles[outputIndex] = MakeInactiveParticle();
      return;
   }

   GpuParticleState state = gStates[stateIndex];

   if (state.ownerParticleIndex != ownerIndex || state.positionAndActive.w <= 0.5f) {
      gOutputParticles[outputIndex] = MakeInactiveParticle();
      return;
   }

   float3 position = state.positionAndActive.xyz;
   float3 scale = attributes.sizeAndRotation.xyz;
   float3 right = float3(1.0f, 0.0f, 0.0f);
   float3 up = float3(0.0f, 1.0f, 0.0f);
   float3 forward = float3(0.0f, 0.0f, 1.0f);
   uint billboardType = (uint)gRenderParams.x;
   bool velocityStretchEnabled = gRenderParams.w > 0.5f;

   if (velocityStretchEnabled || billboardType >= 4u) {
      up = SafeNormalize(state.velocityAndLifetime.xyz, gCameraUp.xyz);
      right = SafeNormalize(cross(gCameraForward.xyz, up), gCameraRight.xyz);
      forward = SafeNormalize(cross(right, up), gCameraForward.xyz);
   } else if (billboardType == 0u) {
      right = SafeNormalize(RotateByQuaternion(right, attributes.rotationQuaternion), right);
      up = SafeNormalize(RotateByQuaternion(up, attributes.rotationQuaternion), up);
      forward = SafeNormalize(RotateByQuaternion(forward, attributes.rotationQuaternion), forward);
   } else if (billboardType == 1u) {
      forward = SafeNormalize(position - gCameraPosition.xyz, gCameraForward.xyz);
      right = SafeNormalize(cross(gCameraUp.xyz, forward), gCameraRight.xyz);
      up = SafeNormalize(cross(forward, right), gCameraUp.xyz);
      RotateBillboardPlane(right, up, attributes.sizeAndRotation.w);
   } else if (billboardType == 2u || billboardType == 3u) {
      up = float3(0.0f, 1.0f, 0.0f);
      forward = position - gCameraPosition.xyz;
      forward.y = 0.0f;
      forward = SafeNormalize(forward, float3(0.0f, 0.0f, 1.0f));
      right = SafeNormalize(cross(up, forward), float3(1.0f, 0.0f, 0.0f));
      RotateBillboardPlane(right, up, attributes.sizeAndRotation.w);
   }

   // 方向合わせと長さ変更を分離し、トレイルとも独立して選択できるようにする。
   if (velocityStretchEnabled) {
      float speed = length(state.velocityAndLifetime.xyz);
      scale.y *= 1.0f + speed * gRenderParams.y * gRenderParams.z;
   }

   right *= scale.x;
   up *= scale.y;
   forward *= scale.z;
   float4x4 world = float4x4(float4(right, 0.0f), float4(up, 0.0f), float4(forward, 0.0f), float4(position, 1.0f));

   ParticleForGPU outputParticle;
   outputParticle.world = world;
   outputParticle.wVP = mul(world, gViewProjection);
   outputParticle.uvTransform = attributes.uvTransform;
   outputParticle.color = attributes.color;
   outputParticle.color.a *= state.positionAndActive.w;
   outputParticle.customData = attributes.customData;
   outputParticle.customData.w = length(state.velocityAndLifetime.xyz);

   if (gCameraFadeParams.x > 0.5f) {
      float fadeNear = gCameraFadeParams.y;
      float fadeFar = max(gCameraFadeParams.z, fadeNear + 0.0001f);
      outputParticle.customData.z =
         saturate((distance(position, gCameraPosition.xyz) - fadeNear) / (fadeFar - fadeNear));
   } else {
      outputParticle.customData.z = 1.0f;
   }

   gOutputParticles[outputIndex] = outputParticle;
}
