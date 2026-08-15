#include "pch.h"
#include "SpriteRenderer.h"
#include "Graphics/GraphicsDevice.h"
#include "Sprite/Sprite.h"
#include "Graphics/Material.h"
#include "Graphics/Mesh.h"
#include "Graphics/TransformationMatrix.h"
#include "PSOManager.h"
#include "LightManager.h"
#include "LightDataBuffer.h"
#include "Camera/Camera.h"
#include "Utility/Logger.h"
#include "Component/MaterialComponent.h"

namespace GameEngine {

void SpriteRenderer::Initialize(GraphicsDevice* device, PSOManager* psoManager) {
	device_ = device;
	psoManager_ = psoManager;
}

void SpriteRenderer::DrawSprite(const SpriteDrawData& spriteData,
	Material* defaultMaterial,
	LightManager* lightManager,
	std::function<void(const std::string&, BlendMode)> setPipelineFunc) {

	// パイプラインを設定（必ず先に呼び出す）
	// マテリアルに blendMode が設定されていればそれを優先、なければ DrawCommand の blendMode を使用
	// ※ この時点ではまだマテリアルを取得していないので DrawCommand の blendMode で先行設定し、
	//   マテリアル取得後に必要であれば再設定する
	BlendMode resolvedBlendMode = spriteData.blendMode;
	// 先行設定で通常ケースを処理し、後から見つかったマテリアル固有値が異なる時だけ
	// PSOキャッシュ経由で再設定する。
	setPipelineFunc("Sprite", resolvedBlendMode);

	Sprite* sprite = spriteData.sprite;
	if (!sprite) {
		Logger::Error("Sprite is null in DrawSprite");
		return;
	}

	Mesh* mesh = sprite->GetMesh();
	if (!mesh) {
		Logger::Error("MeshComponent mesh is missing in DrawSprite");
		return;
	}

	TransformationMatrix* transformationMatrix = sprite->GetTransformationMatrix();
	if (!transformationMatrix) {
		Logger::Error("TransformationMatrix is missing in DrawSprite");
		return;
	}

	auto* materialComponent = sprite->GetComponent<MaterialComponent>();
	if (!materialComponent) {
		Logger::Error("MaterialComponent is missing in DrawSprite");
		return;
	}

   if (materialComponent->materials.empty()) {
		materialComponent->AssignMaterial(defaultMaterial);
	}

	// ワールド空間スプライトも面の色をそのまま表示するため、3Dライト計算は無効化する。
   Material* spriteMaterial = materialComponent->materials.empty() ? nullptr : materialComponent->materials[0];
	if (spriteMaterial) {
		spriteMaterial->SetLightingMode(Material::LightingMode::NONE);
		// マテリアルに blendMode が設定されていれば再設定
		if (auto matBlend = spriteMaterial->GetBlendMode()) {
			resolvedBlendMode = *matBlend;
			setPipelineFunc("Sprite", resolvedBlendMode);
		}
	}

	auto* cmdList = device_->GetCommandList();
	Camera* camera = spriteData.camera;

	if (!camera) {
		Logger::Error("Camera is null in DrawSprite");
		return;
	}

	// LightDataBufferを取得
	LightDataBuffer* lightBuffer = lightManager->GetLightDataBuffer();

	auto resolveSlot = [this](const char* semantic) -> std::optional<UINT> {
		if (!psoManager_) {
			Logger::Error("[SpriteRenderer] PSOManager is null while resolving root slot: " + std::string(semantic));
			return std::nullopt;
		}

		auto resolved = psoManager_->ResolvePipelineRootParameter("Sprite", semantic);
		if (!resolved.has_value()) {
			Logger::Error("[SpriteRenderer] Failed to resolve root slot: pipeline=Sprite, semantic=" + std::string(semantic));
		}
		return resolved;
	};

	const auto materialSlot = resolveSlot("material");
	const auto transformSlot = resolveSlot("transform");
	const auto cameraSlot = resolveSlot("camera");
	const auto lightCountSlot = resolveSlot("lightcount");
	const auto directionalLightSlot = resolveSlot("directionallights");
	const auto pointLightSlot = resolveSlot("pointlights");
	const auto spotLightSlot = resolveSlot("spotlights");
	const auto areaLightSlot = resolveSlot("arealights");
	const auto textureSlot = resolveSlot("texture");
	if (!materialSlot || !transformSlot || !cameraSlot || !lightCountSlot ||
		!directionalLightSlot || !pointLightSlot || !spotLightSlot ||
		!areaLightSlot || !textureSlot) {
		return;
	}

	// 頂点バッファとインデックスバッファを設定
	cmdList->IASetVertexBuffers(0, 1, &mesh->GetVertexBufferView());
	cmdList->IASetIndexBuffer(&mesh->GetIndexBufferView());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	// 変換・カメラ・ライト・テクスチャを意味名で解決済みのスロットへまとめて束縛する。
	// Sprite PSOがObject3Dと似た資源構成でも、JSON上の並び順には依存しない。
	// Object3Dルートシグネチャに合わせてルートパラメータを設定
	// Root Parameter 0: Material (Pixel Shader)
 if (!spriteMaterial) {
	 Logger::Warning("[SpriteRenderer] Material is null, skip draw");
	 return;
 }
 cmdList->SetGraphicsRootConstantBufferView(materialSlot.value(), spriteMaterial->GetMaterialResource()->GetGPUVirtualAddress());
	
	// Root Parameter 1: TransformationMatrix (Vertex Shader)
    cmdList->SetGraphicsRootConstantBufferView(transformSlot.value(), transformationMatrix->GetTransformationMatrixResource()->GetGPUVirtualAddress());
	
	// Root Parameter 2: Camera (Pixel Shader)
 cmdList->SetGraphicsRootConstantBufferView(cameraSlot.value(), camera->GetCameraResource()->GetGPUVirtualAddress());
	
	// Root Parameter 3: LightCount (Pixel Shader)
    cmdList->SetGraphicsRootConstantBufferView(lightCountSlot.value(), lightBuffer->GetLightCountResource()->GetGPUVirtualAddress());
	
	// Root Parameter 4: DirectionalLights StructuredBuffer (t0)
  cmdList->SetGraphicsRootDescriptorTable(directionalLightSlot.value(), lightBuffer->GetDirectionalLightSRV());
	
	// Root Parameter 5: PointLights StructuredBuffer (t1)
    cmdList->SetGraphicsRootDescriptorTable(pointLightSlot.value(), lightBuffer->GetPointLightSRV());
	
	// Root Parameter 6: SpotLights StructuredBuffer (t2)
 cmdList->SetGraphicsRootDescriptorTable(spotLightSlot.value(), lightBuffer->GetSpotLightSRV());
	
	// Root Parameter 7: AreaLights StructuredBuffer (t3)
 cmdList->SetGraphicsRootDescriptorTable(areaLightSlot.value(), lightBuffer->GetAreaLightSRV());
	
	// Root Parameter 8: Texture (t4)
    cmdList->SetGraphicsRootDescriptorTable(textureSlot.value(), spriteData.textureSrvHandle);

	// 描画
	cmdList->DrawIndexedInstanced(mesh->GetIndexCount(), 1, 0, 0, 0);
}

} // namespace GameEngine
