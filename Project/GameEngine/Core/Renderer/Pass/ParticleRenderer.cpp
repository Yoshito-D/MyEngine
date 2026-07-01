#include "pch.h"
#include "ParticleRenderer.h"
#include "Graphics/GraphicsDevice.h"
#include "PSOManager.h"
#include "Effect/ParticleSystem.h"
#include "Graphics/Texture.h"
#include "Graphics/Mesh.h"
#include "Model/ModelAsset.h"
#include "ShaderManager.h"
#include "RootBindingSlots.h"

namespace GameEngine {

void ParticleRenderer::Initialize(GraphicsDevice* device, PSOManager* psoManager) {
	device_ = device;
	psoManager_ = psoManager;
}

void ParticleRenderer::DrawParticle(const ParticleDrawData& particleData,
	std::function<void(const std::string&, BlendMode)> setPipelineFunc) {
	ParticleSystem* particleSystem = particleData.particleSystem;
	if (!particleSystem) {
		Logger::Warning("[ParticleRenderer] ParticleSystem is null, skip draw");
		return;
	}

	// アクティブなパーティクルがない場合は描画しない
	uint32_t activeCount = particleSystem->GetActiveParticleCount();
	if (activeCount == 0) return;

	// Particleパイプラインを設定
	// マテリアルに blendMode が設定されていればそれを優先、なければ加算ブレンド
	auto* earlyMaterial = particleSystem->GetMaterial();
	const BlendMode resolvedBlendMode = (earlyMaterial && earlyMaterial->GetBlendMode().has_value())
		? earlyMaterial->GetBlendMode().value()
		: BlendMode::kBlendModeAdd;
	setPipelineFunc("Particle", resolvedBlendMode);

	auto* cmdList = device_->GetCommandList();

	auto resolveSlot = [this](const char* semantic, UINT fallback) -> UINT {
		if (!psoManager_) {
			return fallback;
		}

		auto* shaderManager = psoManager_->GetShaderManager();
		if (!shaderManager) {
			return fallback;
		}

		auto resolved = shaderManager->ResolvePipelineRootParameter("Particle", semantic);
		return resolved.value_or(fallback);
	};

	const UINT materialSlot = resolveSlot("material", RootBindingSlots::Particle::kMaterial);
	const UINT instancingSlot = resolveSlot("instancing", RootBindingSlots::Particle::kInstancing);
	const UINT textureSlot = resolveSlot("texture", RootBindingSlots::Particle::kTexture);

	// マテリアル設定（ParticleMaterial）
	auto* material = particleSystem->GetMaterial();
	if (material && material->GetMaterialResource()) {
     cmdList->SetGraphicsRootConstantBufferView(materialSlot, material->GetMaterialResource()->GetGPUVirtualAddress());
	}

	// インスタンシング用SRV（パーティクルデータ配列） 
    cmdList->SetGraphicsRootDescriptorTable(instancingSlot, particleSystem->GetInstancingSrvHandleGPU());

	// テクスチャSRV
	Texture* texture = particleSystem->GetTexture();
	if (texture) {
      cmdList->SetGraphicsRootDescriptorTable(textureSlot, texture->GetTextureSrvHandleGPU());
	}

	// メッシュ設定（Billboard用Quad または Model）
	ModelAsset* modelAsset = particleSystem->GetModelAsset();
	if (modelAsset) {
		// Model モード
		const auto& meshes = modelAsset->GetMeshData();
		for (size_t i = 0; i < meshes.size(); ++i) {
			cmdList->IASetVertexBuffers(0, 1, &modelAsset->GetVertexBufferView(i));
			cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmdList->DrawInstanced(static_cast<UINT>(meshes[i].vertices.size()), activeCount, 0, 0);
		}
	} else {
		// Billboard mode (default)
		Mesh* mesh = particleSystem->GetMesh();
		if (mesh) {
			cmdList->IASetVertexBuffers(0, 1, &mesh->GetVertexBufferView());
			cmdList->IASetIndexBuffer(&mesh->GetIndexBufferView());
			cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmdList->DrawIndexedInstanced(mesh->GetIndexCount(), activeCount, 0, 0, 0);
		}
	}
}

} // namespace GameEngine
