#include "pch.h"
#include "MsdfFont.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/ResourceHelper.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace GameEngine {
namespace {
uint64_t BuildGlyphCacheKey(uint32_t pixelSize, uint32_t codePoint) {
   return (static_cast<uint64_t>(pixelSize) << 32u) | codePoint;
}

uint64_t BuildKerningKey(uint32_t leftGlyph, uint32_t rightGlyph) {
   return (static_cast<uint64_t>(leftGlyph) << 32u) | rightGlyph;
}

bool ReadBounds(
   const nlohmann::json& value,
   float& left,
   float& bottom,
   float& right,
   float& top) {
   if (!value.is_object() ||
      !value.contains("left") || !value.at("left").is_number() ||
      !value.contains("bottom") || !value.at("bottom").is_number() ||
      !value.contains("right") || !value.at("right").is_number() ||
      !value.contains("top") || !value.at("top").is_number()) {
      return false;
   }

   left = value.at("left").get<float>();
   bottom = value.at("bottom").get<float>();
   right = value.at("right").get<float>();
   top = value.at("top").get<float>();
   return true;
}
}

MsdfFont::~MsdfFont() {
   Clear();
}

bool MsdfFont::Load(GraphicsDevice* device, const std::filesystem::path& jsonPath) {
   Clear();
   if (!device || !device->GetDevice() || !std::filesystem::is_regular_file(jsonPath)) {
      Logger::Error("[MsdfFont] Invalid load request: " + jsonPath.generic_string());
      return false;
   }

   nlohmann::json root;
   try {
      std::ifstream stream(jsonPath);
      if (!stream) {
         Logger::Error("[MsdfFont] Failed to open JSON: " + jsonPath.generic_string());
         return false;
      }
      stream >> root;
   } catch (const std::exception& exception) {
      Logger::Error("[MsdfFont] Failed to parse JSON: " + std::string(exception.what()));
      return false;
   }

   if (!root.contains("atlas") || !root.at("atlas").is_object() ||
      !root.contains("metrics") || !root.at("metrics").is_object() ||
      !root.contains("glyphs") || !root.at("glyphs").is_array()) {
      Logger::Error("[MsdfFont] Required atlas, metrics, or glyphs section is missing.");
      return false;
   }

   const auto& atlas = root.at("atlas");
   const std::string atlasTypeName = atlas.value("type", "");
   if (atlasTypeName != "msdf" && atlasTypeName != "mtsdf") {
      Logger::Error("[MsdfFont] Unsupported atlas type: " + atlasTypeName);
      return false;
   }

   const uint32_t atlasWidth = atlas.value("width", 0u);
   const uint32_t atlasHeight = atlas.value("height", 0u);
   const float distanceRange = atlas.value("distanceRange", 0.0f);
   if (atlasWidth == 0 || atlasHeight == 0 || distanceRange <= 0.0f) {
      Logger::Error("[MsdfFont] Invalid atlas dimensions or distance range.");
      return false;
   }

   std::unordered_map<uint32_t, SourceGlyph> sourceGlyphs;
   for (const auto& glyphData : root.at("glyphs")) {
      if (!glyphData.is_object() || !glyphData.contains("unicode") || !glyphData.at("unicode").is_number_unsigned()) {
         continue;
      }

      SourceGlyph glyph{};
      glyph.codePoint = glyphData.at("unicode").get<uint32_t>();
      glyph.advance = glyphData.value("advance", 0.0f);
      if (glyphData.contains("planeBounds") && glyphData.contains("atlasBounds")) {
         glyph.hasImage = ReadBounds(
            glyphData.at("planeBounds"),
            glyph.planeLeft,
            glyph.planeBottom,
            glyph.planeRight,
            glyph.planeTop) &&
            ReadBounds(
               glyphData.at("atlasBounds"),
               glyph.atlasLeft,
               glyph.atlasBottom,
               glyph.atlasRight,
               glyph.atlasTop);
      }
      sourceGlyphs[glyph.codePoint] = glyph;
   }
   if (sourceGlyphs.empty()) {
      Logger::Error("[MsdfFont] No Unicode glyphs were found.");
      return false;
   }

   std::unordered_map<uint64_t, float> kerningPairs;
   if (root.contains("kerning") && root.at("kerning").is_array()) {
      for (const auto& kerningData : root.at("kerning")) {
         if (!kerningData.is_object() ||
            !kerningData.contains("unicode1") || !kerningData.at("unicode1").is_number_unsigned() ||
            !kerningData.contains("unicode2") || !kerningData.at("unicode2").is_number_unsigned() ||
            !kerningData.contains("advance") || !kerningData.at("advance").is_number()) {
            continue;
         }
         kerningPairs[BuildKerningKey(
            kerningData.at("unicode1").get<uint32_t>(),
            kerningData.at("unicode2").get<uint32_t>())] = kerningData.at("advance").get<float>();
      }
   }

   std::filesystem::path imagePath = jsonPath;
   imagePath.replace_extension(".png");
   DirectX::ScratchImage image;
   const DirectX::WIC_FLAGS wicFlags = static_cast<DirectX::WIC_FLAGS>(
      DirectX::WIC_FLAGS_FORCE_RGB |
      DirectX::WIC_FLAGS_IGNORE_SRGB |
      DirectX::WIC_FLAGS_FORCE_LINEAR);
   HRESULT result = DirectX::LoadFromWICFile(
      imagePath.wstring().c_str(),
      wicFlags,
      nullptr,
      image);
   if (FAILED(result)) {
      Logger::Error("[MsdfFont] Failed to load atlas PNG: " + imagePath.generic_string());
      return false;
   }

   if (image.GetMetadata().format != DXGI_FORMAT_R8G8B8A8_UNORM) {
      DirectX::ScratchImage convertedImage;
      result = DirectX::Convert(
         image.GetImages(),
         image.GetImageCount(),
         image.GetMetadata(),
         DXGI_FORMAT_R8G8B8A8_UNORM,
         DirectX::TEX_FILTER_DEFAULT,
         DirectX::TEX_THRESHOLD_DEFAULT,
         convertedImage);
      if (FAILED(result)) {
         Logger::Error("[MsdfFont] Failed to convert atlas PNG to linear RGBA8.");
         return false;
      }
      image = std::move(convertedImage);
   }

   const auto& metadata = image.GetMetadata();
   if (metadata.width != atlasWidth || metadata.height != atlasHeight) {
      Logger::Error("[MsdfFont] JSON and PNG atlas dimensions do not match.");
      return false;
   }

   auto atlasTexture = ResourceHelper::CreateTextureResource(device->GetDevice(), metadata);
   if (!atlasTexture) {
      Logger::Error("[MsdfFont] Failed to create the atlas texture.");
      return false;
   }
   auto intermediateResource = ResourceHelper::UploadTextureData(
      atlasTexture.Get(),
      image,
      device->GetDevice(),
      device->GetCommandList());
   if (!intermediateResource) {
      Logger::Error("[MsdfFont] Failed to upload the atlas texture.");
      return false;
   }

   const UINT descriptorIndex = device->GetNextSrvIndex();
   const CD3DX12_CPU_DESCRIPTOR_HANDLE srvCpu(
      device->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart(),
      descriptorIndex,
      device->GetDescriptorSizeCBVSRVUAV());
   const CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpu(
      device->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart(),
      descriptorIndex,
      device->GetDescriptorSizeCBVSRVUAV());
   D3D12_SHADER_RESOURCE_VIEW_DESC srvDescription{};
   srvDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
   srvDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   srvDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
   srvDescription.Texture2D.MipLevels = 1;
   device->GetDevice()->CreateShaderResourceView(atlasTexture.Get(), &srvDescription, srvCpu);
   device->IncrementSrvIndex();

   const auto& metrics = root.at("metrics");
   device_ = device;
   atlasTexture_ = std::move(atlasTexture);
   intermediateResource_ = std::move(intermediateResource);
   atlasSrv_ = srvGpu;
   descriptorIndex_ = descriptorIndex;
   atlasWidth_ = atlasWidth;
   atlasHeight_ = atlasHeight;
   distanceRange_ = distanceRange;
   lineHeight_ = metrics.value("lineHeight", 0.0f);
   ascender_ = metrics.value("ascender", 0.0f);
   descender_ = metrics.value("descender", 0.0f);
   bottomOrigin_ = atlas.value("yOrigin", "bottom") != "top";
   atlasType_ = atlasTypeName == "mtsdf" ? TextAtlasType::Mtsdf : TextAtlasType::Msdf;
   sourceGlyphs_ = std::move(sourceGlyphs);
   kerningPairs_ = std::move(kerningPairs);
   Logger::Info("[MsdfFont] Loaded atlas: " + jsonPath.generic_string());
   return true;
}

const GlyphInfo* MsdfFont::GetGlyph(uint32_t pixelSize, char32_t codePoint) {
   if (pixelSize == 0 || atlasSrv_.ptr == 0) {
      return nullptr;
   }

   const SourceGlyph* source = FindSourceGlyph(codePoint);
   if (!source) {
      source = FindSourceGlyph(0xFFFD);
   }
   if (!source) {
      source = FindSourceGlyph(U'?');
   }
   if (!source) {
      return nullptr;
   }

   const uint64_t cacheKey = BuildGlyphCacheKey(pixelSize, source->codePoint);
   if (const auto iterator = scaledGlyphs_.find(cacheKey); iterator != scaledGlyphs_.end()) {
      return &iterator->second;
   }

   const float scale = static_cast<float>(pixelSize);
   GlyphInfo glyph{};
   glyph.advance = source->advance * scale;
   glyph.glyphIndex = source->codePoint;
   glyph.atlasSrv = atlasSrv_;
   glyph.atlasSize = { static_cast<float>(atlasWidth_), static_cast<float>(atlasHeight_) };
   glyph.distanceRange = distanceRange_;
   glyph.atlasType = atlasType_;

   if (source->hasImage) {
      glyph.bitmapSize = {
         (source->planeRight - source->planeLeft) * scale,
         (source->planeTop - source->planeBottom) * scale
      };
      glyph.bearing = {
         source->planeLeft * scale,
         source->planeTop * scale
      };
      glyph.uvMin.x = source->atlasLeft / static_cast<float>(atlasWidth_);
      glyph.uvMax.x = source->atlasRight / static_cast<float>(atlasWidth_);
      if (bottomOrigin_) {
         glyph.uvMin.y = (static_cast<float>(atlasHeight_) - source->atlasTop) / static_cast<float>(atlasHeight_);
         glyph.uvMax.y = (static_cast<float>(atlasHeight_) - source->atlasBottom) / static_cast<float>(atlasHeight_);
      } else {
         glyph.uvMin.y = source->atlasTop / static_cast<float>(atlasHeight_);
         glyph.uvMax.y = source->atlasBottom / static_cast<float>(atlasHeight_);
      }
   }

   const auto [iterator, inserted] = scaledGlyphs_.emplace(cacheKey, glyph);
   (void)inserted;
   return &iterator->second;
}

FontMetrics MsdfFont::GetMetrics(uint32_t pixelSize) const {
   const float scale = static_cast<float>(pixelSize);
   return {
      ascender_ * scale,
      descender_ * scale,
      lineHeight_ * scale
   };
}

float MsdfFont::GetKerning(uint32_t leftGlyph, uint32_t rightGlyph, uint32_t pixelSize) const {
   if (leftGlyph == 0 || rightGlyph == 0 || pixelSize == 0) {
      return 0.0f;
   }
   const auto iterator = kerningPairs_.find(BuildKerningKey(leftGlyph, rightGlyph));
   return iterator != kerningPairs_.end() ? iterator->second * static_cast<float>(pixelSize) : 0.0f;
}

void MsdfFont::ReleaseIntermediateResources() {
   intermediateResource_.Reset();
}

void MsdfFont::Clear() {
   scaledGlyphs_.clear();
   sourceGlyphs_.clear();
   kerningPairs_.clear();
   intermediateResource_.Reset();
   atlasTexture_.Reset();
   if (device_ && descriptorIndex_ != UINT_MAX) {
      device_->ReleaseSrvIndex(descriptorIndex_);
   }
   device_ = nullptr;
   descriptorIndex_ = UINT_MAX;
   atlasSrv_ = {};
   atlasWidth_ = 0;
   atlasHeight_ = 0;
}

const MsdfFont::SourceGlyph* MsdfFont::FindSourceGlyph(char32_t codePoint) const {
   const auto iterator = sourceGlyphs_.find(static_cast<uint32_t>(codePoint));
   return iterator != sourceGlyphs_.end() ? &iterator->second : nullptr;
}

} // namespace GameEngine
