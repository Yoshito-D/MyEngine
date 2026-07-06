#include "pch.h"
#include "PrimitiveMeshComponent.h"
#include "ComponentRegistry.h"
#include "Core/Graphics/Mesh.h"
#include "Graphics/Texture.h"
#include "Object.h"

#include <algorithm>
#include <string>

namespace {
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   GameEngine::PrimitiveMeshComponent::kTypeName,
   [](GameEngine::Object& o) -> GameEngine::IObjectComponent* { return o.AddComponent<GameEngine::PrimitiveMeshComponent>(); },
   GameEngine::PrimitiveMeshComponent::kDisplayName
);

const char* ToPrimitiveTypeName(GameEngine::PrimitiveMeshComponent::PrimitiveType primitiveType) {
   switch (primitiveType) {
      case GameEngine::PrimitiveMeshComponent::PrimitiveType::Quad:
      default:
         return "Quad";
   }
}

GameEngine::PrimitiveMeshComponent::PrimitiveType ParsePrimitiveType(
   const nlohmann::json& value,
   GameEngine::PrimitiveMeshComponent::PrimitiveType fallback
) {
   if (value.is_string()) {
      const std::string name = value.get<std::string>();
      if (name == "Quad") {
         return GameEngine::PrimitiveMeshComponent::PrimitiveType::Quad;
      }
   } else if (value.is_number_integer()) {
      if (value.get<int>() == static_cast<int>(GameEngine::PrimitiveMeshComponent::PrimitiveType::Quad)) {
         return GameEngine::PrimitiveMeshComponent::PrimitiveType::Quad;
      }
   }

   return fallback;
}

bool ReadVector2(const nlohmann::json& data, const char* key, GameEngine::Vector2& out) {
   if (!data.contains(key) || !data.at(key).is_array() || data.at(key).size() != 2) {
      return false;
   }

   out.x = data.at(key)[0].get<float>();
   out.y = data.at(key)[1].get<float>();
   return true;
}
}

#ifdef USE_IMGUI
#include "imgui.h"
#include "Utility/ImGuiHelper.h"
#endif

namespace GameEngine {

const char* PrimitiveMeshComponent::GetTypeName() const {
   return kTypeName;
}

nlohmann::json PrimitiveMeshComponent::Serialize() const {
   return nlohmann::json{
      { "primitiveType", ToPrimitiveTypeName(primitiveType_) },
      { "quadSize", { quadSize_.x, quadSize_.y } },
      { "quadAnchorPoint", { quadAnchorPoint_.x, quadAnchorPoint_.y } },
      { "flipX", flipX_ },
      { "flipY", flipY_ }
   };
}

void PrimitiveMeshComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }

   if (data.contains("primitiveType")) {
      primitiveType_ = ParsePrimitiveType(data.at("primitiveType"), primitiveType_);
   }

   Vector2 value = quadSize_;
   if (ReadVector2(data, "quadSize", value) || ReadVector2(data, "size", value)) {
      quadSize_ = value;
   }

   value = quadAnchorPoint_;
   if (ReadVector2(data, "quadAnchorPoint", value) || ReadVector2(data, "anchor", value)) {
      quadAnchorPoint_ = value;
   }

   if (data.contains("flipX") && data.at("flipX").is_boolean()) {
      flipX_ = data.at("flipX").get<bool>();
   }
   if (data.contains("flipY") && data.at("flipY").is_boolean()) {
      flipY_ = data.at("flipY").get<bool>();
   }

   ApplyToMesh();
}

#ifdef USE_IMGUI
void PrimitiveMeshComponent::DrawInspector() {
   auto Tr = [](const char* japanese, const char* english) {
      return ImGuiHelper::Localize({ japanese, english });
   };

   const std::string header = MakeObjectComponentHeaderLabel(GetTypeName());
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }

   const char* primitiveTypeItems[] = { "Quad" };
   int primitiveTypeIndex = 0;
   if (ImGui::Combo(Tr("プリミティブ", "Primitive"), &primitiveTypeIndex, primitiveTypeItems, 1)) {
      primitiveType_ = PrimitiveType::Quad;
   }

   ImGui::DragFloat2(Tr("サイズ", "Size"), &quadSize_.x, 0.1f, 0.0f, 4096.0f);
   ImGui::DragFloat2(Tr("アンカー", "Anchor"), &quadAnchorPoint_.x, 0.01f, -10.0f, 10.0f);
   ImGui::Checkbox(Tr("左右反転", "Flip X"), &flipX_);
   ImGui::Checkbox(Tr("上下反転", "Flip Y"), &flipY_);

   ImGui::Spacing();
}
#endif

void PrimitiveMeshComponent::CreateMesh() {
   if (primitiveType_ != PrimitiveType::Quad) {
      return;
   }

   mesh_ = std::make_unique<Mesh>();
   mesh_->CreateSprite(quadSize_.x, quadSize_.y);
   ApplyToMesh();
}

Mesh* PrimitiveMeshComponent::EnsureMesh() {
   if (!mesh_) {
      CreateMesh();
   }
   return mesh_.get();
}

void PrimitiveMeshComponent::ApplyToMesh() {
   ApplyToMesh(mesh_.get());
}

void PrimitiveMeshComponent::ApplyToMesh(Mesh* mesh) const {
   if (!mesh || primitiveType_ != PrimitiveType::Quad) {
      return;
   }

   auto* vertexData = mesh->GetVertexData();
   if (!vertexData) {
      return;
   }

   float left = 0.0f - quadAnchorPoint_.x * quadSize_.x;
   float right = quadSize_.x - quadAnchorPoint_.x * quadSize_.x;
   float top = quadSize_.y - quadAnchorPoint_.y * quadSize_.y;
   float bottom = 0.0f - quadAnchorPoint_.y * quadSize_.y;

   // Sprite由来のQuadは頂点順を固定しているため、反転は位置だけを入れ替えて既存のUV更新経路を保つ。
   if (flipX_) {
      left = -left;
      right = -right;
   }

   if (flipY_) {
      top = -top;
      bottom = -bottom;
   }

   vertexData[0].position = Vector4(left, bottom, 0.0f, 1.0f);
   vertexData[1].position = Vector4(left, top, 0.0f, 1.0f);
   vertexData[2].position = Vector4(right, bottom, 0.0f, 1.0f);
   vertexData[3].position = Vector4(right, top, 0.0f, 1.0f);
}

void PrimitiveMeshComponent::ApplyTextureCoordinates(Texture* texture, const Vector2& leftTop, const Vector2& size) {
   Mesh* mesh = EnsureMesh();
   if (!texture || !mesh || !mesh->GetVertexData()) {
      return;
   }

   const DirectX::TexMetadata& metadata = texture->GetMetadata();
   if (metadata.width == 0 || metadata.height == 0) {
      return;
   }

   Vector2 actualTextureLeftTop = leftTop;
   Vector2 actualTextureSize = size;

   // 矩形が未指定または実テクスチャを超える場合は、テクスチャ全体を使用する。
   if (actualTextureSize.x <= 0.0f || actualTextureSize.y <= 0.0f ||
      actualTextureSize.x > static_cast<float>(metadata.width) ||
      actualTextureSize.y > static_cast<float>(metadata.height)) {
      actualTextureSize.x = static_cast<float>(metadata.width);
      actualTextureSize.y = static_cast<float>(metadata.height);
      actualTextureLeftTop = { 0.0f, 0.0f };
   }

   const float texLeft = actualTextureLeftTop.x / static_cast<float>(metadata.width);
   const float texRight = (actualTextureLeftTop.x + actualTextureSize.x) / static_cast<float>(metadata.width);
   const float texTop = actualTextureLeftTop.y / static_cast<float>(metadata.height);
   const float texBottom = (actualTextureLeftTop.y + actualTextureSize.y) / static_cast<float>(metadata.height);

   auto* vertexData = mesh->GetVertexData();
   vertexData[0].texCoord = Vector2(texLeft, texBottom);
   vertexData[1].texCoord = Vector2(texLeft, texTop);
   vertexData[2].texCoord = Vector2(texRight, texBottom);
   vertexData[3].texCoord = Vector2(texRight, texTop);
}

}
