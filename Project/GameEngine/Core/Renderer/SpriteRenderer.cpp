#include "pch.h"
#include "SpriteRenderer.h"
#include "Graphics/GraphicsDevice.h"
#include "Sprite/Sprite.h"
#include "Graphics/Material.h"
#include "Graphics/Mesh.h"
#include "PSOManager.h"
#include "ShaderManager.h"
#include "LightManager.h"
#include "Graphics/LightDataBuffer.h"
#include "Camera/Camera.h"
#include "Utility/Logger.h"
#include "RootBindingSlots.h"

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
	setPipelineFunc("Sprite", spriteData.blendMode);

	Sprite* sprite = spriteData.sprite;
	if (!sprite) {
		Logger::GetInstance().Log("Sprite is null in DrawSprite", Logger::LogLevel::Error);
		return;
	}

	if (sprite->GetMaterials().size() == 0) {
		sprite->SetMaterial(defaultMaterial);
	}

	// スプライトのマテリアルのライティングモードをNONEに設定
	Material* spriteMaterial = sprite->GetMaterial();
	if (spriteMaterial) {
		spriteMaterial->SetLightingMode(Material::LightingMode::NONE);
	}

	auto* cmdList = device_->GetCommandList();
	Camera* camera = spriteData.camera;

	if (!camera) {
		Logger::GetInstance().Log("Camera is null in DrawSprite", Logger::LogLevel::Error);
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
	cmdList->IASetVertexBuffers(0, 1, &sprite->GetMesh()->GetVertexBufferView());
	cmdList->IASetIndexBuffer(&sprite->GetMesh()->GetIndexBufferView());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	// Object3Dルートシグネチャに合わせてルートパラメータを設定
	// Root Parameter 0: Material (Pixel Shader)
    cmdList->SetGraphicsRootConstantBufferView(materialSlot, sprite->GetMaterial()->GetMaterialResource()->GetGPUVirtualAddress());
	
	// Root Parameter 1: TransformationMatrix (Vertex Shader)
    cmdList->SetGraphicsRootConstantBufferView(transformSlot, sprite->GetTransformationMatrix()->GetTransformationMatrixResource()->GetGPUVirtualAddress());
	
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
	cmdList->DrawIndexedInstanced(sprite->GetMesh()->GetIndexCount(), 1, 0, 0, 0);
}

} // namespace GameEngine
