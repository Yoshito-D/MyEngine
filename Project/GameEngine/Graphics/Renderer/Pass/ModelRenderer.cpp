#include "GameEngine/pch.h"
#include "GameEngine/Graphics/Renderer/Pass/ModelRenderer.h"
#include "GameEngine/Graphics/Device/GraphicsDevice.h"
#include "GameEngine/Graphics/Resources/Mesh.h"
#include "GameEngine/Graphics/Resources/TransformationMatrix.h"
#include "GameEngine/Object/Model/Model.h"
#include "GameEngine/Graphics/Resources/Material.h"
#include "GameEngine/Graphics/Pipeline/PSOManager.h"
#include "GameEngine/Graphics/Renderer/Light/LightManager.h"
#include "GameEngine/Scene/Light/DirectionalLight.h"
#include "GameEngine/Scene/Light/PointLight.h"
#include "GameEngine/Scene/Light/SpotLight.h"
#include "GameEngine/Scene/Light/AreaLight.h"
#include "GameEngine/Graphics/Renderer/Light/LightDataBuffer.h"
#include "GameEngine/Assets/Model/ModelAsset.h"
#include "GameEngine/Object/Component/Animation/AnimationComponent.h"
#include "GameEngine/Object/Component/Rendering/MaterialComponent.h"
#include "GameEngine/Object/Component/Rendering/MeshComponent.h"
#include "GameEngine/Graphics/Resources/Texture.h"

namespace GameEngine {
namespace {
constexpr const char* kSkinningComputePipelineName = "SkinningCompute";

}

void ModelRenderer::Initialize(GraphicsDevice* device, const PSOManager* psoManager, AssetManager* assetManager) {
   device_ = device;
   psoManager_ = psoManager;
   assetManager_ = assetManager;
   MaterialComponent::SetPipelineManager(psoManager);
   // 型付きのnullキューブは0を返すため、ピクセルシェーダーが環境マップを宣言していても
   // 欠落した環境マップを安全に扱える。0のデスクリプターハンドルはバインドしない。
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
   const ModelAsset* asset = meshComponent ? meshComponent->GetModelAsset() : nullptr;
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
	  skinningEnabled = animationComponent->DescribeSettings().useSkinning;
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
	  skinCluster->HasPalette();

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
         skinCluster->DispatchSkinning(cmdList, i, { *skinningInfoSlot, *paletteSlot, *inputVerticesSlot, *influencesSlot, *outputVerticesSlot });
	  }

   }

   // 各描画は自身のスロットを解決する。ルートシグネチャは異なる可能性があるため、使用する全リソースを
   // PSO選択後（コンピュートスキニングのディスパッチ後を含む）に再バインドする。
   auto bindMaterial = [&](size_t slot) {
      const Material* material = materialComponent->GetMaterial(slot);
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
   // ルートバインドや描画を発行する前に、必要なリソースをすべて検証する。
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
   // SetPipelineはコンピュートディスパッチをまたいで以前のグラフィックスPSOをキャッシュする場合がある。
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

   // スロットは既存のサブメッシュ順を維持し、未設定スロットはスロット0を継承する。
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
