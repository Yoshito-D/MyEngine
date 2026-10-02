#include "GameEngine/Graphics/Renderer/Pass/PlayerShadowPass.h"
#include "GameEngine/Graphics/Renderer/Pass/FrameContext.h"
#include "GameEngine/Graphics/Device/GraphicsDevice.h"
#include "GameEngine/Graphics/Device/OffscreenRenderTarget.h"
#include "GameEngine/Graphics/Resources/TransformationMatrix.h"
#include "GameEngine/Graphics/Resources/Mesh.h"
#include "GameEngine/Object/Model/Model.h"
#include "GameEngine/Object/Component/Rendering/MeshComponent.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Component/Animation/AnimationComponent.h"
#include "GameEngine/Math/MathUtils.h"
#include "GameEngine/Utility/Logger.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace GameEngine {
namespace {
constexpr float kDirectionEpsilon = 1e-8f;
constexpr UINT kSkinningGroupSize = 1024; // Skinning.CS.hlslのnumthreadsと対応する。
constexpr const char* kSkinningPipeline = "SkinningCompute";

bool IsFinite(const Vector3& value) {
   return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsFinite(const Matrix4x4& matrix) {
   for (const auto& row : matrix.m) {
      for (float value : row) {
         if (!std::isfinite(value)) return false;
      }
   }
   return true;
}

bool IsPositive(float value) {
   return std::isfinite(value) && value > 0.0f;
}

bool IsValidSettings(const PlayerShadowSettings& settings) {
   return settings.maskWidth > 0 && settings.maskHeight > 0 &&
      settings.maskWidth <= D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION &&
      settings.maskHeight <= D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION &&
      IsPositive(settings.projectionWidth) && IsPositive(settings.projectionHeight) &&
      IsPositive(settings.cameraDistance) && IsPositive(settings.nearClip) &&
      std::isfinite(settings.farClip) && settings.farClip > settings.nearClip &&
      std::isfinite(settings.opacity) && settings.opacity >= 0.0f && settings.opacity <= 1.0f &&
      IsPositive(settings.receiverRange);
}

void TransitionResource(ID3D12GraphicsCommandList* commands, ID3D12Resource* resource,
   D3D12_RESOURCE_STATES& state, D3D12_RESOURCE_STATES nextState) {
   if (state == nextState) return;
   D3D12_RESOURCE_BARRIER barrier{};
   barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
   barrier.Transition.pResource = resource;
   barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
   barrier.Transition.StateBefore = state;
   barrier.Transition.StateAfter = nextState;
   commands->ResourceBarrier(1, &barrier);
   state = nextState;
}

// このパスでは通常描画の行列バッファを作成・更新しない。
// 読むだけにすることで、既にキューに積まれた通常描画のWVPを変えてしまう事故を避ける。
const TransformationMatrix::TransformationMatrixData* GetModelMatrices(Model* model) {
   auto* transform = model ? model->GetComponent<TransformComponent>() : nullptr;
   const auto* matrices = transform ? transform->GetTransformationMatrix() : nullptr;
   return matrices ? matrices->GetTransformationMatrixData() : nullptr;
}

bool UsesSkinning(Model* model, MeshComponent* mesh) {
   auto* animation = model->GetComponent<AnimationComponent>();
   const auto* asset = mesh->GetModelAsset();
   return asset && asset->HasSkinningData() && (!animation || animation->useSkinning);
}

bool IsValidVertexView(const D3D12_VERTEX_BUFFER_VIEW& view) {
   return view.BufferLocation != 0 && view.StrideInBytes == sizeof(Mesh::VertexData) &&
      view.SizeInBytes >= view.StrideInBytes;
}
}

PlayerShadowPass::~PlayerShadowPass() {
   // GPUの待機はRenderer/GraphicsDevice側のフレーム境界で行う。
   // デストラクターで勝手にコマンドリストを実行すると、まだ記録中の描画まで送信してしまう。
   ReleaseResources();
}

bool PlayerShadowPass::ReportFailure(const std::string& reason) {
   if (lastFailure_ != reason) {
      Logger::Warning("[PlayerShadowPass] " + reason);
      lastFailure_ = reason;
   }
   return false;
}

void PlayerShadowPass::ReleaseResources() {
   if (maskConstantBuffer_ && mappedMaskConstants_) maskConstantBuffer_->Unmap(0, nullptr);
   if (projectConstantBuffer_ && mappedProjectConstants_) projectConstantBuffer_->Unmap(0, nullptr);
   if (parameterConstantBuffer_ && mappedParameters_) parameterConstantBuffer_->Unmap(0, nullptr);
   mappedMaskConstants_ = nullptr;
   mappedProjectConstants_ = nullptr;
   mappedParameters_ = nullptr;
   maskConstantBuffer_.Reset();
   projectConstantBuffer_.Reset();
   parameterConstantBuffer_.Reset();
   maskTexture_.Reset();
   maskRtvHeap_.Reset();
   maskSrvAllocation_.reset();
   maskRtvHandle_ = {};
   maskSrvHandle_ = {};
   maskState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
   maskReady_ = false;
   initialized_ = false;
   device_ = nullptr;
   ClearFrameData();
}

bool PlayerShadowPass::Initialize(GraphicsDevice* device, const PlayerShadowSettings& settings) {
   if (initialized_) return ReportFailure("Already initialized; recreate the pass after GPU completion.");
   if (!device || !device->GetDevice() || !device->GetSRVHeap() || !IsValidSettings(settings)) {
      return ReportFailure("Invalid device or shadow settings.");
   }

   device_ = device;
   settings_ = settings;
   try {
      // ごく一部だけ作れた状態を公開しない。途中失敗時はSRVの所有権も含めて巻き戻す。
      if (!CreateMaskResources() || !CreateConstantBuffers()) {
         ReleaseResources();
         return false;
      }
   } catch (const std::exception& error) {
      ReportFailure(std::string("Resource allocation failed: ") + error.what());
      ReleaseResources();
      return false;
   }

   previousTangentUp_ = { 0.0f, 0.0f, 1.0f };
   initialized_ = true;
   lastFailure_.clear();
   return true;
}

void PlayerShadowPass::SetFrameData(const PlayerShadowFrameData& frameData) {
   frameData_ = frameData;
   hasFrameData_ = true;
}

void PlayerShadowPass::ClearFrameData() {
   frameData_ = {};
   hasFrameData_ = false;
   playerGeometry_.clear();
   receiverGeometry_.clear();
   bindings_ = {};
}

bool PlayerShadowPass::CreateMaskResources() {
   D3D12_RESOURCE_DESC desc{};
   desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   desc.Width = settings_.maskWidth;
   desc.Height = settings_.maskHeight;
   desc.DepthOrArraySize = 1;
   desc.MipLevels = 1;
   desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
   desc.SampleDesc.Count = 1;
   desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
   desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

   D3D12_HEAP_PROPERTIES heap{};
   heap.Type = D3D12_HEAP_TYPE_DEFAULT;
   D3D12_CLEAR_VALUE clearValue{};
   clearValue.Format = desc.Format;
   auto* gpu = device_->GetDevice();
   HRESULT result = gpu->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
      maskState_, &clearValue, IID_PPV_ARGS(maskTexture_.GetAddressOf()));
   if (FAILED(result)) return ReportFailure("Mask texture creation failed: " + std::to_string(result));

   // シーンのRTVヒープは既存の4スロットを使い切るため、マスク用には専用の1スロットを持つ。
   D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
   rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
   rtvHeapDesc.NumDescriptors = 1;
   result = gpu->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(maskRtvHeap_.GetAddressOf()));
   if (FAILED(result)) return ReportFailure("Mask RTV heap creation failed: " + std::to_string(result));
   maskRtvHandle_ = maskRtvHeap_->GetCPUDescriptorHandleForHeapStart();
   gpu->CreateRenderTargetView(maskTexture_.Get(), nullptr, maskRtvHandle_);

   // SRVはシーンと同じshader-visibleヒープへ作る。別ヒープへの切替で他のSRVを失効させない。
   maskSrvAllocation_ = device_->AllocateSrvDescriptor();
   if (!maskSrvAllocation_) return ReportFailure("Mask SRV allocation failed.");
   const UINT index = maskSrvAllocation_->GetIndex();
   const UINT stride = device_->GetDescriptorSizeCBVSRVUAV();
   auto cpuHandle = device_->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart();
   cpuHandle.ptr += static_cast<SIZE_T>(index) * stride;
   maskSrvHandle_ = device_->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart();
   maskSrvHandle_.ptr += static_cast<UINT64>(index) * stride;

   D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
   srv.Format = desc.Format;
   srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
   srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   srv.Texture2D.MipLevels = 1;
   gpu->CreateShaderResourceView(maskTexture_.Get(), &srv, cpuHandle);
   // clearValueは初期化を行わない。最初のDrawPlayerMaskによるクリアまではSRVを公開しない。
   return true;
}

bool PlayerShadowPass::CreateConstantBuffers() {
   // 共通ResourceHelperは失敗時にassertするため、ここではHRESULTを検査して影だけを中止する。
   const auto createBuffer = [this](size_t bytes, Microsoft::WRL::ComPtr<ID3D12Resource>& resource,
      void** mapped) {
      D3D12_HEAP_PROPERTIES heap{};
      heap.Type = D3D12_HEAP_TYPE_UPLOAD;
      D3D12_RESOURCE_DESC desc{};
      desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      desc.Width = (bytes + 255u) & ~size_t(255u);
      desc.Height = 1;
      desc.DepthOrArraySize = 1;
      desc.MipLevels = 1;
      desc.SampleDesc.Count = 1;
      desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      HRESULT result = device_->GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
         &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(resource.GetAddressOf()));
      if (FAILED(result)) return ReportFailure("Constant buffer creation failed: " + std::to_string(result));
      const D3D12_RANGE noCpuRead{ 0, 0 };
      result = resource->Map(0, &noCpuRead, mapped);
      if (FAILED(result) || !*mapped) return ReportFailure("Constant buffer Map failed: " + std::to_string(result));
      std::memset(*mapped, 0, static_cast<size_t>(desc.Width));
      return true;
   };

   // 現在のGraphicsDeviceはフレーム末尾でGPU完了を待つ。将来複数フレームを並列化する場合は、
   // これらをフレーム別の領域へ変更し、GPUが読む途中のUPLOADメモリーを上書きしないこと。
   return createBuffer(sizeof(MaskConstants), maskConstantBuffer_, reinterpret_cast<void**>(&mappedMaskConstants_)) &&
      createBuffer(sizeof(ProjectConstants), projectConstantBuffer_, reinterpret_cast<void**>(&mappedProjectConstants_)) &&
      createBuffer(sizeof(ShadowParameters), parameterConstantBuffer_, reinterpret_cast<void**>(&mappedParameters_));
}

bool PlayerShadowPass::ValidateFrameData() const {
   if (!hasFrameData_ || !frameData_.player || !frameData_.receiver || !frameData_.camera ||
      frameData_.player == frameData_.receiver || !IsFinite(frameData_.playerPosition) ||
      !IsFinite(frameData_.planetCenter) || !IsPositive(frameData_.planetRadius)) return false;

   const Vector3 offset = frameData_.playerPosition - frameData_.planetCenter;
   const float distanceSquared = offset.LengthSquared();
   if (!IsFinite(offset) || !std::isfinite(distanceSquared) || distanceSquared < kDirectionEpsilon) return false;

   auto* playerMesh = frameData_.player->GetComponent<MeshComponent>();
   auto* receiverMesh = frameData_.receiver->GetComponent<MeshComponent>();
   const auto* playerMatrices = GetModelMatrices(frameData_.player);
   const auto* receiverMatrices = GetModelMatrices(frameData_.receiver);
   // 受け面は剛体として移動・回転できる球形惑星。変形する受け面のスキニングは対象外。
   return playerMesh && receiverMesh && !UsesSkinning(frameData_.receiver, receiverMesh) &&
      playerMatrices && receiverMatrices && IsFinite(playerMatrices->world) &&
      IsFinite(receiverMatrices->world) && IsFinite(receiverMatrices->wVP) &&
      IsFinite(receiverMatrices->worldInverseTranspose);
}

bool PlayerShadowPass::UpdateShadowProjection() {
   const Vector3 offset = frameData_.playerPosition - frameData_.planetCenter;
   const float lengthSquared = offset.LengthSquared();
   if (!IsFinite(offset) || !std::isfinite(lengthSquared) || lengthSquared < kDirectionEpsilon ||
      !IsPositive(frameData_.planetRadius) || !IsValidSettings(settings_)) return false;

   const float distance = std::sqrt(lengthSquared);
   const float groundDepth = settings_.cameraDistance + distance - frameData_.planetRadius;
   // 地表より内側、または真下の地表が投影範囲外なら影を描かない。
   if (distance < frameData_.planetRadius || !std::isfinite(groundDepth) ||
      groundDepth < settings_.nearClip || groundDepth > settings_.farClip) return false;

   shadowUp_ = offset * (1.0f / distance);
   groundPosition_ = frameData_.planetCenter + shadowUp_ * frameData_.planetRadius;

   // 世界のY軸を固定すると極でLookAtが退化する。前フレームの軸を接平面に移して連続性を保つ。
   Vector3 tangent = previousTangentUp_ - shadowUp_ * previousTangentUp_.Dot(shadowUp_);
   if (!IsFinite(tangent) || tangent.LengthSquared() < kDirectionEpsilon) {
      const Vector3 reference = std::abs(shadowUp_.y) < 0.9f
         ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
      tangent = reference - shadowUp_ * reference.Dot(shadowUp_);
   }
   tangent = tangent.Normalize();
   const Vector3 eye = frameData_.playerPosition + shadowUp_ * settings_.cameraDistance;
   if (!IsFinite(eye) || !IsFinite(groundPosition_) ||
      (frameData_.playerPosition - eye).LengthSquared() < kDirectionEpsilon) return false;

   const Matrix4x4 view = MakeLookAtMatrix(eye, frameData_.playerPosition, tangent);
   // このエンジンの引数順はleft, top, right, bottom。UVのY反転は合成PSで一度だけ行う。
   const Matrix4x4 projection = MakeOrthographicMatrix(
      -settings_.projectionWidth * 0.5f, settings_.projectionHeight * 0.5f,
      settings_.projectionWidth * 0.5f, -settings_.projectionHeight * 0.5f,
      settings_.nearClip, settings_.farClip);
   shadowViewProjection_ = view * projection;
   if (!IsFinite(shadowViewProjection_)) return false;
   previousTangentUp_ = tangent;
   return true;
}

bool PlayerShadowPass::CollectGeometry(Model* model, std::vector<DrawGeometry>& geometry) {
   geometry.clear();
   auto* component = model->GetComponent<MeshComponent>();
   if (!component) return ReportFailure("Missing MeshComponent.");

   const auto append = [this, &geometry](const D3D12_VERTEX_BUFFER_VIEW& vertices,
      const D3D12_INDEX_BUFFER_VIEW& indices, size_t indexCount, size_t meshIndex) {
      const UINT indexSize = indices.Format == DXGI_FORMAT_R32_UINT ? 4u :
         (indices.Format == DXGI_FORMAT_R16_UINT ? 2u : 0u);
      if (!IsValidVertexView(vertices) || !indices.BufferLocation || !indexSize ||
         !indexCount || indexCount % 3 != 0 || indexCount > std::numeric_limits<UINT>::max() ||
         indexCount > indices.SizeInBytes / indexSize) {
         return ReportFailure("Invalid triangle mesh buffers.");
      }
      geometry.push_back({ vertices, indices, static_cast<UINT>(indexCount), meshIndex });
      return true;
   };

   if (component->GetSourceType() == MeshComponent::SourceType::Primitive) {
      // 影パス中に遅延ロード/メッシュ生成はしない。通常描画までに準備済みの形状を使う。
      const auto* mesh = component->GetMesh();
      return mesh && append(mesh->GetVertexBufferView(), mesh->GetIndexBufferView(), mesh->GetIndexCount(), 0);
   }

   const auto* asset = component->GetModelAsset();
   if (!asset) return ReportFailure("Model asset is not ready.");
   const auto& meshes = asset->GetMeshData();
   geometry.reserve(meshes.size());
   for (size_t i = 0; i < meshes.size(); ++i) {
      if (meshes[i].indices.empty()) continue;
      if (!append(asset->GetVertexBufferView(i), asset->GetIndexBufferView(i), meshes[i].indices.size(), i)) return false;
   }
   return !geometry.empty();
}

bool PlayerShadowPass::PrepareBindings(FrameContext& ctx) {
   const auto* receiverMesh = frameData_.receiver->GetComponent<MeshComponent>();
   const std::string projectName = receiverMesh->IsReverseFaces()
      ? PSOManager::MakeReversedFacePipelineName("PlayerShadowProject") : "PlayerShadowProject";
   bindings_.maskPipeline = ctx.psoManager->GetPipeline("PlayerShadowMask", BlendMode::kBlendModeNone);
   bindings_.projectPipeline = ctx.psoManager->GetPipeline(projectName, BlendMode::kBlendModeNormal);
   if (!bindings_.maskPipeline || !bindings_.projectPipeline ||
      !bindings_.maskPipeline->GetPipelineState() || !bindings_.projectPipeline->GetPipelineState()) {
      return ReportFailure("Register PlayerShadowMask and PlayerShadowProject pipelines before enabling this pass.");
   }

   // Root indexはHLSLのb0/t0番号とは別物。JSONのsemanticから取得して並び替えにも追従する。
   const auto mask = ctx.psoManager->ResolvePipelineRootParameter("PlayerShadowMask", "shadowmaskconstants");
   const auto project = ctx.psoManager->ResolvePipelineRootParameter(projectName, "shadowprojectconstants");
   const auto parameters = ctx.psoManager->ResolvePipelineRootParameter(projectName, "shadowparameters");
   const auto texture = ctx.psoManager->ResolvePipelineRootParameter(projectName, "shadowmask");
   if (!mask || !project || !parameters || !texture) return ReportFailure("Missing shadow root semantics.");
   bindings_.maskConstants = *mask;
   bindings_.projectConstants = *project;
   bindings_.parameters = *parameters;
   bindings_.maskTexture = *texture;
   return true;
}

bool PlayerShadowPass::PreparePlayerVertices(FrameContext& ctx) {
   auto* mesh = frameData_.player->GetComponent<MeshComponent>();
   if (!UsesSkinning(frameData_.player, mesh)) return true;

   auto* skin = mesh->GetSkinCluster();
   const auto* asset = mesh->GetModelAsset();
   const auto* compute = ctx.psoManager->GetComputePipeline(kSkinningPipeline);
   auto* root = compute ? ctx.psoManager->GetRootSignature(compute->rootSignatureName) : nullptr;
   if (!skin || !skin->paletteResource || skin->paletteSrvHandle.second.ptr == 0 ||
      !compute || !compute->pipelineState || !root || !root->GetRootSignature()) {
      return ReportFailure("Skinning resources are incomplete; skipping the shadow instead of using a stale pose.");
   }

   const auto information = ctx.psoManager->ResolvePipelineRootParameter(kSkinningPipeline, "skinninginformation");
   const auto palette = ctx.psoManager->ResolvePipelineRootParameter(kSkinningPipeline, "matrixpalette");
   const auto vertices = ctx.psoManager->ResolvePipelineRootParameter(kSkinningPipeline, "inputvertices");
   const auto influences = ctx.psoManager->ResolvePipelineRootParameter(kSkinningPipeline, "influences");
   const auto output = ctx.psoManager->ResolvePipelineRootParameter(kSkinningPipeline, "outputvertices");
   if (!information || !palette || !vertices || !influences || !output) return ReportFailure("Missing skinning root semantics.");

   // 途中のサブメッシュで失敗して半更新になることを避け、Dispatch前に全資源を確認する。
   for (const auto& draw : playerGeometry_) {
      const size_t i = draw.meshIndex;
      if (!skin->HasComputeSkinningResources(i) || i >= skin->skinnedVertexResourceStates.size() ||
         i >= skin->mappedSkinningInformationData.size() || !skin->mappedSkinningInformationData[i] ||
         !IsValidVertexView(skin->skinnedVertexBufferViews[i])) return ReportFailure("Invalid skinned submesh buffers.");
      const size_t vertexCount = asset->GetMeshData()[i].vertices.size();
      if (!vertexCount || vertexCount > static_cast<size_t>(D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION) * kSkinningGroupSize ||
         skin->mappedSkinningInformationData[i]->numVertices != vertexCount ||
         vertexCount > skin->skinnedVertexBufferViews[i].SizeInBytes / sizeof(Mesh::VertexData)) {
         return ReportFailure("Skinning vertex count does not match its output buffer.");
      }
   }

   auto* commands = device_->GetCommandList();
   commands->SetComputeRootSignature(root->GetRootSignature());
   commands->SetPipelineState(compute->pipelineState.Get());
   // 現時点のModelRendererにはフレーム単位の更新済み判定がないため、ここでも確実に計算する。
   // 画面外で通常描画されなかったプレイヤーにも現在のポーズを使う。ボーンPalette自体の更新はゲーム側。
   for (auto& draw : playerGeometry_) {
      const size_t i = draw.meshIndex;
      TransitionResource(commands, skin->skinnedVertexResources[i].Get(),
         skin->skinnedVertexResourceStates[i], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      commands->SetComputeRootConstantBufferView(*information, skin->skinningInformationResources[i]->GetGPUVirtualAddress());
      commands->SetComputeRootDescriptorTable(*palette, skin->paletteSrvHandle.second);
      commands->SetComputeRootDescriptorTable(*vertices, skin->inputVertexSrvHandles[i].second);
      commands->SetComputeRootDescriptorTable(*influences, skin->influenceSrvHandles[i].second);
      commands->SetComputeRootDescriptorTable(*output, skin->skinnedVertexUavHandles[i].second);
      const UINT vertexCount = static_cast<UINT>(asset->GetMeshData()[i].vertices.size());
      commands->Dispatch((vertexCount + kSkinningGroupSize - 1) / kSkinningGroupSize, 1, 1);
      // UAV書込みを、後続のInput Assemblerの頂点フェッチから見える状態へ戻す。
      TransitionResource(commands, skin->skinnedVertexResources[i].Get(),
         skin->skinnedVertexResourceStates[i], D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
      draw.vertexView = skin->skinnedVertexBufferViews[i];
   }
   return true;
}

bool PlayerShadowPass::UpdateConstantBuffers() {
   const auto* player = GetModelMatrices(frameData_.player);
   const auto* receiver = GetModelMatrices(frameData_.receiver);
   const Matrix4x4 shadowWVP = player->world * shadowViewProjection_;
   if (!IsFinite(shadowWVP)) return ReportFailure("Player shadow matrix contains non-finite values.");

   // 親行列・モデルルート変換を含む通常描画のWorldを使う。EQUAL比較の受け面WVPは再計算せずコピーする。
   mappedMaskConstants_->playerShadowWVP = shadowWVP;
   mappedProjectConstants_->worldViewProjection = receiver->wVP;
   mappedProjectConstants_->world = receiver->world;
   mappedProjectConstants_->worldInverseTranspose = receiver->worldInverseTranspose;
   mappedProjectConstants_->shadowViewProjection = shadowViewProjection_;
   mappedParameters_->shadowUpAndOpacity = { shadowUp_.x, shadowUp_.y, shadowUp_.z, settings_.opacity };
   mappedParameters_->groundPositionAndRange = { groundPosition_.x, groundPosition_.y, groundPosition_.z, settings_.receiverRange };
   return true;
}

void PlayerShadowPass::DrawGeometryList(const std::vector<DrawGeometry>& geometry) {
   auto* commands = device_->GetCommandList();
   commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   for (const auto& draw : geometry) {
      commands->IASetVertexBuffers(0, 1, &draw.vertexView);
      commands->IASetIndexBuffer(&draw.indexView);
      commands->DrawIndexedInstanced(draw.indexCount, 1, 0, 0, 0);
   }
}

void PlayerShadowPass::DrawPlayerMask(FrameContext& ctx) {
   (void)ctx;
   auto* commands = device_->GetCommandList();
   TransitionResource(commands, maskTexture_.Get(), maskState_, D3D12_RESOURCE_STATE_RENDER_TARGET);
   commands->OMSetRenderTargets(1, &maskRtvHandle_, FALSE, nullptr);
   const float black[4]{};
   commands->ClearRenderTargetView(maskRtvHandle_, black, 0, nullptr);
   const D3D12_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(settings_.maskWidth),
      static_cast<float>(settings_.maskHeight), 0.0f, 1.0f };
   const D3D12_RECT scissor{ 0, 0, static_cast<LONG>(settings_.maskWidth), static_cast<LONG>(settings_.maskHeight) };
   commands->RSSetViewports(1, &viewport);
   commands->RSSetScissorRects(1, &scissor);
   commands->SetGraphicsRootSignature(bindings_.maskPipeline->GetRootSignature());
   commands->SetPipelineState(bindings_.maskPipeline->GetPipelineState());
   commands->SetGraphicsRootConstantBufferView(bindings_.maskConstants, maskConstantBuffer_->GetGPUVirtualAddress());
   // 同じ白を上書きするシルエットなので深度とブレンドは不要。α切り抜きは現在の専用PSの対象外。
   DrawGeometryList(playerGeometry_);
   TransitionResource(commands, maskTexture_.Get(), maskState_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
   maskReady_ = true;
}

void PlayerShadowPass::RestoreSceneTarget(FrameContext& ctx) {
   // この呼出先は別途実装が必要。色・深度の両方を保持し、通常解像度のViewport/ScissorとSRVヒープを戻す。
   // PreDrawWithoutClear(true)は深度を消すため、代用すると合成のEQUAL比較が成立しなくなる。
   ctx.offscreenRenderTarget->BindPreservingContents(true);
}

void PlayerShadowPass::DrawReceiverShadow(FrameContext& ctx) {
   (void)ctx;
   auto* commands = device_->GetCommandList();
   commands->SetGraphicsRootSignature(bindings_.projectPipeline->GetRootSignature());
   commands->SetPipelineState(bindings_.projectPipeline->GetPipelineState());
   commands->SetGraphicsRootConstantBufferView(bindings_.projectConstants, projectConstantBuffer_->GetGPUVirtualAddress());
   commands->SetGraphicsRootConstantBufferView(bindings_.parameters, parameterConstantBuffer_->GetGPUVirtualAddress());
   commands->SetGraphicsRootDescriptorTable(bindings_.maskTexture, maskSrvHandle_);
   // シーンHDR形式・深度EQUAL・深度書込なし・通常α合成はPlayerShadowProjectのJSONで指定する。
   DrawGeometryList(receiverGeometry_);
}

void PlayerShadowPass::Execute(FrameContext& ctx) {
   bool gpuStateChanged = false;
   bool maskTargetBound = false;
   try {
      // 早期returnをこのラムダ内に閉じ込め、外側で必ずキャッシュ無効化と借用参照の解除を行う。
      const auto render = [&]() {
         if (!hasFrameData_) return;
         if (!initialized_ || ctx.device != device_ || !ctx.psoManager || !ctx.offscreenRenderTarget ||
            !ctx.invalidatePipelineBindingFunc || !device_->GetCommandList() || !device_->GetSRVHeap() ||
            !device_->GetDSVHeap() || !device_->GetDepthBufferResource() ||
            !ctx.offscreenRenderTarget->GetResource() ||
            !ctx.offscreenRenderTarget->GetWidth() || !ctx.offscreenRenderTarget->GetHeight() ||
            !mappedMaskConstants_ || !mappedProjectConstants_ || !mappedParameters_) {
            ReportFailure("Pass or frame context is not ready.");
            return;
         }
         if (!ValidateFrameData()) {
            ReportFailure("Invalid frame data, missing current model matrices, or a deforming receiver.");
            return;
         }
         if (!UpdateShadowProjection()) {
            ReportFailure("Cannot project onto the planet with the current camera range.");
            return;
         }
         if (!PrepareBindings(ctx) || !CollectGeometry(frameData_.player, playerGeometry_) ||
            !CollectGeometry(frameData_.receiver, receiverGeometry_) || !UpdateConstantBuffers()) return;

         // ここまで資源の作成・検証を終え、RTV変更後には失敗し得る検索や動的確保を行わない。
         gpuStateChanged = true;
         auto* heap = device_->GetSRVHeap();
         device_->GetCommandList()->SetDescriptorHeaps(1, &heap);
         if (!PreparePlayerVertices(ctx)) return;
         maskTargetBound = true;
         DrawPlayerMask(ctx);
         RestoreSceneTarget(ctx);
         maskTargetBound = false;
         DrawReceiverShadow(ctx);
         lastFailure_.clear();
      };
      render();
   } catch (const std::exception& error) {
      ReportFailure(std::string("Shadow pass failed: ") + error.what());
   }

   // マスク描画後の終了経路でも、次のTransparentPassを256x256の描画先に置き去りにしない。
   if (maskTargetBound) RestoreSceneTarget(ctx);
   if (gpuStateChanged && ctx.invalidatePipelineBindingFunc) ctx.invalidatePipelineBindingFunc();
   ClearFrameData();
}

} // namespace GameEngine
