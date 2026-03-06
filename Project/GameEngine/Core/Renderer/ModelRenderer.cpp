#include "pch.h"
#include "ModelRenderer.h"
#include "Graphics/GraphicsDevice.h"
#include "Model/Model.h"
#include "Graphics/Material.h"
#include "PSOManager.h"
#include "LightManager.h"
#include "Graphics/DirectionalLight.h"
#include "Graphics/PointLight.h"
#include "Graphics/SpotLight.h"
#include "Graphics/AreaLight.h"
#include "Graphics/LightDataBuffer.h"
#include "Model/ModelAsset.h"

namespace GameEngine {

void ModelRenderer::Initialize(GraphicsDevice* device, PSOManager* psoManager) {
	device_ = device;
	psoManager_ = psoManager;
}

void ModelRenderer::DrawModel(const ModelDrawData& modelData,
	Material* defaultMaterial,
	LightManager* lightManager,
	std::function<void(const std::string&, BlendMode)> setPipelineFunc) {

	// Object3Dパイプラインを設定
	setPipelineFunc("Object3D", modelData.blendMode);

	Model* model = modelData.model;
	if (model->GetMaterials().size() == 0) {
		model->SetMaterial(defaultMaterial);
	}

	auto* cmdList = device_->GetCommandList();
	ModelAsset* asset = model->GetModelAsset();
	const auto& meshes = asset->GetMeshData();
	const auto& materials = model->GetMaterials();

	assert(!materials.empty());
	assert(!modelData.textures.empty());

	Camera* camera = modelData.camera;

	// LightDataBufferを取得
	LightDataBuffer* lightBuffer = lightManager->GetLightDataBuffer();

	// 共通バインディング（全メッシュで共通）
	// Root Parameter 1: TransformationMatrix (Vertex Shader)
	cmdList->SetGraphicsRootConstantBufferView(1, model->GetTransformationMatrix()->GetTransformationMatrixResource()->GetGPUVirtualAddress());
	
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

	// 各メッシュごとの描画
	for (size_t i = 0; i < meshes.size(); ++i) {
		// --- マテリアル取得（不足分は先頭を使い回し） ---
		const Material* mat = (i < materials.size()) ? materials[i] : materials[0];
		assert(mat != nullptr);

		// --- テクスチャSRV取得（不足分は先頭を使い回し） ---
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = (i < modelData.textures.size()) ? modelData.textures[i] : modelData.textures[0];
		assert(srvHandle.ptr != 0);

		// --- メッシュ固有のバインディング ---
		// Root Parameter 0: Material (Pixel Shader)
		cmdList->SetGraphicsRootConstantBufferView(0, mat->GetMaterialResource()->GetGPUVirtualAddress());
		
		// Root Parameter 8: Texture (t4)
		cmdList->SetGraphicsRootDescriptorTable(8, srvHandle);
		
		// 頂点バッファとプリミティブトポロジを設定
		cmdList->IASetVertexBuffers(0, 1, &asset->GetVertexBufferView(i));
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		
		// 描画
		cmdList->DrawInstanced(static_cast<UINT>(meshes[i].vertices.size()), 1, 0, 0);
	}
}

} // namespace GameEngine
