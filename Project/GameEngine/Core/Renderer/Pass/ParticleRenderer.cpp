#include "pch.h"
#include "ParticleRenderer.h"
#include "Graphics/GraphicsDevice.h"
#include "PSOManager.h"
#include "Effect/ParticleSystem.h"
#include "Graphics/Texture.h"
#include "Graphics/Mesh.h"
#include "Model/ModelAsset.h"

namespace GameEngine {

void ParticleRenderer::Initialize(GraphicsDevice* device, PSOManager* psoManager) {
	device_ = device;
	psoManager_ = psoManager;
}

void ParticleRenderer::DrawParticle(const ParticleDrawData& particleData,
	std::function<void(const std::string&, BlendMode)> setPipelineFunc,
	std::function<void()> invalidatePipelineBindingFunc,
	D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle,
	D3D12_GPU_DESCRIPTOR_HANDLE sceneDepthHandle) {
	ParticleSystem* particleSystem = particleData.particleSystem;
	if (!particleSystem) {
		Logger::Warning("[ParticleRenderer] ParticleSystem is null, skip draw");
		return;
	}

	// リボンも初回のGPUシミュレーション結果から履歴を作るため、描画数判定より先にComputeを実行する。
	particleSystem->DispatchGpuSimulation(psoManager_);
	// Compute PSOを設定するとRendererのグラフィックスPSOキャッシュと実コマンドリストが乖離する。
	// 後続のSetPipelineが必ずグラフィックスPSOを再設定できるよう、外部変更を明示的に通知する。
	if (invalidatePipelineBindingFunc) {
		invalidatePipelineBindingFunc();
	}

	// 親粒子の死亡後は本体数が0でも、切り離されたトレイルだけを描画する。
	const uint32_t activeCount = particleSystem->GetDrawParticleCount();
	const bool hasRibbon = particleSystem->GetTrailModule() &&
		particleSystem->GetTrailModule()->IsEnabled() &&
		particleSystem->GetRibbonIndexCount() > 0;
	if (activeCount == 0 && !hasRibbon) return;

	// Particleパイプラインを設定
	// マテリアルに blendMode が設定されていればそれを優先、なければ加算ブレンド
	auto* earlyMaterial = particleSystem->GetMaterial();
	// 背景屈折は通常の加算では背景色を保持できないため、アルファ合成PSOへ強制する。
	// 非屈折粒子だけがマテリアル指定または既定の加算ブレンドを使う。
	const bool usesSceneRefraction = earlyMaterial && std::fabs(earlyMaterial->GetDistortionStrength()) > 0.0001f;
	const BlendMode resolvedBlendMode = usesSceneRefraction
		? BlendMode::kBlendModeNormal
		: (earlyMaterial && earlyMaterial->GetBlendMode().has_value())
			? earlyMaterial->GetBlendMode().value()
			: BlendMode::kBlendModeAdd;
	setPipelineFunc("Particle", resolvedBlendMode);

	auto* cmdList = device_->GetCommandList();

	auto resolveSlot = [this](const char* semantic) -> std::optional<UINT> {
		if (!psoManager_) {
			Logger::Error("[ParticleRenderer] PSOManager is null while resolving root slot: " + std::string(semantic));
			return std::nullopt;
		}

		auto resolved = psoManager_->ResolvePipelineRootParameter("Particle", semantic);
		if (!resolved.has_value()) {
			Logger::Error("[ParticleRenderer] Failed to resolve root slot: pipeline=Particle, semantic=" + std::string(semantic));
		}
		return resolved;
	};

	const auto materialSlot = resolveSlot("material");
	const auto instancingSlot = resolveSlot("instancing");
	const auto textureSlot = resolveSlot("texture");
	const auto sceneColorSlot = resolveSlot("scenecolor");
	const auto sceneDepthSlot = resolveSlot("scenedepth");
	if (!materialSlot || !instancingSlot || !textureSlot || !sceneColorSlot || !sceneDepthSlot) {
		return;
	}

	// マテリアル設定（ParticleMaterial）
	auto* material = particleSystem->GetMaterial();
	if (material && material->GetMaterialResource()) {
     cmdList->SetGraphicsRootConstantBufferView(materialSlot.value(), material->GetMaterialResource()->GetGPUVirtualAddress());
	}

	if (sceneColorHandle.ptr != 0) {
		// 透明パス開始時に固定した色・深度を参照し、現在書き込み中のRTVとの自己依存を避ける。
		cmdList->SetGraphicsRootDescriptorTable(sceneColorSlot.value(), sceneColorHandle);
	}
	if (sceneDepthHandle.ptr != 0) {
		cmdList->SetGraphicsRootDescriptorTable(sceneDepthSlot.value(), sceneDepthHandle);
	}

	// トレイルは本体を置き換えず、先に独立した履歴メッシュとして描画する。
	if (hasRibbon) {
		if (Texture* ribbonTexture = particleSystem->GetRibbonTexture()) {
			cmdList->SetGraphicsRootDescriptorTable(textureSlot.value(), ribbonTexture->GetTextureSrvHandleGPU());
		}
		cmdList->SetGraphicsRootDescriptorTable(
			instancingSlot.value(), particleSystem->GetRibbonInstancingSrvHandleGPU());
		const auto& vertexBufferView = particleSystem->GetRibbonVertexBufferView();
		const auto& indexBufferView = particleSystem->GetRibbonIndexBufferView();
		cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);
		cmdList->IASetIndexBuffer(&indexBufferView);
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->DrawIndexedInstanced(particleSystem->GetRibbonIndexCount(), 1, 0, 0, 0);
	}

	if (activeCount == 0) return;

	// 本体は常にGPUシミュレーション出力を使い、トレイルの有無とは独立して描画する。
	if (Texture* texture = particleSystem->GetTexture()) {
		cmdList->SetGraphicsRootDescriptorTable(textureSlot.value(), texture->GetTextureSrvHandleGPU());
	}
	cmdList->SetGraphicsRootDescriptorTable(instancingSlot.value(), particleSystem->GetInstancingSrvHandleGPU());

	// メッシュ設定（Billboard用Quad または Model）
	ModelAsset* modelAsset = particleSystem->GetModelAsset();
	if (modelAsset) {
		// Modelモードは各頂点をactiveCount回インスタンス化し、粒子データはSRVから引く。
		const auto& meshes = modelAsset->GetMeshData();
		for (size_t i = 0; i < meshes.size(); ++i) {
			cmdList->IASetVertexBuffers(0, 1, &modelAsset->GetVertexBufferView(i));
			cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmdList->DrawInstanced(static_cast<UINT>(meshes[i].vertices.size()), activeCount, 0, 0);
		}
	} else {
		// Billboardモードは共有Quadのインデックスを使い、頂点数を抑える。
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
