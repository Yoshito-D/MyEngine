#include "pch.h"
#include "ModelRenderer.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/Mesh.h"
#include "Graphics/TransformationMatrix.h"
#include "Model/Model.h"
#include "Graphics/Material.h"
#include "PSOManager.h"
#include "LightManager.h"
#include "DirectionalLight.h"
#include "PointLight.h"
#include "SpotLight.h"
#include "AreaLight.h"
#include "LightDataBuffer.h"
#include "Model/ModelAsset.h"
#include "Component/Model/AnimationComponent.h"
#include "Component/MaterialComponent.h"
#include "Component/MeshComponent.h"

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
   auto* meshComponent = model->GetComponent<MeshComponent>();
   ModelAsset* asset = meshComponent ? meshComponent->GetModelAsset() : nullptr;
   Mesh* primitiveMesh = meshComponent && meshComponent->GetSourceType() == MeshComponent::SourceType::Primitive
      ? meshComponent->EnsureMesh()
      : nullptr;
   if (!asset && !primitiveMesh) {
	  return;
   }
   const std::vector<MeshData>* modelMeshes = asset ? &asset->GetMeshData() : nullptr;
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

   SkinCluster* skinCluster = meshComponent ? meshComponent->GetSkinCluster() : nullptr;
   const bool canUseSkinning = asset && skinningEnabled && skinCluster;

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
   const std::string pipelineName = meshComponent->IsReverseFaces()
      ? PSOManager::MakeReversedFacePipelineName(defaultPipelineName)
      : defaultPipelineName;

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

   auto resolvePipelineSlot = [this, &pipelineName](const char* semantic) -> std::optional<UINT> {
	  if (!psoManager_) {
		 Logger::Error("[ModelRenderer] PSOManager is null while resolving root slot: " + std::string(semantic));
		 return std::nullopt;
	  }

	  auto resolved = psoManager_->ResolvePipelineRootParameter(pipelineName, semantic);
	  if (!resolved.has_value()) {
		 Logger::Error("[ModelRenderer] Failed to resolve root slot: pipeline=" + pipelineName +
			", semantic=" + semantic);
	  }
	  return resolved;
   };

   const auto materialSlot = resolvePipelineSlot("material");
   const auto transformSlot = resolvePipelineSlot("transform");
   const auto cameraSlot = resolvePipelineSlot("camera");
   const auto lightCountSlot = resolvePipelineSlot("lightcount");
   const auto directionalLightSlot = resolvePipelineSlot("directionallights");
   const auto pointLightSlot = resolvePipelineSlot("pointlights");
   const auto spotLightSlot = resolvePipelineSlot("spotlights");
   const auto areaLightSlot = resolvePipelineSlot("arealights");
   const auto textureSlot = resolvePipelineSlot("texture");
   const auto environmentTextureSlot = resolvePipelineSlot("envmap");
   if (!materialSlot || !transformSlot || !cameraSlot || !lightCountSlot ||
	  !directionalLightSlot || !pointLightSlot || !spotLightSlot ||
	  !areaLightSlot || !textureSlot || !environmentTextureSlot) {
	  return;
   }

   TransformationMatrix* transformationMatrix = model->GetTransformationMatrix();
   if (!transformationMatrix) {
	  Logger::Warning("[ModelRenderer] TransformationMatrix is missing, skip draw");
	  return;
   }

   if (useSkinning) {
	  const auto resolveComputeSlot = [this](const char* semantic) -> std::optional<UINT> {
		 auto resolved = psoManager_->ResolvePipelineRootParameter(kSkinningComputePipelineName, semantic);
		 if (!resolved.has_value()) {
			Logger::Error("[ModelRenderer] Failed to resolve compute root slot: pipeline=" +
			   std::string(kSkinningComputePipelineName) + ", semantic=" + semantic);
		 }
		 return resolved;
	  };

	  const auto skinningInfoSlot = resolveComputeSlot("skinninginformation");
	  const auto paletteSlot = resolveComputeSlot("matrixpalette");
	  const auto inputVerticesSlot = resolveComputeSlot("inputvertices");
	  const auto influencesSlot = resolveComputeSlot("influences");
	  const auto outputVerticesSlot = resolveComputeSlot("outputvertices");
	  if (!skinningInfoSlot || !paletteSlot || !inputVerticesSlot || !influencesSlot || !outputVerticesSlot) {
		 return;
	  }

	  cmdList->SetComputeRootSignature(skinningComputeRootSignature->GetRootSignature());
	  cmdList->SetPipelineState(skinningComputePipeline->pipelineState.Get());

	  for (size_t i = 0; modelMeshes && i < modelMeshes->size(); ++i) {
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
			skinningInfoSlot.value(),
			skinCluster->skinningInformationResources[i]->GetGPUVirtualAddress());
		 cmdList->SetComputeRootDescriptorTable(paletteSlot.value(), skinCluster->paletteSrvHandle.second);
		 cmdList->SetComputeRootDescriptorTable(inputVerticesSlot.value(), skinCluster->inputVertexSrvHandles[i].second);
		 cmdList->SetComputeRootDescriptorTable(influencesSlot.value(), skinCluster->influenceSrvHandles[i].second);
		 cmdList->SetComputeRootDescriptorTable(outputVerticesSlot.value(), skinCluster->skinnedVertexUavHandles[i].second);

		 const UINT vertexCount = static_cast<UINT>((*modelMeshes)[i].vertices.size());
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

   if (primitiveMesh) {
      const Material* material = materials[0];
      const D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = modelData.textures[0];
      if (!material || textureHandle.ptr == 0) {
         Logger::Warning("[ModelRenderer] Primitive mesh material or texture is invalid, skip draw");
         return;
      }

      cmdList->SetGraphicsRootConstantBufferView(
         materialSlot.value(), material->GetMaterialResource()->GetGPUVirtualAddress());
      cmdList->SetGraphicsRootDescriptorTable(textureSlot.value(), textureHandle);
      if (modelData.environmentTextureSrvHandle.ptr != 0) {
         cmdList->SetGraphicsRootDescriptorTable(
            environmentTextureSlot.value(), modelData.environmentTextureSrvHandle);
      }
      cmdList->IASetVertexBuffers(0, 1, &primitiveMesh->GetVertexBufferView());
      cmdList->IASetIndexBuffer(&primitiveMesh->GetIndexBufferView());
      cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      cmdList->DrawIndexedInstanced(primitiveMesh->GetIndexCount(), 1, 0, 0, 0);
      return;
   }

   // 各メッシュごとの描画
   for (size_t i = 0; modelMeshes && i < modelMeshes->size(); ++i) {
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
	  cmdList->SetGraphicsRootConstantBufferView(materialSlot.value(), mat->GetMaterialResource()->GetGPUVirtualAddress());

	  // Root Parameter 8: Texture (t4)
	  cmdList->SetGraphicsRootDescriptorTable(textureSlot.value(), srvHandle);

	  // EnvironmentTexture (t5): バインド (設定されている場合)
   if (modelData.environmentTextureSrvHandle.ptr != 0) {
		 cmdList->SetGraphicsRootDescriptorTable(environmentTextureSlot.value(), modelData.environmentTextureSrvHandle);
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
	  cmdList->DrawIndexedInstanced(static_cast<UINT>((*modelMeshes)[i].indices.size()), 1, 0, 0, 0);
   }
}

} // namespace GameEngine
