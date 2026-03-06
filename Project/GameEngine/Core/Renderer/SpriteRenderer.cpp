#include "pch.h"
#include "SpriteRenderer.h"
#include "Graphics/GraphicsDevice.h"
#include "Sprite/Sprite.h"
#include "Graphics/Material.h"
#include "Graphics/Mesh.h"
#include "PSOManager.h"
#include "LightManager.h"
#include "Graphics/LightDataBuffer.h"
#include "Camera/Camera.h"
#include "Utility/Logger.h"

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

	// 頂点バッファとインデックスバッファを設定
	cmdList->IASetVertexBuffers(0, 1, &sprite->GetMesh()->GetVertexBufferView());
	cmdList->IASetIndexBuffer(&sprite->GetMesh()->GetIndexBufferView());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	// Object3Dルートシグネチャに合わせてルートパラメータを設定
	// Root Parameter 0: Material (Pixel Shader)
	cmdList->SetGraphicsRootConstantBufferView(0, sprite->GetMaterial()->GetMaterialResource()->GetGPUVirtualAddress());
	
	// Root Parameter 1: TransformationMatrix (Vertex Shader)
	cmdList->SetGraphicsRootConstantBufferView(1, sprite->GetTransformationMatrix()->GetTransformationMatrixResource()->GetGPUVirtualAddress());
	
	// Root Parameter 2: Camera (Pixel Shader)
	cmdList->SetGraphicsRootConstantBufferView(2, camera->GetCameraResource()->GetGPUVirtualAddress());
	
	// Root Parameter 3: LightCount (Pixel Shader)
	cmdList->SetGraphicsRootConstantBufferView(3, lightBuffer->GetLightCountResource()->GetGPUVirtualAddress());
	
	// Root Parameter 4: DirectionalLights StructuredBuffer (t0)
	cmdList->SetGraphicsRootDescriptorTable(4, lightBuffer->GetDirectionalLightSRV());
	
	// Root Parameter 5: PointLights StructuredBuffer (t1)
	cmdList->SetGraphicsRootDescriptorTable(5, lightBuffer->GetPointLightSRV());
	
	// Root Parameter 6: SpotLights StructuredBuffer (t2)
	cmdList->SetGraphicsRootDescriptorTable(6, lightBuffer->GetSpotLightSRV());
	
	// Root Parameter 7: AreaLights StructuredBuffer (t3)
	cmdList->SetGraphicsRootDescriptorTable(7, lightBuffer->GetAreaLightSRV());
	
	// Root Parameter 8: Texture (t4)
	cmdList->SetGraphicsRootDescriptorTable(8, spriteData.textureSrvHandle);

	// 描画
	cmdList->DrawIndexedInstanced(sprite->GetMesh()->GetIndexCount(), 1, 0, 0, 0);
}

} // namespace GameEngine
