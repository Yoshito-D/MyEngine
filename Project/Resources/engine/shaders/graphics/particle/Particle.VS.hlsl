#include "Particle.hlsli"

struct ParticleForGPU {
    float4x4 wVP;
    float4x4 world;
    float4x4 uvTransform;
    float4 color;
    float4 customData;
};

StructuredBuffer<ParticleForGPU> gParticle : register(t0);

struct VertexShaderInput {
    float4 position : POSITION0;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID) {
    VertexShaderOutput output;
    output.position = mul(input.position, gParticle[instanceId].wVP);
    output.texCoord = input.texCoord;
    output.color = gParticle[instanceId].color;
    output.uvTransform0 = gParticle[instanceId].uvTransform[0];
    output.uvTransform1 = gParticle[instanceId].uvTransform[1];
    output.uvTransform2 = gParticle[instanceId].uvTransform[2];
    output.uvTransform3 = gParticle[instanceId].uvTransform[3];
    output.customData = gParticle[instanceId].customData;
    return output;
}
