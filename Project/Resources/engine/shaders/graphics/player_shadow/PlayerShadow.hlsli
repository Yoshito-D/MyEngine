#ifndef PLAYER_SHADOW_HLSLI
#define PLAYER_SHADOW_HLSLI

struct PlayerShadowMaskConstants {
    row_major float4x4 playerShadowWVP;
};

struct PlayerShadowProjectConstants {
    row_major float4x4 worldViewProjection;
    row_major float4x4 world;
    row_major float4x4 worldInverseTranspose;
    row_major float4x4 shadowViewProjection;
};

struct PlayerShadowParameters {
    float4 shadowUpAndOpacity;
    float4 groundPositionAndRange;
};

struct PlayerShadowVertexInput {
    float4 position : POSITION0;
    float2 texCoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct PlayerShadowMaskVertexOutput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

struct PlayerShadowProjectVertexOutput {
    float4 position : SV_POSITION;
    float4 shadowPosition : TEXCOORD0;
    float3 worldPosition : TEXCOORD1;
    float3 worldNormal : TEXCOORD2;
};

#endif
