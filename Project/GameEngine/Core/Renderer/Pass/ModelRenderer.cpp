#include "pch.h"
#include "ModelRenderer.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/TransformationMatrix.h"
#include "Model/Model.h"
#include "Graphics/Material.h"
#include "PSOManager.h"
#include "LightManager.h"
#include "RootBindingSlots.h"
#include "DirectionalLight.h"
#include "PointLight.h"
#include "SpotLight.h"
#include "AreaLight.h"
#include "LightDataBuffer.h"
#include "Model/ModelAsset.h"
#include "Component/AnimationComponent.h"
#include "Component/MaterialComponent.h"
#include "Component/ModelAssetComponent.h"

namespace GameEngine {
namespace {
constexpr const char* kSkinningComputePipelineName = "SkinningCompute";
constexpr UINT kSkinningThreadGroupSize = 1024;

void TransitionResource(
   ID3D12GraphicsCommandList* cmdList,
   ID3D12Resource* resource,
   D3D12_RESOURCE_STATES before,
   D3D12_RESOURCE_STATES after) {
   if (!resource || before == after) {
	  return;
   }

   CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, before, after);
   cmdList->ResourceBarrier(1, &barrier);
}
}

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
   auto* materialComponent = model->GetComponent<MaterialComponent>();
   if (!materialComponent) {
	  return;
   }

   std::vector<Material*> fallbackMaterials;
   const std::vector<Material*>* effectiveMaterials = &materialComponent->materials;
   if (effectiveMaterials->empty()) {
	  if (!defaultMaterial) {
		 return;
	  }
	  fallbackMaterials.push_back(defaultMaterial);
	  effectiveMaterials = &fallbackMaterials;
   }

   auto* cmdList = device_->GetCommandList();
   auto* modelAssetComp = model->GetComponent<ModelAssetComponent>();
   ModelAsset* asset = modelAssetComp ? modelAssetComp->GetModelAsset() : nullptr;
   if (!asset) {
	  return;
   }
   const auto& meshes = asset->GetMeshData();
   const auto& materials = *effectiveMaterials;

   if (materials.empty()) {
      Logger::Warning("[ModelRenderer] No materials assigned, skip draw");
      return;
   }
   if (modelData.textures.empty()) {
      Logger::Warning("[ModelRenderer] No textures assigned, skip draw");
      return;
   }

   Camera* camera = modelData.camera;

   // LightDataBufferを取得
   LightDataBuffer* lightBuffer = lightManager->GetLightDataBuffer();

   bool skinningEnabled = true;
   if (const auto* animationComponent = model->GetComponent<AnimationComponent>()) {
	  skinningEnabled = animationComponent->useSkinning;
   }

   SkinCluster* skinCluster = modelAssetComp->GetSkinCluster();
   const bool canUseSkinning = skinningEnabled && skinCluster;

   const auto* skinningComputePipeline = psoManager_ ? psoManager_->GetComputePipeline(kSkinningComputePipelineName) : nullptr;
   auto* skinningComputeRootSignature = (psoManager_ && skinningComputePipeline)
	  ? psoManager_->GetRootSignature(skinningComputePipeline->rootSignatureName)
	  : nullptr;
   const bool useSkinning =
	  canUseSkinning &&
	  skinningComputePipeline &&
	  skinningComputePipeline->pipelineState &&
	  skinningComputeRootSignature &&
	  skinCluster->paletteSrvHandle.second.ptr != 0;

   // マテリアルにパイプライン名が指定されていればそれを使用、なければデフォルト "Object3D"
   const std::string& materialPipelineName = materials[0]->GetPipelineName();
   const std::string defaultPipelineName = materialPipelineName.empty() ? "Object3D" : materialPipelineName;
   const std::string pipelineName = defaultPipelineName;

   // マテリアルに blendMode が設定されていればそれを優先、なければ DrawCommand の blendMode を使用
   const BlendMode resolvedBlendMode = materials[0]->GetBlendMode().value_or(modelData.blendMode);

   PipelineState* graphicsPipeline = psoManager_ ? psoManager_->GetPipeline(pipelineName, resolvedBlendMode) : nullptr;
   if (!graphicsPipeline && psoManager_) {
	  graphicsPipeline = psoManager_->GetPipeline(pipelineName, BlendMode::kBlendModeNone);
   }
   if (!graphicsPipeline) {
	  Logger::Error("[ModelRenderer] Failed to resolve graphics pipeline: " + pipelineName);
	  return;
   }

   // 使用パイプラインを設定
   setPipelineFunc(pipelineName, resolvedBlendMode);

   auto resolvePipelineSlot = [this, &pipelineName](const char* semantic, UINT fallback) -> UINT {
	  if (!psoManager_) {
        return fallback;
	  }

	  auto resolved = psoManager_->ResolvePipelineRootParameter(pipelineName, semantic);
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
   const UINT environmentTextureSlot = resolvePipelineSlot("envmap", RootBindingSlots::Object3D::kEnvMap);

   TransformationMatrix* transformationMatrix = model->GetTransformationMatrix();
   if (!transformationMatrix) {
	  Logger::Warning("[ModelRenderer] TransformationMatrix is missing, skip draw");
	  return;
   }

   if (useSkinning) {
	  const UINT skinningInfoSlot = psoManager_->ResolvePipelineRootParameter(kSkinningComputePipelineName, "skinninginformation").value_or(0);
	  const UINT paletteSlot = psoManager_->ResolvePipelineRootParameter(kSkinningComputePipelineName, "matrixpalette").value_or(1);
	  const UINT inputVerticesSlot = psoManager_->ResolvePipelineRootParameter(kSkinningComputePipelineName, "inputvertices").value_or(2);
	  const UINT influencesSlot = psoManager_->ResolvePipelineRootParameter(kSkinningComputePipelineName, "influences").value_or(3);
	  const UINT outputVerticesSlot = psoManager_->ResolvePipelineRootParameter(kSkinningComputePipelineName, "outputvertices").value_or(4);

	  cmdList->SetComputeRootSignature(skinningComputeRootSignature->GetRootSignature());
	  cmdList->SetPipelineState(skinningComputePipeline->pipelineState.Get());

	  for (size_t i = 0; i < meshes.size(); ++i) {
		 if (!skinCluster->HasComputeSkinningResources(i) || i >= skinCluster->skinnedVertexResourceStates.size()) {
			continue;
		 }

		 ID3D12Resource* skinnedVertexResource = skinCluster->skinnedVertexResources[i].Get();
		 TransitionResource(
			cmdList,
			skinnedVertexResource,
			skinCluster->skinnedVertexResourceStates[i],
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		 skinCluster->skinnedVertexResourceStates[i] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		 cmdList->SetComputeRootConstantBufferView(
			skinningInfoSlot,
			skinCluster->skinningInformationResources[i]->GetGPUVirtualAddress());
		 cmdList->SetComputeRootDescriptorTable(paletteSlot, skinCluster->paletteSrvHandle.second);
		 cmdList->SetComputeRootDescriptorTable(inputVerticesSlot, skinCluster->inputVertexSrvHandles[i].second);
		 cmdList->SetComputeRootDescriptorTable(influencesSlot, skinCluster->influenceSrvHandles[i].second);
		 cmdList->SetComputeRootDescriptorTable(outputVerticesSlot, skinCluster->skinnedVertexUavHandles[i].second);

		 const UINT vertexCount = static_cast<UINT>(meshes[i].vertices.size());
		 const UINT dispatchCount = (vertexCount + kSkinningThreadGroupSize - 1) / kSkinningThreadGroupSize;
		 cmdList->Dispatch(dispatchCount, 1, 1);

		 TransitionResource(
			cmdList,
			skinnedVertexResource,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		 skinCluster->skinnedVertexResourceStates[i] = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	  }

	  // Compute DispatchでPSOが切り替わるため、以降のRoot Parameter設定前に描画PSOへ戻す。
	  cmdList->SetGraphicsRootSignature(graphicsPipeline->GetRootSignature());
	  cmdList->SetPipelineState(graphicsPipeline->GetPipelineState());
   }

   // 共通バインディング（全メッシュで共通）
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

   // 各メッシュごとの描画
   for (size_t i = 0; i < meshes.size(); ++i) {
	  // --- マテリアル取得（不足分は先頭を使い回し） ---
	  const Material* mat = (i < materials.size()) ? materials[i] : materials[0];
	  if (!mat) {
		 Logger::Warning("[ModelRenderer] Material is null at index " + std::to_string(i) + ", skip mesh");
		 continue;
	  }

	  // --- テクスチャSRV取得（不足分は先頭を使い回し） ---
	  D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = (i < modelData.textures.size()) ? modelData.textures[i] : modelData.textures[0];
	  if (srvHandle.ptr == 0) {
		 Logger::Warning("[ModelRenderer] Invalid texture SRV handle at index " + std::to_string(i) + ", skip mesh");
		 continue;
	  }

	  // --- メッシュ固有のバインディング ---
	  // Root Parameter 0: Material (Pixel Shader)
	  cmdList->SetGraphicsRootConstantBufferView(materialSlot, mat->GetMaterialResource()->GetGPUVirtualAddress());

	  // Root Parameter 8: Texture (t4)
	  cmdList->SetGraphicsRootDescriptorTable(textureSlot, srvHandle);

	  // EnvironmentTexture (t5): バインド (設定されている場合)
   if (modelData.environmentTextureSrvHandle.ptr != 0) {
		 cmdList->SetGraphicsRootDescriptorTable(environmentTextureSlot, modelData.environmentTextureSrvHandle);
	  }

     // 頂点バッファとプリミティブトポロジを設定
      if (useSkinning && skinCluster->HasComputeSkinningResources(i)) {
		 const D3D12_VERTEX_BUFFER_VIEW* skinnedVertexBufferView = skinCluster->GetSkinnedVertexBufferView(i);
		 cmdList->IASetVertexBuffers(0, 1, skinnedVertexBufferView);
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
