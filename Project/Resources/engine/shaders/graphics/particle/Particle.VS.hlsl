#include "Particle.hlsli"

// CPU側ParticleSystem::ParticleForGPUおよびParticleRender CSの出力と共有する224バイト構造。
// StructuredBufferのstrideはsizeof(ParticleForGPU)で作られるため、順序や型を独立に変更してはならない。
struct ParticleForGPU {
    float4x4 wVP;
    float4x4 world;
    float4x4 uvTransform;
    float4 color;
    float4 customData;
};

// t0は粒子インスタンス配列で、SV_InstanceIDが描画対象要素を直接選択する。
StructuredBuffer<ParticleForGPU> gParticle : register(t0);

// 通常粒子はMesh::VertexData、リボンはParticleRibbon CSが生成する同一36バイト形式を入力する。
// normalは通常メッシュでは法線だが、リボン頂点に限りx成分を頂点アルファとして再利用する。
struct VertexShaderInput {
    float4 position : POSITION0;
    float2 texCoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID) {
    VertexShaderOutput output;
    output.position = mul(input.position, gParticle[instanceId].wVP);
    output.texCoord = input.texCoord;
    output.color = gParticle[instanceId].color;
    // 通常粒子のcustomData.xは0以上の寿命進行度であり、リボン描画だけが-1を設定する。
    // その場合はRibbon CSがnormal.xへ格納した端点アルファを色へ反映し、区間内を補間させる。
    if (gParticle[instanceId].customData.x < 0.0f)
    {
        output.color.a *= saturate(input.normal.x);
    }
    // HLSLステージ間で行列そのものを一つのセマンティクスへ載せず、4行を明示的に受け渡す。
    output.uvTransform0 = gParticle[instanceId].uvTransform[0];
    output.uvTransform1 = gParticle[instanceId].uvTransform[1];
    output.uvTransform2 = gParticle[instanceId].uvTransform[2];
    output.uvTransform3 = gParticle[instanceId].uvTransform[3];
    output.customData = gParticle[instanceId].customData;
    return output;
}
