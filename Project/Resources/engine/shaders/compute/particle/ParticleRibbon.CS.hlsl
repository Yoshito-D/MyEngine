struct RibbonSegment
{
    float4 startAndWidth;
    float4 endAndWidth;
    float4 startTangentAndV;
    float4 endTangentAndV;
    float4 alphaAndPadding;
};

struct ParticleVertex
{
    float4 position;
    float2 texCoord;
    float3 normal;
};

cbuffer RibbonSettings : register(b0)
{
    float4x4 gViewProjection;
    float4 gCameraPosition;
    float4 gCameraRight;
    float4 gCameraUp;
    float4 gCameraForward;
    float4 gRenderParams;
    float4 gCameraFadeParams;
    float4 gAttractorPosition;
    float4 gAttractorParams;
    float4 gSimulationOriginAndLocal;
    float4 gSimulationRotation;
    uint gSegmentCount;
    uint3 gPadding;
};

StructuredBuffer<RibbonSegment> gSegments : register(t0);
RWStructuredBuffer<ParticleVertex> gVertices : register(u0);
RWStructuredBuffer<uint> gIndices : register(u1);

float3 SafeNormalize(float3 value, float3 fallbackValue)
{
    float lengthSquared = dot(value, value);
    return lengthSquared > 0.000001f ? value * rsqrt(lengthSquared) : fallbackValue;
}

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint segmentIndex = dispatchThreadId.x;
    if (segmentIndex >= gSegmentCount)
    {
        return;
    }

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
