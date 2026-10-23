struct RibbonSegment {
   float4 startAndWidth;    ///< xyzはリボン線分の始点、wは始点側の幅。
   float4 endAndWidth;      ///< xyzはリボン線分の終点、wは終点側の幅。
   float4 startTangentAndV; ///< xyzは始点の接線、wは長さ方向のテクスチャ座標。
   float4 endTangentAndV;   ///< xyzは終点の接線、wは長さ方向のテクスチャ座標。
   float4 alphaAndPadding;  ///< xはリボンの透明度、残りはGPU配置調整用の未使用領域。
};

struct ParticleVertex {
   float4 position; ///< 描画形状を構成する基準頂点の位置。
   float2 texCoord; ///< テクスチャ座標。
   float3 normal;   ///< 面の向きを表す法線ベクトル。
};

cbuffer RibbonSettings : register(b0) {
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
   uint gSegmentCount;               ///< 今回のGPU処理で生成するリボン線分数。
   uint3 gPadding;                   ///< GPU転送データの配置境界を揃える未使用領域。
};

StructuredBuffer<RibbonSegment> gSegments : register(t0);
RWStructuredBuffer<ParticleVertex> gVertices : register(u0);
RWStructuredBuffer<uint> gIndices : register(u1);

float3 SafeNormalize(float3 value, float3 fallbackValue) {
   float lengthSquared = dot(value, value);
   return lengthSquared > 0.000001f ? value * rsqrt(lengthSquared) : fallbackValue;
}

[numthreads(64, 1, 1)] void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
   uint segmentIndex = dispatchThreadId.x;

   if (segmentIndex >= gSegmentCount) return;

   RibbonSegment segment = gSegments[segmentIndex];
   float3 start = segment.startAndWidth.xyz;
   float3 end = segment.endAndWidth.xyz;
   float startHalfWidth = segment.startAndWidth.w * 0.5f;
   float endHalfWidth = segment.endAndWidth.w * 0.5f;
   float3 startTangent = SafeNormalize(segment.startTangentAndV.xyz, float3(0.0f, 1.0f, 0.0f));
   float3 endTangent = SafeNormalize(segment.endTangentAndV.xyz, startTangent);
   float3 startView = SafeNormalize(gCameraPosition.xyz - start, -gCameraForward.xyz);
   float3 endView = SafeNormalize(gCameraPosition.xyz - end, -gCameraForward.xyz);

   // 隣接区間から平均した点接線を使い、各区間の端を同じ左右位置へ揃える。
   float3 startSide = SafeNormalize(cross(startTangent, startView), gCameraRight.xyz) * startHalfWidth;
   float3 endSide = SafeNormalize(cross(endTangent, endView), gCameraRight.xyz) * endHalfWidth;

   uint vertexBase = segmentIndex * 4u;
   ParticleVertex vertex;
   vertex.position = float4(start - startSide, 1.0f);
   vertex.texCoord = float2(0.0f, segment.startTangentAndV.w);

   // Particle VSはリボンインスタンスだけnormal.xを頂点アルファとして解釈する。
   vertex.normal = float3(saturate(segment.alphaAndPadding.x), 0.0f, 0.0f);
   gVertices[vertexBase] = vertex;
   vertex.position = float4(start + startSide, 1.0f);
   vertex.texCoord = float2(1.0f, segment.startTangentAndV.w);
   gVertices[vertexBase + 1u] = vertex;
   vertex.position = float4(end - endSide, 1.0f);
   vertex.texCoord = float2(0.0f, segment.endTangentAndV.w);
   vertex.normal = float3(saturate(segment.alphaAndPadding.y), 0.0f, 0.0f);
   gVertices[vertexBase + 2u] = vertex;
   vertex.position = float4(end + endSide, 1.0f);
   vertex.texCoord = float2(1.0f, segment.endTangentAndV.w);
   gVertices[vertexBase + 3u] = vertex;

   uint indexBase = segmentIndex * 6u;
   gIndices[indexBase] = vertexBase;
   gIndices[indexBase + 1u] = vertexBase + 2u;
   gIndices[indexBase + 2u] = vertexBase + 1u;
   gIndices[indexBase + 3u] = vertexBase + 1u;
   gIndices[indexBase + 4u] = vertexBase + 2u;
   gIndices[indexBase + 5u] = vertexBase + 3u;
}
