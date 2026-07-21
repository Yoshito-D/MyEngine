#include "pch.h"
#include "Component/MeshComponent.h"
#include "Component/ComponentRegistry.h"
#include "Core/Graphics/Mesh.h"
#include "Framework/EngineContext.h"
#include "Graphics/Texture.h"
#include "Object.h"

#include <algorithm>
#include <string>

namespace {
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   GameEngine::MeshComponent::kTypeName,
   [](GameEngine::Object& o) -> GameEngine::IObjectComponent* { return o.AddComponent<GameEngine::MeshComponent>(); },
   GameEngine::MeshComponent::kDisplayName,
   GameEngine::ObjectType::Model | GameEngine::ObjectType::Sprite
);

const char* ToSourceTypeName(GameEngine::MeshComponent::SourceType sourceType) {
   return sourceType == GameEngine::MeshComponent::SourceType::Primitive ? "Primitive" : "ModelFile";
}

GameEngine::MeshComponent::SourceType ParseSourceType(
   const nlohmann::json& value,
   GameEngine::MeshComponent::SourceType fallback
) {
   if (value.is_string()) {
      const std::string name = value.get<std::string>();
      if (name == "ModelFile") {
         return GameEngine::MeshComponent::SourceType::ModelFile;
      }
      if (name == "Primitive") {
         return GameEngine::MeshComponent::SourceType::Primitive;
      }
   }
   return fallback;
}

const char* ToPrimitiveTypeName(GameEngine::MeshComponent::PrimitiveType primitiveType) {
   switch (primitiveType) {
      case GameEngine::MeshComponent::PrimitiveType::Ring: return "Ring";
      case GameEngine::MeshComponent::PrimitiveType::Sphere: return "Sphere";
      case GameEngine::MeshComponent::PrimitiveType::Box: return "Box";
      case GameEngine::MeshComponent::PrimitiveType::Cylinder: return "Cylinder";
      case GameEngine::MeshComponent::PrimitiveType::Cone: return "Cone";
      case GameEngine::MeshComponent::PrimitiveType::Circle: return "Circle";
      case GameEngine::MeshComponent::PrimitiveType::Plane: return "Plane";
      case GameEngine::MeshComponent::PrimitiveType::Torus: return "Torus";
      case GameEngine::MeshComponent::PrimitiveType::Triangle: return "Triangle";
      case GameEngine::MeshComponent::PrimitiveType::Quad:
      default: return "Quad";
   }
}

GameEngine::MeshComponent::PrimitiveType ParsePrimitiveType(
   const nlohmann::json& value,
   GameEngine::MeshComponent::PrimitiveType fallback
) {
   if (value.is_string()) {
      const std::string name = value.get<std::string>();
      if (name == "Quad") return GameEngine::MeshComponent::PrimitiveType::Quad;
      if (name == "Ring") return GameEngine::MeshComponent::PrimitiveType::Ring;
      if (name == "Sphere") return GameEngine::MeshComponent::PrimitiveType::Sphere;
      if (name == "Box") return GameEngine::MeshComponent::PrimitiveType::Box;
      if (name == "Cylinder") return GameEngine::MeshComponent::PrimitiveType::Cylinder;
      if (name == "Cone") return GameEngine::MeshComponent::PrimitiveType::Cone;
      if (name == "Circle") return GameEngine::MeshComponent::PrimitiveType::Circle;
      if (name == "Plane") return GameEngine::MeshComponent::PrimitiveType::Plane;
      if (name == "Torus") return GameEngine::MeshComponent::PrimitiveType::Torus;
      if (name == "Triangle") return GameEngine::MeshComponent::PrimitiveType::Triangle;
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

bool ReadVector3(const nlohmann::json& data, const char* key, GameEngine::Vector3& out) {
   if (!data.contains(key) || !data.at(key).is_array() || data.at(key).size() != 3) {
      return false;
   }

   out.x = data.at(key)[0].get<float>();
   out.y = data.at(key)[1].get<float>();
   out.z = data.at(key)[2].get<float>();
   return true;
}

const char* ToPlaneOrientationName(GameEngine::MeshComponent::PlaneOrientation orientation) {
   switch (orientation) {
      case GameEngine::MeshComponent::PlaneOrientation::XZ: return "XZ";
      case GameEngine::MeshComponent::PlaneOrientation::YZ: return "YZ";
      case GameEngine::MeshComponent::PlaneOrientation::XY:
      default: return "XY";
   }
}

GameEngine::MeshComponent::PlaneOrientation ParsePlaneOrientation(
   const nlohmann::json& value,
   GameEngine::MeshComponent::PlaneOrientation fallback
) {
   if (value.is_string()) {
      const std::string name = value.get<std::string>();
      if (name == "XY") return GameEngine::MeshComponent::PlaneOrientation::XY;
      if (name == "XZ") return GameEngine::MeshComponent::PlaneOrientation::XZ;
      if (name == "YZ") return GameEngine::MeshComponent::PlaneOrientation::YZ;
   }
   return fallback;
}

GameEngine::Mesh::PlaneOrientation ToMeshPlaneOrientation(
   GameEngine::MeshComponent::PlaneOrientation orientation
) {
   switch (orientation) {
      case GameEngine::MeshComponent::PlaneOrientation::XZ:
         return GameEngine::Mesh::PlaneOrientation::XZ;
      case GameEngine::MeshComponent::PlaneOrientation::YZ:
         return GameEngine::Mesh::PlaneOrientation::YZ;
      case GameEngine::MeshComponent::PlaneOrientation::XY:
      default:
         return GameEngine::Mesh::PlaneOrientation::XY;
   }
}
}

#ifdef USE_IMGUI
#include "imgui.h"
#include "Scene/BaseScene.h"
#include "Utility/ImGuiHelper.h"
#endif

namespace GameEngine {

const char* MeshComponent::GetTypeName() const {
   return kTypeName;
}

void MeshComponent::SetSourceType(SourceType sourceType) {
   if (sourceType_ == sourceType) {
      return;
   }
   sourceType_ = sourceType;
   if (sourceType_ == SourceType::ModelFile) {
      mesh_.reset();
   }
}

void MeshComponent::SetModelAsset(const std::shared_ptr<ModelAsset>& modelAsset) {
   modelAsset_ = modelAsset;
   assetId_ = modelAsset_ ? modelAsset_->GetAssetId() : std::string{};
   sourceType_ = SourceType::ModelFile;
   mesh_.reset();

   skinCluster_.reset();
   if (modelAsset_ && modelAsset_->HasSkinningData()) {
      skinCluster_ = modelAsset_->CreateSkinClusterInstance();
   }
}

bool MeshComponent::SetModelAssetByAssetId(const std::string& assetId) {
   if (assetId.empty()) {
      SetModelAsset(nullptr);
      return true;
   }

   auto modelAsset = EngineContext::LoadModelByAssetId(assetId);
   if (!modelAsset) {
      return false;
   }

   SetModelAsset(modelAsset);
   assetId_ = assetId;
   return true;
}

ModelAsset* MeshComponent::GetModelAsset() const {
   return sourceType_ == SourceType::ModelFile ? modelAsset_.get() : nullptr;
}

SkinCluster* MeshComponent::GetSkinCluster() {
   if (sourceType_ != SourceType::ModelFile) {
      return nullptr;
   }
   if (skinCluster_) {
      return &(*skinCluster_);
   }
   return modelAsset_ ? modelAsset_->GetSkinCluster() : nullptr;
}

const SkinCluster* MeshComponent::GetSkinCluster() const {
   if (sourceType_ != SourceType::ModelFile) {
      return nullptr;
   }
   if (skinCluster_) {
      return &(*skinCluster_);
   }
   return modelAsset_ ? modelAsset_->GetSkinCluster() : nullptr;
}

nlohmann::json MeshComponent::Serialize() const {
   return nlohmann::json{
      { "sourceType", ToSourceTypeName(sourceType_) },
      { "reverseFaces", reverseFaces_ },
      { "assetId", sourceType_ == SourceType::ModelFile ? assetId_ : std::string{} },
      { "primitiveType", ToPrimitiveTypeName(primitiveType_) },
      { "planeOrientation", ToPlaneOrientationName(planeOrientation_) },
      { "quadSize", { quadSize_.x, quadSize_.y } },
      { "quadAnchorPoint", { quadAnchorPoint_.x, quadAnchorPoint_.y } },
      { "flipX", flipX_ },
      { "flipY", flipY_ },
      { "originY", originY_ },
      { "ringInnerRadius", ringInnerRadius_ },
      { "ringOuterRadius", ringOuterRadius_ },
      { "ringSegments", ringSegments_ },
      { "sphereRadius", sphereRadius_ },
      { "sphereStacks", sphereStacks_ },
      { "sphereSlices", sphereSlices_ },
      { "boxSize", { boxSize_.x, boxSize_.y, boxSize_.z } },
      { "cylinderTopRadius", cylinderTopRadius_ },
      { "cylinderBottomRadius", cylinderBottomRadius_ },
      { "cylinderHeight", cylinderHeight_ },
      { "cylinderSegments", cylinderSegments_ },
      { "coneRadius", coneRadius_ },
      { "coneHeight", coneHeight_ },
      { "coneSegments", coneSegments_ },
      { "circleRadius", circleRadius_ },
      { "circleSegments", circleSegments_ },
      { "planeWidth", planeWidth_ },
      { "planeDepth", planeDepth_ },
      { "planeWidthSegments", planeWidthSegments_ },
      { "planeDepthSegments", planeDepthSegments_ },
      { "torusMajorRadius", torusMajorRadius_ },
      { "torusMinorRadius", torusMinorRadius_ },
      { "torusMajorSegments", torusMajorSegments_ },
      { "torusMinorSegments", torusMinorSegments_ },
      { "triangleV0", { triangleV0_.x, triangleV0_.y, triangleV0_.z } },
      { "triangleV1", { triangleV1_.x, triangleV1_.y, triangleV1_.z } },
      { "triangleV2", { triangleV2_.x, triangleV2_.y, triangleV2_.z } }
   };
}

void MeshComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }

   if (data.contains("sourceType")) {
      sourceType_ = ParseSourceType(data.at("sourceType"), sourceType_);
   }
   if (data.contains("reverseFaces") && data.at("reverseFaces").is_boolean()) {
      reverseFaces_ = data.at("reverseFaces").get<bool>();
   }

   if (sourceType_ == SourceType::ModelFile && data.contains("assetId") && data.at("assetId").is_string()) {
      const std::string assetId = data.at("assetId").get<std::string>();
      if (!assetId.empty()) {
         SetModelAssetByAssetId(assetId);
      }
   }

   if (data.contains("primitiveType")) {
      primitiveType_ = ParsePrimitiveType(data.at("primitiveType"), primitiveType_);
   }
   if (data.contains("planeOrientation")) {
      planeOrientation_ = ParsePlaneOrientation(data.at("planeOrientation"), planeOrientation_);
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

   auto readFloat = [&data](const char* key, float& destination, float minimum) {
      if (data.contains(key) && data.at(key).is_number()) {
         destination = (std::max)(data.at(key).get<float>(), minimum);
      }
   };
   auto readSegments = [&data](const char* key, uint32_t& destination, uint32_t minimum) {
      if (data.contains(key) && data.at(key).is_number_unsigned()) {
         destination = (std::max)(data.at(key).get<uint32_t>(), minimum);
      }
   };

   if (data.contains("originY") && data.at("originY").is_number()) {
      originY_ = std::clamp(data.at("originY").get<float>(), 0.0f, 1.0f);
   }
   readFloat("ringInnerRadius", ringInnerRadius_, 0.0f);
   readFloat("ringOuterRadius", ringOuterRadius_, ringInnerRadius_);
   readSegments("ringSegments", ringSegments_, 3);
   readFloat("sphereRadius", sphereRadius_, 0.001f);
   readSegments("sphereStacks", sphereStacks_, 2);
   readSegments("sphereSlices", sphereSlices_, 3);
   if (ReadVector3(data, "boxSize", boxSize_)) {
      boxSize_.x = (std::max)(boxSize_.x, 0.001f);
      boxSize_.y = (std::max)(boxSize_.y, 0.001f);
      boxSize_.z = (std::max)(boxSize_.z, 0.001f);
   }
   readFloat("cylinderTopRadius", cylinderTopRadius_, 0.0f);
   readFloat("cylinderBottomRadius", cylinderBottomRadius_, 0.0f);
   readFloat("cylinderHeight", cylinderHeight_, 0.001f);
   readSegments("cylinderSegments", cylinderSegments_, 3);
   readFloat("coneRadius", coneRadius_, 0.001f);
   readFloat("coneHeight", coneHeight_, 0.001f);
   readSegments("coneSegments", coneSegments_, 3);
   readFloat("circleRadius", circleRadius_, 0.001f);
   readSegments("circleSegments", circleSegments_, 3);
   readFloat("planeWidth", planeWidth_, 0.001f);
   readFloat("planeDepth", planeDepth_, 0.001f);
   readSegments("planeWidthSegments", planeWidthSegments_, 1);
   readSegments("planeDepthSegments", planeDepthSegments_, 1);
   readFloat("torusMajorRadius", torusMajorRadius_, 0.001f);
   readFloat("torusMinorRadius", torusMinorRadius_, 0.001f);
   readSegments("torusMajorSegments", torusMajorSegments_, 3);
   readSegments("torusMinorSegments", torusMinorSegments_, 3);
   ReadVector3(data, "triangleV0", triangleV0_);
   ReadVector3(data, "triangleV1", triangleV1_);
   ReadVector3(data, "triangleV2", triangleV2_);

   if (mesh_ && sourceType_ == SourceType::Primitive) {
      CreateMesh();
   } else if (sourceType_ == SourceType::ModelFile) {
      mesh_.reset();
   }
}

#ifdef USE_IMGUI
void MeshComponent::DrawInspector() {
   auto Tr = [](const char* japanese, const char* english) {
      return ImGuiHelper::Localize({ japanese, english });
   };

   const std::string header = MakeObjectComponentHeaderLabel(GetTypeName());
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }

   const char* sourceTypeItems[] = {
      Tr("モデルファイル", "Model File"),
      Tr("プリミティブ", "Primitive")
   };
   int sourceTypeIndex = static_cast<int>(sourceType_);
   if (ImGui::Combo(Tr("メッシュ種別", "Mesh Source"), &sourceTypeIndex, sourceTypeItems, 2)) {
      SetSourceType(static_cast<SourceType>(sourceTypeIndex));
   }
   ImGui::Checkbox(Tr("表裏を反転", "Reverse Faces"), &reverseFaces_);

   if (sourceType_ == SourceType::ModelFile) {
      ImGui::Text("%s: %s",
         Tr("アセットID", "Asset ID"),
         assetId_.empty() ? Tr("なし", "None") : assetId_.c_str());
      ImGui::Text("%s: %s",
         Tr("状態", "Status"),
         modelAsset_ ? Tr("読み込み済み", "Loaded") : Tr("未読み込み", "Not loaded"));
      if (modelAsset_) {
         ImGui::Text("%s: %s",
            Tr("スキニング", "Skinning"),
            modelAsset_->HasSkinningData() ? Tr("あり", "Available") : Tr("なし", "None"));
      }

      const std::string dropLabel = assetId_.empty()
         ? Tr("モデルアセットをここへドロップ", "Drop Model Asset Here")
         : assetId_;
      ImGui::Button((dropLabel + "##MeshModelAssetDrop").c_str(), ImVec2(-1.0f, 0.0f));
      if (ImGui::BeginDragDropTarget()) {
         if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_ASSET_MODEL")) {
            const char* assetId = static_cast<const char*>(payload->Data);
            if (assetId && payload->DataSize > 1) {
               if (auto* currentScene = BaseScene::GetCurrentScene()) {
                  if (auto* editorContext = currentScene->GetEditorSceneContext()) {
                     editorContext->SetModelAsset(&GetOwner(), assetId);
                  } else {
                     SetModelAssetByAssetId(assetId);
                  }
               } else {
                  SetModelAssetByAssetId(assetId);
               }
            }
         }
         ImGui::EndDragDropTarget();
      }
      ImGui::Spacing();
      return;
   }

   bool meshChanged = false;
   const char* primitiveTypeItems[] = {
      "Quad", "Ring", "Sphere", "Box", "Cylinder",
      "Cone", "Circle", "Plane", "Torus", "Triangle"
   };
   int primitiveTypeIndex = static_cast<int>(primitiveType_);
   if (ImGui::Combo(Tr("プリミティブ", "Primitive"), &primitiveTypeIndex, primitiveTypeItems, 10)) {
      primitiveType_ = static_cast<PrimitiveType>(primitiveTypeIndex);
      meshChanged = true;
   }

   const bool supportsPlaneOrientation = primitiveType_ == PrimitiveType::Ring ||
      primitiveType_ == PrimitiveType::Circle ||
      primitiveType_ == PrimitiveType::Plane ||
      primitiveType_ == PrimitiveType::Triangle;
   if (supportsPlaneOrientation) {
      const char* orientationItems[] = { "XY", "XZ", "YZ" };
      int orientationIndex = static_cast<int>(planeOrientation_);
      if (ImGui::Combo(Tr("生成平面", "Plane Orientation"), &orientationIndex, orientationItems, 3)) {
         planeOrientation_ = static_cast<PlaneOrientation>(orientationIndex);
         meshChanged = true;
      }
   }

   const bool supportsOriginY = primitiveType_ == PrimitiveType::Sphere ||
      primitiveType_ == PrimitiveType::Box ||
      primitiveType_ == PrimitiveType::Cylinder ||
      primitiveType_ == PrimitiveType::Cone ||
      primitiveType_ == PrimitiveType::Torus;
   if (supportsOriginY) {
      meshChanged |= ImGui::SliderFloat(Tr("原点Y", "Origin Y"), &originY_, 0.0f, 1.0f, "%.2f");
   }

   switch (primitiveType_) {
      case PrimitiveType::Quad:
         meshChanged |= ImGui::DragFloat2(Tr("サイズ", "Size"), &quadSize_.x, 0.1f, 0.001f, 4096.0f);
         meshChanged |= ImGui::DragFloat2(Tr("アンカー", "Anchor"), &quadAnchorPoint_.x, 0.01f, -10.0f, 10.0f);
         meshChanged |= ImGui::Checkbox(Tr("左右反転", "Flip X"), &flipX_);
         meshChanged |= ImGui::Checkbox(Tr("上下反転", "Flip Y"), &flipY_);
         break;
      case PrimitiveType::Ring: {
         meshChanged |= ImGui::DragFloat(Tr("内半径", "Inner Radius"), &ringInnerRadius_, 0.01f, 0.0f, 1000.0f);
         meshChanged |= ImGui::DragFloat(Tr("外半径", "Outer Radius"), &ringOuterRadius_, 0.01f, 0.001f, 1000.0f);
         int segments = static_cast<int>(ringSegments_);
         if (ImGui::DragInt(Tr("分割数", "Segments"), &segments, 1.0f, 3, 512)) {
            ringSegments_ = static_cast<uint32_t>((std::max)(segments, 3));
            meshChanged = true;
         }
         break;
      }
      case PrimitiveType::Sphere: {
         meshChanged |= ImGui::DragFloat(Tr("半径", "Radius"), &sphereRadius_, 0.01f, 0.001f, 1000.0f);
         int stacks = static_cast<int>(sphereStacks_);
         int slices = static_cast<int>(sphereSlices_);
         if (ImGui::DragInt(Tr("スタック", "Stacks"), &stacks, 1.0f, 2, 256)) {
            sphereStacks_ = static_cast<uint32_t>((std::max)(stacks, 2));
            meshChanged = true;
         }
         if (ImGui::DragInt(Tr("スライス", "Slices"), &slices, 1.0f, 3, 512)) {
            sphereSlices_ = static_cast<uint32_t>((std::max)(slices, 3));
            meshChanged = true;
         }
         break;
      }
      case PrimitiveType::Box:
         meshChanged |= ImGui::DragFloat3(Tr("サイズ", "Size"), &boxSize_.x, 0.01f, 0.001f, 1000.0f);
         break;
      case PrimitiveType::Cylinder: {
         meshChanged |= ImGui::DragFloat(Tr("上半径", "Top Radius"), &cylinderTopRadius_, 0.01f, 0.0f, 1000.0f);
         meshChanged |= ImGui::DragFloat(Tr("下半径", "Bottom Radius"), &cylinderBottomRadius_, 0.01f, 0.0f, 1000.0f);
         meshChanged |= ImGui::DragFloat(Tr("高さ", "Height"), &cylinderHeight_, 0.01f, 0.001f, 1000.0f);
         int segments = static_cast<int>(cylinderSegments_);
         if (ImGui::DragInt(Tr("分割数", "Segments"), &segments, 1.0f, 3, 512)) {
            cylinderSegments_ = static_cast<uint32_t>((std::max)(segments, 3));
            meshChanged = true;
         }
         break;
      }
      case PrimitiveType::Cone: {
         meshChanged |= ImGui::DragFloat(Tr("半径", "Radius"), &coneRadius_, 0.01f, 0.001f, 1000.0f);
         meshChanged |= ImGui::DragFloat(Tr("高さ", "Height"), &coneHeight_, 0.01f, 0.001f, 1000.0f);
         int segments = static_cast<int>(coneSegments_);
         if (ImGui::DragInt(Tr("分割数", "Segments"), &segments, 1.0f, 3, 512)) {
            coneSegments_ = static_cast<uint32_t>((std::max)(segments, 3));
            meshChanged = true;
         }
         break;
      }
      case PrimitiveType::Circle: {
         meshChanged |= ImGui::DragFloat(Tr("半径", "Radius"), &circleRadius_, 0.01f, 0.001f, 1000.0f);
         int segments = static_cast<int>(circleSegments_);
         if (ImGui::DragInt(Tr("分割数", "Segments"), &segments, 1.0f, 3, 512)) {
            circleSegments_ = static_cast<uint32_t>((std::max)(segments, 3));
            meshChanged = true;
         }
         break;
      }
      case PrimitiveType::Plane: {
         meshChanged |= ImGui::DragFloat(Tr("幅", "Width"), &planeWidth_, 0.01f, 0.001f, 1000.0f);
         meshChanged |= ImGui::DragFloat(Tr("奥行き", "Depth"), &planeDepth_, 0.01f, 0.001f, 1000.0f);
         int widthSegments = static_cast<int>(planeWidthSegments_);
         int depthSegments = static_cast<int>(planeDepthSegments_);
         if (ImGui::DragInt(Tr("幅の分割数", "Width Segments"), &widthSegments, 1.0f, 1, 512)) {
            planeWidthSegments_ = static_cast<uint32_t>((std::max)(widthSegments, 1));
            meshChanged = true;
         }
         if (ImGui::DragInt(Tr("奥行きの分割数", "Depth Segments"), &depthSegments, 1.0f, 1, 512)) {
            planeDepthSegments_ = static_cast<uint32_t>((std::max)(depthSegments, 1));
            meshChanged = true;
         }
         break;
      }
      case PrimitiveType::Torus: {
         meshChanged |= ImGui::DragFloat(Tr("主半径", "Major Radius"), &torusMajorRadius_, 0.01f, 0.001f, 1000.0f);
         meshChanged |= ImGui::DragFloat(Tr("副半径", "Minor Radius"), &torusMinorRadius_, 0.01f, 0.001f, 1000.0f);
         int majorSegments = static_cast<int>(torusMajorSegments_);
         int minorSegments = static_cast<int>(torusMinorSegments_);
         if (ImGui::DragInt(Tr("主分割数", "Major Segments"), &majorSegments, 1.0f, 3, 512)) {
            torusMajorSegments_ = static_cast<uint32_t>((std::max)(majorSegments, 3));
            meshChanged = true;
         }
         if (ImGui::DragInt(Tr("副分割数", "Minor Segments"), &minorSegments, 1.0f, 3, 512)) {
            torusMinorSegments_ = static_cast<uint32_t>((std::max)(minorSegments, 3));
            meshChanged = true;
         }
         break;
      }
      case PrimitiveType::Triangle:
         meshChanged |= ImGui::DragFloat3("V0", &triangleV0_.x, 0.01f);
         meshChanged |= ImGui::DragFloat3("V1", &triangleV1_.x, 0.01f);
         meshChanged |= ImGui::DragFloat3("V2", &triangleV2_.x, 0.01f);
         break;
   }

   if (meshChanged) {
      ringInnerRadius_ = (std::max)(ringInnerRadius_, 0.0f);
      ringOuterRadius_ = (std::max)(ringOuterRadius_, ringInnerRadius_ + 0.001f);
      boxSize_.x = (std::max)(boxSize_.x, 0.001f);
      boxSize_.y = (std::max)(boxSize_.y, 0.001f);
      boxSize_.z = (std::max)(boxSize_.z, 0.001f);
      CreateMesh();
   }

   ImGui::Spacing();
}
#endif

void MeshComponent::CreateMesh() {
   if (sourceType_ != SourceType::Primitive) {
      mesh_.reset();
      return;
   }
   mesh_ = std::make_unique<Mesh>();
   const Mesh::PlaneOrientation meshOrientation = ToMeshPlaneOrientation(planeOrientation_);
   switch (primitiveType_) {
      case PrimitiveType::Quad:
         mesh_->CreateSprite(quadSize_.x, quadSize_.y);
         ApplyToMesh();
         break;
      case PrimitiveType::Ring:
         mesh_->CreateRing(ringInnerRadius_, ringOuterRadius_, ringSegments_, meshOrientation);
         break;
      case PrimitiveType::Sphere:
         mesh_->CreateSphere(sphereRadius_, sphereStacks_, sphereSlices_, originY_);
         break;
      case PrimitiveType::Box:
         mesh_->CreateBox(boxSize_.x, boxSize_.y, boxSize_.z, originY_);
         break;
      case PrimitiveType::Cylinder:
         mesh_->CreateCylinder(
            cylinderTopRadius_, cylinderBottomRadius_, cylinderHeight_, cylinderSegments_, originY_);
         break;
      case PrimitiveType::Cone:
         mesh_->CreateCone(coneRadius_, coneHeight_, coneSegments_, originY_);
         break;
      case PrimitiveType::Circle:
         mesh_->CreateCircle(circleRadius_, circleSegments_, meshOrientation);
         break;
      case PrimitiveType::Plane:
         mesh_->CreatePlane(
            planeWidth_, planeDepth_, planeWidthSegments_, planeDepthSegments_, meshOrientation);
         break;
      case PrimitiveType::Torus:
         mesh_->CreateTorus(
            torusMajorRadius_, torusMinorRadius_, torusMajorSegments_, torusMinorSegments_, originY_);
         break;
      case PrimitiveType::Triangle:
         mesh_->CreateTriangle(triangleV0_, triangleV1_, triangleV2_, meshOrientation);
         break;
   }
}

Mesh* MeshComponent::EnsureMesh() {
   if (sourceType_ != SourceType::Primitive) {
      return nullptr;
   }
   if (!mesh_) {
      CreateMesh();
   }
   return mesh_.get();
}

void MeshComponent::ApplyToMesh() {
   ApplyToMesh(mesh_.get());
}

void MeshComponent::ApplyToMesh(Mesh* mesh) const {
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

void MeshComponent::ApplyTextureCoordinates(Texture* texture, const Vector2& leftTop, const Vector2& size) {
   // 非Quad形状はMesh生成時のUVを使う。Quad専用の4頂点更新を適用すると三角形などで範囲外になる。
   if (sourceType_ != SourceType::Primitive || primitiveType_ != PrimitiveType::Quad) {
      return;
   }

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
