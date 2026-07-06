#include "pch.h"
#include "SpriteRenderer.h"
#include "Graphics/GraphicsDevice.h"
#include "Sprite/Sprite.h"
#include "Graphics/Material.h"
#include "Graphics/Mesh.h"
#include "Graphics/TransformationMatrix.h"
#include "PSOManager.h"
#include "ShaderManager.h"
#include "LightManager.h"
#include "LightDataBuffer.h"
#include "Camera/Camera.h"
#include "Utility/Logger.h"
#include "RootBindingSlots.h"
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
	setPipelineFunc("Sprite", resolvedBlendMode);

	Sprite* sprite = spriteData.sprite;
	if (!sprite) {
		Logger::Error("Sprite is null in DrawSprite");
		return;
	}

	Mesh* mesh = sprite->GetMesh();
	if (!mesh) {
		Logger::Error("PrimitiveMeshComponent mesh is missing in DrawSprite");
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

	// スプライトのマテリアルのライティングモードをNONEに設定
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

	auto resolveSlot = [this](const char* semantic, UINT fallback) -> UINT {
		if (!psoManager_) {
			return fallback;
		}

		auto* shaderManager = psoManager_->GetShaderManager();
		if (!shaderManager) {
			return fallback;
		}

		auto resolved = shaderManager->ResolveObject3DRootParameter(semantic);
		return resolved.value_or(fallback);
	};

	const UINT materialSlot = resolveSlot("material", RootBindingSlots::Object3D::kMaterial);
	const UINT transformSlot = resolveSlot("transform", RootBindingSlots::Object3D::kTransform);
	const UINT cameraSlot = resolveSlot("camera", RootBindingSlots::Object3D::kCamera);
	const UINT lightCountSlot = resolveSlot("lightcount", RootBindingSlots::Object3D::kLightCount);
	const UINT directionalLightSlot = resolveSlot("directionallights", RootBindingSlots::Object3D::kDirectionalLight);
	const UINT pointLightSlot = resolveSlot("pointlights", RootBindingSlots::Object3D::kPointLight);
	const UINT spotLightSlot = resolveSlot("spotlights", RootBindingSlots::Object3D::kSpotLight);
	const UINT areaLightSlot = resolveSlot("arealights", RootBindingSlots::Object3D::kAreaLight);
	const UINT textureSlot = resolveSlot("texture", RootBindingSlots::Object3D::kTexture);

	// 頂点バッファとインデックスバッファを設定
	cmdList->IASetVertexBuffers(0, 1, &mesh->GetVertexBufferView());
	cmdList->IASetIndexBuffer(&mesh->GetIndexBufferView());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	// Object3Dルートシグネチャに合わせてルートパラメータを設定
	// Root Parameter 0: Material (Pixel Shader)
 if (!spriteMaterial) {
	 Logger::Warning("[SpriteRenderer] Material is null, skip draw");
	 return;
 }
 cmdList->SetGraphicsRootConstantBufferView(materialSlot, spriteMaterial->GetMaterialResource()->GetGPUVirtualAddress());
	
	// Root Parameter 1: TransformationMatrix (Vertex Shader)
    cmdList->SetGraphicsRootConstantBufferView(transformSlot, transformationMatrix->GetTransformationMatrixResource()->GetGPUVirtualAddress());
	
	// Root Parameter 2: Camera (Pixel Shader)
 cmdList->SetGraphicsRootConstantBufferView(cameraSlot, camera->GetCameraResource()->GetGPUVirtualAddress());
	
	// Root Parameter 3: LightCount (Pixel Shader)
    cmdList->SetGraphicsRootConstantBufferView(lightCountSlot, lightBuffer->GetLightCountResource()->GetGPUVirtualAddress());
	
	// Root Parameter 4: DirectionalLights StructuredBuffer (t0)
  cmdList->SetGraphicsRootDescriptorTable(directionalLightSlot, lightBuffer->GetDirectionalLightSRV());
	
	// Root Parameter 5: PointLights StructuredBuffer (t1)
    cmdList->SetGraphicsRootDescriptorTable(pointLightSlot, lightBuffer->GetPointLightSRV());
	
	// Root Parameter 6: SpotLights StructuredBuffer (t2)
 cmdList->SetGraphicsRootDescriptorTable(spotLightSlot, lightBuffer->GetSpotLightSRV());
	
	// Root Parameter 7: AreaLights StructuredBuffer (t3)
 cmdList->SetGraphicsRootDescriptorTable(areaLightSlot, lightBuffer->GetAreaLightSRV());
	
	// Root Parameter 8: Texture (t4)
    cmdList->SetGraphicsRootDescriptorTable(textureSlot, spriteData.textureSrvHandle);

	// 描画
	cmdList->DrawIndexedInstanced(mesh->GetIndexCount(), 1, 0, 0, 0);
}

} // namespace GameEngine
