#include "Particle.hlsli"

struct ParticleForGPU {
    float32_t4x4 wVP;
    float32_t4x4 world;
    float32_t4 color;
};

StructuredBuffer<ParticleForGPU> gParticle : register(t0);

struct VertexShaderInput {
    float32_t4 position : POSITION0;
    float32_t2 texCoord : TEXCOORD0;
    float32_t4 color : COLOR0;
};

VertexShaderOutput main(VertexShaderInput input,uint32_t instanceId: SV_InstanceID) {
    VertexShaderOutput output;
    output.position = mul(input.position, gParticle[instanceId].wVP);
    output.texCoord = input.texCoord;
    output.color = gParticle[instanceId].color;
    return output;
}