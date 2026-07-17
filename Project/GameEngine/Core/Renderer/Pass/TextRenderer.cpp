#include "pch.h"
#include "TextRenderer.h"
#include "Asset/Font/FontManager.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/ResourceHelper.h"
#include "PSOManager.h"
#include <cmath>
#include <limits>
#include <unordered_map>

namespace GameEngine {
namespace {
size_t GrowCapacity(size_t required, size_t minimum) {
   size_t capacity = minimum;
   while (capacity < required) {
      capacity *= 2u;
   }
   return capacity;
}
}

bool TextRenderer::Initialize(GraphicsDevice* device, PSOManager* psoManager, FontManager* fontManager) {
   if (!device || !psoManager || !fontManager) {
      Logger::Error("[TextRenderer] Invalid initialization arguments.");
      return false;
   }

   device_ = device;
   psoManager_ = psoManager;
   fontManager_ = fontManager;
   viewportBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), 256u);
   return viewportBuffer_ != nullptr;
}

void TextRenderer::BeginFrame() {
   vertices_.clear();
   indices_.clear();
}

std::vector<TextDrawData> TextRenderer::QueueText(
   const TextLayoutResult& layout,
   const TextStyle& style,
   const Transform& transform,
   size_t visibleGlyphCount,
   uint32_t screenWidth,
   uint32_t screenHeight) {
   std::vector<TextDrawData> drawDataList;
   if (layout.glyphs.empty() || screenWidth == 0 || screenHeight == 0) {
      return drawDataList;
   }

   screenWidth_ = screenWidth;
   screenHeight_ = screenHeight;
   struct PageGeometry {
      D3D12_GPU_DESCRIPTOR_HANDLE atlasSrv = {};
      Vector4 atlasParameters = {};
      std::vector<TextVertex> vertices;
      std::vector<uint32_t> indices;
   };

   const Vector2 anchorPosition = CalculateAnchorPosition(style.screenAnchor, screenWidth, screenHeight);
   const Vector2 pivotOffset = { layout.size.x * style.pivot.x, layout.size.y * style.pivot.y };
   const Vector2 origin = {
      anchorPosition.x + transform.translation.x,
      anchorPosition.y + transform.translation.y
   };

   std::vector<PageGeometry> pageGeometry;
   std::unordered_map<UINT64, size_t> pageLookup;
   const size_t glyphLimit = (std::min)(visibleGlyphCount, layout.glyphs.size());
   for (size_t placementIndex = 0; placementIndex < glyphLimit; ++placementIndex) {
      const GlyphPlacement& placement = layout.glyphs[placementIndex];
      const GlyphInfo& glyph = placement.glyph;
      if (glyph.bitmapSize.x <= 0.0f || glyph.bitmapSize.y <= 0.0f || glyph.atlasSrv.ptr == 0) {
         continue;
      }

      size_t geometryIndex = 0;
      const auto lookup = pageLookup.find(glyph.atlasSrv.ptr);
      if (lookup == pageLookup.end()) {
         geometryIndex = pageGeometry.size();
         pageLookup[glyph.atlasSrv.ptr] = geometryIndex;
         pageGeometry.push_back({
            glyph.atlasSrv,
            {
               glyph.atlasSize.x,
               glyph.atlasSize.y,
               glyph.distanceRange,
               static_cast<float>(glyph.atlasType)
            }
         });
      } else {
         geometryIndex = lookup->second;
      }

      PageGeometry& geometry = pageGeometry[geometryIndex];
      const Vector2 localTopLeft = placement.position - pivotOffset;
      const Vector2 localTopRight = { localTopLeft.x + glyph.bitmapSize.x, localTopLeft.y };
      const Vector2 localBottomLeft = { localTopLeft.x, localTopLeft.y + glyph.bitmapSize.y };
      const Vector2 localBottomRight = { localTopRight.x, localBottomLeft.y };
      const Vector4 atlasParameters = geometry.atlasParameters;

      const uint32_t firstVertex = static_cast<uint32_t>(geometry.vertices.size());
      geometry.vertices.push_back({ TransformPoint(localTopLeft, transform, origin), glyph.uvMin, style.color, atlasParameters });
      geometry.vertices.push_back({ TransformPoint(localTopRight, transform, origin), { glyph.uvMax.x, glyph.uvMin.y }, style.color, atlasParameters });
      geometry.vertices.push_back({ TransformPoint(localBottomLeft, transform, origin), { glyph.uvMin.x, glyph.uvMax.y }, style.color, atlasParameters });
      geometry.vertices.push_back({ TransformPoint(localBottomRight, transform, origin), glyph.uvMax, style.color, atlasParameters });
      geometry.indices.insert(geometry.indices.end(), {
         firstVertex + 0u, firstVertex + 1u, firstVertex + 2u,
         firstVertex + 1u, firstVertex + 3u, firstVertex + 2u
      });
   }

   for (PageGeometry& geometry : pageGeometry) {
      if (geometry.indices.empty()) {
         continue;
      }

      const uint32_t globalBaseVertex = static_cast<uint32_t>(vertices_.size());
      const uint32_t startIndex = static_cast<uint32_t>(indices_.size());
      vertices_.insert(vertices_.end(), geometry.vertices.begin(), geometry.vertices.end());
      for (uint32_t localIndex : geometry.indices) {
         indices_.push_back(globalBaseVertex + localIndex);
      }

      TextDrawData drawData{};
      drawData.atlasSrv = geometry.atlasSrv;
      drawData.indexCount = static_cast<uint32_t>(geometry.indices.size());
      drawData.startIndex = startIndex;
      drawData.baseVertex = 0;
      drawData.sortingOrder = style.sortingOrder;
      drawDataList.push_back(drawData);
   }

   return drawDataList;
}

bool TextRenderer::UploadBuffers() {
   if (vertices_.empty() || indices_.empty()) {
      return true;
   }
   if (!EnsureBufferCapacity(vertices_.size(), indices_.size())) {
      return false;
   }

   void* mappedVertices = nullptr;
   if (FAILED(vertexBuffer_->Map(0, nullptr, &mappedVertices)) || !mappedVertices) {
      Logger::Error("[TextRenderer] Failed to map the vertex buffer.");
      return false;
   }
   std::memcpy(mappedVertices, vertices_.data(), vertices_.size() * sizeof(TextVertex));
   vertexBuffer_->Unmap(0, nullptr);

   void* mappedIndices = nullptr;
   if (FAILED(indexBuffer_->Map(0, nullptr, &mappedIndices)) || !mappedIndices) {
      Logger::Error("[TextRenderer] Failed to map the index buffer.");
      return false;
   }
   std::memcpy(mappedIndices, indices_.data(), indices_.size() * sizeof(uint32_t));
   indexBuffer_->Unmap(0, nullptr);

   struct ViewportData {
      Vector2 size;
      Vector2 inverseSize;
   };
   ViewportData* viewportData = nullptr;
   if (FAILED(viewportBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&viewportData))) || !viewportData) {
      Logger::Error("[TextRenderer] Failed to map the viewport buffer.");
      return false;
   }
   viewportData->size = { static_cast<float>(screenWidth_), static_cast<float>(screenHeight_) };
   viewportData->inverseSize = { 1.0f / viewportData->size.x, 1.0f / viewportData->size.y };
   viewportBuffer_->Unmap(0, nullptr);
   return true;
}

void TextRenderer::DrawUIText(
   const TextDrawData& textData,
   const std::function<void(const std::string&, BlendMode)>& setPipelineFunc) {
   if (!device_ || !psoManager_ || !setPipelineFunc || textData.indexCount == 0 || textData.atlasSrv.ptr == 0) {
      return;
   }

   setPipelineFunc("Text", BlendMode::kBlendModeNormal);
   const auto viewportSlot = psoManager_->ResolvePipelineRootParameter("Text", "viewport");
   const auto atlasSlot = psoManager_->ResolvePipelineRootParameter("Text", "fontatlas");
   if (!viewportSlot || !atlasSlot) {
      Logger::Error("[TextRenderer] Failed to resolve Text root slots.");
      return;
   }

   ID3D12GraphicsCommandList* commandList = device_->GetCommandList();
   commandList->SetGraphicsRootConstantBufferView(viewportSlot.value(), viewportBuffer_->GetGPUVirtualAddress());
   commandList->SetGraphicsRootDescriptorTable(atlasSlot.value(), textData.atlasSrv);
   commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
   commandList->IASetIndexBuffer(&indexBufferView_);
   commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   commandList->DrawIndexedInstanced(textData.indexCount, 1, textData.startIndex, textData.baseVertex, 0);
}

void TextRenderer::Finalize() {
   viewportBuffer_.Reset();
   indexBuffer_.Reset();
   vertexBuffer_.Reset();
   vertices_.clear();
   indices_.clear();
   vertexCapacity_ = 0;
   indexCapacity_ = 0;
   fontManager_ = nullptr;
   psoManager_ = nullptr;
   device_ = nullptr;
}

bool TextRenderer::EnsureBufferCapacity(size_t vertexCount, size_t indexCount) {
   if (!device_ || !device_->GetDevice()) {
      return false;
   }

   if (vertexCount > vertexCapacity_) {
      vertexCapacity_ = GrowCapacity(vertexCount, 1024u);
      vertexBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), vertexCapacity_ * sizeof(TextVertex));
      if (!vertexBuffer_) {
         return false;
      }
      vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
      vertexBufferView_.SizeInBytes = static_cast<UINT>(vertexCapacity_ * sizeof(TextVertex));
      vertexBufferView_.StrideInBytes = sizeof(TextVertex);
   }

   if (indexCount > indexCapacity_) {
      indexCapacity_ = GrowCapacity(indexCount, 1536u);
      indexBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), indexCapacity_ * sizeof(uint32_t));
      if (!indexBuffer_) {
         return false;
      }
      indexBufferView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
      indexBufferView_.SizeInBytes = static_cast<UINT>(indexCapacity_ * sizeof(uint32_t));
      indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
   }
   return true;
}

Vector2 TextRenderer::CalculateAnchorPosition(UIAnchor anchor, uint32_t screenWidth, uint32_t screenHeight) {
   const float width = static_cast<float>(screenWidth);
   const float height = static_cast<float>(screenHeight);
   switch (anchor) {
      case UIAnchor::TopCenter:    return { width * 0.5f, 0.0f };
      case UIAnchor::TopRight:     return { width, 0.0f };
      case UIAnchor::MiddleLeft:   return { 0.0f, height * 0.5f };
      case UIAnchor::MiddleCenter: return { width * 0.5f, height * 0.5f };
      case UIAnchor::MiddleRight:  return { width, height * 0.5f };
      case UIAnchor::BottomLeft:   return { 0.0f, height };
      case UIAnchor::BottomCenter: return { width * 0.5f, height };
      case UIAnchor::BottomRight:  return { width, height };
      case UIAnchor::TopLeft:
      default:                     return { 0.0f, 0.0f };
   }
}

Vector2 TextRenderer::TransformPoint(const Vector2& point, const Transform& transform, const Vector2& origin) {
   const float scaledX = point.x * transform.scale.x;
   const float scaledY = point.y * transform.scale.y;
   const float rotation = transform.GetActiveEuler().z;
   const float cosine = std::cos(rotation);
   const float sine = std::sin(rotation);
   return {
      origin.x + scaledX * cosine - scaledY * sine,
      origin.y + scaledX * sine + scaledY * cosine
   };
}

} // namespace GameEngine
