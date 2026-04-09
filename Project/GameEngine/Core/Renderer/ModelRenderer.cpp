#include "pch.h"
#include "ModelRenderer.h"
#include "Graphics/GraphicsDevice.h"
#include "Model/Model.h"
#include "Graphics/Material.h"
#include "PSOManager.h"
#include "ShaderManager.h"
#include "LightManager.h"
#include "RootBindingSlots.h"
#include "Graphics/DirectionalLight.h"
#include "Graphics/PointLight.h"
#include "Graphics/SpotLight.h"
#include "Graphics/AreaLight.h"
#include "Graphics/LightDataBuffer.h"
#include "Model/ModelAsset.h"
#include "Component/AnimationComponent.h"
#include <array>
#include <string_view>

namespace GameEngine {

void ModelRenderer::Initialize(GraphicsDevice* device, PSOManager* psoManager, AssetManager* assetManager) {
   device_ = device;
   psoManager_ = psoManager;
   assetManager_ = assetManager;
}

void ModelRenderer::DrawModel(const ModelDrawData& modelData,
   Material* defaultMaterial,
   LightManager* lightManager,
   std::function<void(const std::string&, BlendMode)> setPipelineFunc) {
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

   bool skinningEnabled = true;
   if (const auto* animationComponent = model->GetComponent<AnimationComponent>()) {
	  skinningEnabled = animationComponent->useSkinning;
   }

   SkinCluster* skinCluster = model->GetSkinCluster();
   const bool canUseSkinning = skinningEnabled && skinCluster;

   std::string skinningPipelineName;
   bool hasSkinningPipeline = false;
   if (psoManager_) {
	  static constexpr std::array<std::string_view, 2> kSkinningPipelineCandidates = {
		 "SkinningObject3D",
		 "SkinningObject3d"
	  };

	  for (std::string_view candidate : kSkinningPipelineCandidates) {
		 const std::string name(candidate);
		 if (psoManager_->GetPipeline(name, modelData.blendMode) ||
			psoManager_->GetPipeline(name, BlendMode::kBlendModeNone)) {
			hasSkinningPipeline = true;
			skinningPipelineName = name;
			break;
		 }
	  }
   }

   const bool useSkinning = canUseSkinning && hasSkinningPipeline;
   const std::string pipelineName = useSkinning ? skinningPipelineName : "Object3D";

   // 使用パイプラインを設定
   setPipelineFunc(pipelineName, modelData.blendMode);

   auto resolvePipelineSlot = [this, &pipelineName](const char* semantic, UINT fallback) -> UINT {
	  if (!psoManager_) {
		 return fallback;
	  }

	  auto* shaderManager = psoManager_->GetShaderManager();
	  if (!shaderManager) {
		 return fallback;
	  }

	  auto resolved = shaderManager->ResolvePipelineRootParameter(pipelineName, semantic);
	  return resolved.value_or(fallback);
   };

   const UINT materialSlot = resolvePipelineSlot("material", RootBindingSlots::Object3D::kMaterial);
   const UINT transformSlot = resolvePipelineSlot("transform", RootBindingSlots::Object3D::kTransform);
   const UINT cameraSlot = resolvePipelineSlot("camera", RootBindingSlots::Object3D::kCamera);
   const UINT lightCountSlot = resolvePipelineSlot("lightcount", RootBindingSlots::Object3D::kLightCount);
   const UINT directionalLightSlot = resolvePipelineSlot("directionallights", RootBindingSlots::Object3D::kDirectionalLight);
   const UINT pointLightSlot = resolvePipelineSlot("pointlights", RootBindingSlots::Object3D::kPointLight);
   const UINT spotLightSlot = resolvePipelineSlot("spotlights", RootBindingSlots::Object3D::kSpotLight);
   const UINT areaLightSlot = resolvePipelineSlot("arealights", RootBindingSlots::Object3D::kAreaLight);
   const UINT textureSlot = resolvePipelineSlot("texture", RootBindingSlots::Object3D::kTexture);
   const UINT skinPaletteSlot = resolvePipelineSlot("skinpalette", RootBindingSlots::Object3D::kSkinPalette);

   // 共通バインディング（全メッシュで共通）
   // Root Parameter 1: TransformationMatrix (Vertex Shader)
   cmdList->SetGraphicsRootConstantBufferView(transformSlot, model->GetTransformationMatrix()->GetTransformationMatrixResource()->GetGPUVirtualAddress());

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

   if (useSkinning && skinCluster->paletteSrvHandle.second.ptr != 0) {
	  cmdList->SetGraphicsRootDescriptorTable(skinPaletteSlot, skinCluster->paletteSrvHandle.second);
   }

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
	  cmdList->SetGraphicsRootConstantBufferView(materialSlot, mat->GetMaterialResource()->GetGPUVirtualAddress());

	  // Root Parameter 8: Texture (t4)
	  cmdList->SetGraphicsRootDescriptorTable(textureSlot, srvHandle);

     // 頂点バッファとプリミティブトポロジを設定
      if (useSkinning) {
		 const D3D12_VERTEX_BUFFER_VIEW* influenceBufferView = skinCluster->GetInfluenceBufferView(i);
		 if (!influenceBufferView) {
			 cmdList->IASetVertexBuffers(0, 1, &asset->GetVertexBufferView(i));
			 cmdList->IASetIndexBuffer(&asset->GetIndexBufferView(i));
			 cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			 cmdList->DrawIndexedInstanced(static_cast<UINT>(meshes[i].indices.size()), 1, 0, 0, 0);
			 continue;
		 }

		 D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[2] = {
			asset->GetVertexBufferView(i),
            *influenceBufferView
		 };
		 cmdList->IASetVertexBuffers(0, 2, vertexBufferViews);
	  } else {
		 cmdList->IASetVertexBuffers(0, 1, &asset->GetVertexBufferView(i));
	  }
	  cmdList->IASetIndexBuffer(&asset->GetIndexBufferView(i));
	  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	  // 描画
	  cmdList->DrawIndexedInstanced(static_cast<UINT>(meshes[i].indices.size()), 1, 0, 0, 0);
   }
}

} // namespace GameEngine
