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
#include "Graphics/Texture.h"

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

   // ComputeのUAV出力とInput Assemblerの頂点読み取りは同じリソースを共有するため、
   // 用途を切り替える境界で明示的に可視化する。
   CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, before, after);
   cmdList->ResourceBarrier(1, &barrier);
}
}

void ModelRenderer::Initialize(GraphicsDevice* device, PSOManager* psoManager, AssetManager* assetManager) {
   device_ = device;
   psoManager_ = psoManager;
   assetManager_ = assetManager;
   MaterialComponent::SetPipelineManager(psoManager);
   // A typed null cube reads zero and makes a missing environment map safe even when
   // the pixel shader declares it. A zero descriptor handle is never bound.
   nullCubeAllocation_ = device_->AllocateSrvDescriptor();
   if (nullCubeAllocation_) {
      const UINT index = nullCubeAllocation_->GetIndex();
      CD3DX12_CPU_DESCRIPTOR_HANDLE cpu(device_->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart(), index, device_->GetDescriptorSizeCBVSRVUAV());
      nullCubeHandle_ = CD3DX12_GPU_DESCRIPTOR_HANDLE(device_->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart(), index, device_->GetDescriptorSizeCBVSRVUAV());
      D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
      desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
      desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      desc.TextureCube.MipLevels = 1;
      device_->GetDevice()->CreateShaderResourceView(nullptr, &desc, cpu);
   }

}

void ModelRenderer::DrawModel(const ModelDrawData& modelData,
   Material* defaultMaterial,
   LightManager* lightManager,
   std::function<void(const std::string&, BlendMode)> setPipelineFunc) {
   Model* model = modelData.model;
   if (!model || !device_ || !psoManager_) return;
   auto* materialComponent = model->GetComponent<MaterialComponent>();
   if (!materialComponent) {
	  return;
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
   Camera* camera = modelData.camera;

   // LightDataBufferを取得
   LightDataBuffer* lightBuffer = lightManager ? lightManager->GetLightDataBuffer() : nullptr;

   bool skinningEnabled = true;
   if (const auto* animationComponent = model->GetComponent<AnimationComponent>()) {
	  skinningEnabled = animationComponent->useSkinning;
   }

   SkinCluster* skinCluster = meshComponent ? meshComponent->GetSkinCluster() : nullptr;
   const bool canUseSkinning = asset && skinningEnabled && skinCluster;

   // アセット・ユーザー設定・GPU資源・Compute PSOがすべて揃った場合だけGPUスキニングへ入る。
   // 不完全なロード状態では静的頂点へフォールバックし、描画全体を失わない。
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

   TransformationMatrix* transformationMatrix = model->GetTransformationMatrix();
   if (useSkinning) {
      // ルートスロットをJSONの意味名から解決し、Computeルート定義の並び替えをC++から隠蔽する。
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
         if (modelData.meshIndex && *modelData.meshIndex != i) continue;
		 if (!skinCluster->HasComputeSkinningResources(i) || i >= skinCluster->skinnedVertexResourceStates.size()) {
			continue;
		 }

		 // 前フレームに頂点入力だった出力バッファをUAVへ戻してから、同じメッシュ番号の
		 // 元頂点・ウェイト・パレットを使って上書きする。
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
		 // 端数頂点も処理するため切り上げ除算し、シェーダー側の範囲判定へ余剰スレッドを委ねる。
		 const UINT dispatchCount = (vertexCount + kSkinningThreadGroupSize - 1) / kSkinningThreadGroupSize;
		 cmdList->Dispatch(dispatchCount, 1, 1);

		 // Dispatch完了後のUAV書き込みを頂点フェッチから可視にし、直後のGraphics描画へ接続する。
		 TransitionResource(
			cmdList,
			skinnedVertexResource,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		 skinCluster->skinnedVertexResourceStates[i] = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	  }

   }

   // Every draw resolves its own slot. Root signatures can differ, so all consumed resources
   // are rebound after selecting the PSO (including after a compute skinning dispatch).
   auto bindMaterial = [&](size_t slot) {
      Material* material = materialComponent->GetMaterial(slot);
      if (!material) material = defaultMaterial;
      if (!material || !psoManager_) return false;
      std::string name = material->GetPipelineName().empty() ? "Object3D" : material->GetPipelineName();
      auto report = [&](const std::string& reason) {
         const std::string key = model->GetObjectName() + ":" + std::to_string(slot) + ":" + name + ":" + reason;
         if (reportedFailures_.insert(key).second) Logger::Warning("[ModelRenderer] object=" +
            model->GetObjectName() + ", slot=" + std::to_string(slot) + ", pipeline=" + name + ": " + reason);
      };
      const auto* contract = psoManager_->GetModelPipeline(name);
      if (!contract) {
         report("unavailable or incompatible; using Object3D");
         name = "Object3D";
         contract = psoManager_->GetModelPipeline(name);
      }
      if (!contract) return false;
      if (contract->parameters.empty() && !material->GetParameters().empty()) {
         report("parameters supplied to a pipeline without a parameter schema; draw skipped");
         return false;
      }
      const BlendMode blend = psoManager_->ResolveModelBlendMode(name, material->GetBlendMode().value_or(modelData.blendMode));
      const std::string variant = meshComponent->IsReverseFaces() ? PSOManager::MakeReversedFacePipelineName(name) : name;
      auto* pipeline = psoManager_->GetPipeline(variant, blend);
      if (!pipeline || !pipeline->GetPipelineState()) { report("missing PSO; draw skipped"); return false; }

      D3D12_GPU_DESCRIPTOR_HANDLE texture = modelData.textures.empty() ? D3D12_GPU_DESCRIPTOR_HANDLE{} :
         modelData.textures[slot < modelData.textures.size() ? slot : 0];
      if (!materialComponent->GetTextureName(slot).empty()) {
         auto* overrideTexture = materialComponent->GetTexture(slot);
         if (overrideTexture && !overrideTexture->GetMetadata().IsCubemap()) texture = overrideTexture->GetTextureSrvHandleGPU();
         else report("unresolved/non-2D texture; using draw texture");
      }
      const auto environment = modelData.environmentTextureSrvHandle.ptr ? modelData.environmentTextureSrvHandle : nullCubeHandle_;
      // Validate every required resource before emitting root bindings or a draw.
      std::vector<D3D12_GPU_VIRTUAL_ADDRESS> buffers(contract->bindings.size());
      std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> descriptors(contract->bindings.size());
      for (size_t i = 0; i < contract->bindings.size(); ++i) {
         const auto& binding = contract->bindings[i];
         if (!binding.required) continue;
         const auto& semantic = binding.semantic;
         ID3D12Resource* resource = nullptr;
         if (semantic == "material") resource = material->HasValidData() ? material->GetMaterialResource() : nullptr;
         else if (semantic == "transform") resource = transformationMatrix ? transformationMatrix->GetTransformationMatrixResource() : nullptr;
         else if (semantic == "camera") resource = camera ? camera->GetCameraResource() : nullptr;
         else if (semantic == "lightcount") resource = lightBuffer ? lightBuffer->GetLightCountResource() : nullptr;
         else if (semantic == "parameters") resource = material->PrepareParameters(*contract);
         else if (semantic == "texture") descriptors[i] = texture;
         else if (semantic == "envmap") descriptors[i] = environment;
         else if (lightBuffer) {
            if (semantic == "directionallights") descriptors[i] = lightBuffer->GetDirectionalLightSRV();
            else if (semantic == "pointlights") descriptors[i] = lightBuffer->GetPointLightSRV();
            else if (semantic == "spotlights") descriptors[i] = lightBuffer->GetSpotLightSRV();
            else if (semantic == "arealights") descriptors[i] = lightBuffer->GetAreaLightSRV();
         }
         if (resource) buffers[i] = resource->GetGPUVirtualAddress();
         if (!buffers[i] && !descriptors[i].ptr) {
            report("missing/invalid resource or parameter: " + semantic + "; draw skipped");
            return false;
         }
      }
      setPipelineFunc(variant, blend);
      // SetPipeline may cache the previous graphics PSO across a compute dispatch.
      cmdList->SetGraphicsRootSignature(pipeline->GetRootSignature());
      cmdList->SetPipelineState(pipeline->GetPipelineState());
      for (UINT i = 0; i < static_cast<UINT>(contract->bindings.size()); ++i) {
         if (buffers[i]) cmdList->SetGraphicsRootConstantBufferView(i, buffers[i]);
         else if (descriptors[i].ptr) cmdList->SetGraphicsRootDescriptorTable(i, descriptors[i]);
      }
      return true;
   };

   if (primitiveMesh) {
      if (!bindMaterial(0)) return;
      cmdList->IASetVertexBuffers(0, 1, &primitiveMesh->GetVertexBufferView());
      cmdList->IASetIndexBuffer(&primitiveMesh->GetIndexBufferView());
      cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      cmdList->DrawIndexedInstanced(primitiveMesh->GetIndexCount(), 1, 0, 0, 0);
      return;
   }

   // Slots retain the existing submesh order; absent slots inherit slot zero.
   for (size_t i = 0; modelMeshes && i < modelMeshes->size(); ++i) {
      if (modelData.meshIndex && *modelData.meshIndex != i) continue;
      if (!bindMaterial(i)) continue;

     // 頂点バッファとプリミティブトポロジを設定
      // Compute出力があるメッシュだけ動的頂点を使い、未対応メッシュは元の頂点へ個別に戻す。
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
