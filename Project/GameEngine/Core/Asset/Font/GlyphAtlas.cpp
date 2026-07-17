#include "pch.h"
#include "GlyphAtlas.h"
#include "Graphics/GraphicsDevice.h"

namespace GameEngine {
namespace {
constexpr uint32_t kGlyphPadding = 1;

uint32_t AlignTexturePitch(uint32_t width) {
   return (width + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
}
}

GlyphAtlas::GlyphAtlas(GraphicsDevice* device, uint32_t pageSize)
   : device_(device), pageSize_((std::max)(pageSize, 64u)) {
}

GlyphAtlas::~GlyphAtlas() {
   if (!device_) {
      return;
   }
   for (const auto& page : pages_) {
      if (page && page->descriptorIndex != UINT_MAX) {
         device_->ReleaseSrvIndex(page->descriptorIndex);
      }
   }
}

const GlyphInfo* GlyphAtlas::FindGlyph(char32_t codePoint) const {
   const auto iterator = glyphs_.find(codePoint);
   return iterator != glyphs_.end() ? &iterator->second : nullptr;
}

const GlyphInfo* GlyphAtlas::AddGlyph(char32_t codePoint, const RasterizedGlyph& glyph) {
   if (const GlyphInfo* existing = FindGlyph(codePoint)) {
      return existing;
   }

   GlyphInfo info{};
   info.bitmapSize = { static_cast<float>(glyph.width), static_cast<float>(glyph.height) };
   info.bearing = { static_cast<float>(glyph.bitmapLeft), static_cast<float>(glyph.bitmapTop) };
   info.advance = glyph.advanceX;
   info.glyphIndex = glyph.glyphIndex;

   // 空白文字など画像を持たないグリフもadvanceをキャッシュし、FreeTypeの再呼び出しを防ぐ。
   if (glyph.width == 0 || glyph.height == 0) {
      const auto [iterator, inserted] = glyphs_.emplace(codePoint, info);
      (void)inserted;
      return &iterator->second;
   }

   if (glyph.width + kGlyphPadding * 2u > pageSize_ || glyph.height + kGlyphPadding * 2u > pageSize_) {
      Logger::Error("[GlyphAtlas] Glyph is larger than an atlas page.");
      return nullptr;
   }

   Page* selectedPage = nullptr;
   uint32_t glyphX = 0;
   uint32_t glyphY = 0;
   uint32_t pageIndex = 0;
   for (uint32_t index = 0; index < pages_.size(); ++index) {
      if (TryAllocate(*pages_[index], glyph.width, glyph.height, glyphX, glyphY)) {
         selectedPage = pages_[index].get();
         pageIndex = index;
         break;
      }
   }

   if (!selectedPage) {
      selectedPage = CreatePage();
      if (!selectedPage || !TryAllocate(*selectedPage, glyph.width, glyph.height, glyphX, glyphY)) {
         Logger::Error("[GlyphAtlas] Failed to allocate a glyph in a new atlas page.");
         return nullptr;
      }
      pageIndex = static_cast<uint32_t>(pages_.size() - 1u);
   }

   for (uint32_t row = 0; row < glyph.height; ++row) {
      const size_t sourceOffset = static_cast<size_t>(row) * glyph.width;
      const size_t destinationOffset = static_cast<size_t>(glyphY + row) * pageSize_ + glyphX;
      std::memcpy(
         selectedPage->pixels.data() + destinationOffset,
         glyph.pixels.data() + sourceOffset,
         glyph.width);
   }
   selectedPage->dirty = true;

   info.uvMin = {
      static_cast<float>(glyphX) / static_cast<float>(pageSize_),
      static_cast<float>(glyphY) / static_cast<float>(pageSize_)
   };
   info.uvMax = {
      static_cast<float>(glyphX + glyph.width) / static_cast<float>(pageSize_),
      static_cast<float>(glyphY + glyph.height) / static_cast<float>(pageSize_)
   };
   info.atlasPage = pageIndex;
   info.atlasSrv = selectedPage->srvGpu;

   const auto [iterator, inserted] = glyphs_.emplace(codePoint, info);
   (void)inserted;
   return &iterator->second;
}

void GlyphAtlas::FlushPendingUploads() {
   for (auto& page : pages_) {
      if (page && page->dirty) {
         UploadPage(*page);
      }
   }
}

void GlyphAtlas::ReleaseIntermediateResources() {
   intermediateResources_.clear();
}

GlyphAtlas::Page* GlyphAtlas::CreatePage() {
   if (!device_ || !device_->GetDevice()) {
      return nullptr;
   }

   auto page = std::make_unique<Page>();
   page->pixels.resize(static_cast<size_t>(pageSize_) * pageSize_, 0u);

   const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
   const CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
      DXGI_FORMAT_R8_UNORM,
      pageSize_,
      pageSize_,
      1,
      1);
   const HRESULT result = device_->GetDevice()->CreateCommittedResource(
      &heapProperties,
      D3D12_HEAP_FLAG_NONE,
      &textureDesc,
      D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr,
      IID_PPV_ARGS(page->texture.GetAddressOf()));
   if (FAILED(result)) {
      Logger::Error("[GlyphAtlas] Failed to create atlas texture.");
      return nullptr;
   }

   page->descriptorIndex = device_->GetNextSrvIndex();
   page->srvCpu = CD3DX12_CPU_DESCRIPTOR_HANDLE(
      device_->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart(),
      page->descriptorIndex,
      device_->GetDescriptorSizeCBVSRVUAV());
   page->srvGpu = CD3DX12_GPU_DESCRIPTOR_HANDLE(
      device_->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart(),
      page->descriptorIndex,
      device_->GetDescriptorSizeCBVSRVUAV());

   D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
   srvDesc.Format = DXGI_FORMAT_R8_UNORM;
   srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
   srvDesc.Texture2D.MipLevels = 1;
   device_->GetDevice()->CreateShaderResourceView(page->texture.Get(), &srvDesc, page->srvCpu);
   device_->IncrementSrvIndex();

   Page* resultPage = page.get();
   pages_.push_back(std::move(page));
   return resultPage;
}

bool GlyphAtlas::TryAllocate(Page& page, uint32_t width, uint32_t height, uint32_t& outputX, uint32_t& outputY) {
   const uint32_t paddedWidth = width + kGlyphPadding * 2u;
   const uint32_t paddedHeight = height + kGlyphPadding * 2u;

   uint32_t candidateX = page.cursorX;
   uint32_t candidateY = page.cursorY;
   uint32_t candidateRowHeight = page.rowHeight;
   if (candidateX + paddedWidth > pageSize_) {
      candidateX = 0;
      candidateY += candidateRowHeight;
      candidateRowHeight = 0;
   }
   if (candidateY + paddedHeight > pageSize_) {
      return false;
   }

   outputX = candidateX + kGlyphPadding;
   outputY = candidateY + kGlyphPadding;
   page.cursorX = candidateX + paddedWidth;
   page.cursorY = candidateY;
   page.rowHeight = (std::max)(candidateRowHeight, paddedHeight);
   return true;
}

bool GlyphAtlas::UploadPage(Page& page) {
   if (!device_ || !page.texture || !page.dirty) {
      return false;
   }

   const uint32_t rowPitch = AlignTexturePitch(pageSize_);
   const uint64_t uploadSize = static_cast<uint64_t>(rowPitch) * pageSize_;
   const CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
   const CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);

   Microsoft::WRL::ComPtr<ID3D12Resource> uploadResource;
   const HRESULT createResult = device_->GetDevice()->CreateCommittedResource(
      &uploadHeap,
      D3D12_HEAP_FLAG_NONE,
      &uploadDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(uploadResource.GetAddressOf()));
   if (FAILED(createResult)) {
      Logger::Error("[GlyphAtlas] Failed to create atlas upload buffer.");
      return false;
   }

   uint8_t* mapped = nullptr;
   if (FAILED(uploadResource->Map(0, nullptr, reinterpret_cast<void**>(&mapped))) || !mapped) {
      Logger::Error("[GlyphAtlas] Failed to map atlas upload buffer.");
      return false;
   }
   for (uint32_t row = 0; row < pageSize_; ++row) {
      std::memcpy(
         mapped + static_cast<size_t>(row) * rowPitch,
         page.pixels.data() + static_cast<size_t>(row) * pageSize_,
         pageSize_);
   }
   uploadResource->Unmap(0, nullptr);

   ID3D12GraphicsCommandList* commandList = device_->GetCommandList();
   if (page.uploaded) {
      const auto toCopy = CD3DX12_RESOURCE_BARRIER::Transition(
         page.texture.Get(),
         D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
         D3D12_RESOURCE_STATE_COPY_DEST);
      commandList->ResourceBarrier(1, &toCopy);
   }

   D3D12_TEXTURE_COPY_LOCATION destination{};
   destination.pResource = page.texture.Get();
   destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
   destination.SubresourceIndex = 0;

   D3D12_TEXTURE_COPY_LOCATION source{};
   source.pResource = uploadResource.Get();
   source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
   source.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8_UNORM;
   source.PlacedFootprint.Footprint.Width = pageSize_;
   source.PlacedFootprint.Footprint.Height = pageSize_;
   source.PlacedFootprint.Footprint.Depth = 1;
   source.PlacedFootprint.Footprint.RowPitch = rowPitch;

   commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

   const auto toShaderResource = CD3DX12_RESOURCE_BARRIER::Transition(
      page.texture.Get(),
      D3D12_RESOURCE_STATE_COPY_DEST,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
   commandList->ResourceBarrier(1, &toShaderResource);

   page.dirty = false;
   page.uploaded = true;
   intermediateResources_.push_back(std::move(uploadResource));
   return true;
}

} // namespace GameEngine
