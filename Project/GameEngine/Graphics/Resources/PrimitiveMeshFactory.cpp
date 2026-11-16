#include "GameEngine/pch.h"
#include "GameEngine/Graphics/Resources/PrimitiveMeshFactory.h"
#include "GameEngine/Graphics/Resources/Mesh.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiHelper.h"
#include "imgui.h"
#endif

namespace GameEngine {
namespace {
GameEngine::Mesh::PlaneOrientation ToMeshPlaneOrientation(GameEngine::MeshComponent::PlaneOrientation orientation) {
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

#ifdef USE_IMGUI
const char* Tr(const char* japanese, const char* english) {
   return ImGuiHelper::Localize({japanese, english});
}
#endif
} // namespace

std::span<const PrimitiveMeshDefinition> PrimitiveMeshFactory::GetDefinitions() {
   // 名前・生成処理・編集項目を同じ登録へ置き、形状追加時の分岐の更新漏れを防ぐ。
   static constexpr PrimitiveMeshDefinition definitions[] = {
      {
         .type = MeshComponent::PrimitiveType::Quad,
         .name = "Quad",
         .supportsPlaneOrientation = false,
         .supportsOriginY = false,
         .build =
            [](Mesh& mesh, const MeshComponent& component) {
               mesh.CreateSprite(component.quadSize_.x, component.quadSize_.y);

               // Sprite生成後にAnchor/Flipを頂点位置へ反映する必要があるのはQuadだけである。
               component.ApplyToMesh(&mesh);
            },
#ifdef USE_IMGUI
         .drawInspector =
            [](MeshComponent& component) {
               bool changed = false;
               changed |= ImGui::DragFloat2(Tr("サイズ", "Size"), &component.quadSize_.x, 0.1f, 0.001f, 4096.0f);
               changed |=
                  ImGui::DragFloat2(Tr("アンカー", "Anchor"), &component.quadAnchorPoint_.x, 0.01f, -10.0f, 10.0f);
               changed |= ImGui::Checkbox(Tr("左右反転", "Flip X"), &component.flipX_);
               changed |= ImGui::Checkbox(Tr("上下反転", "Flip Y"), &component.flipY_);
               return changed;
            },
#endif
      },
      {
         .type = MeshComponent::PrimitiveType::Ring,
         .name = "Ring",
         .supportsPlaneOrientation = true,
         .supportsOriginY = false,
         .build =
            [](Mesh& mesh, const MeshComponent& component) {
               mesh.CreateRing(component.ringInnerRadius_, component.ringOuterRadius_, component.ringSegments_,
                  ToMeshPlaneOrientation(component.planeOrientation_));
            },
#ifdef USE_IMGUI
         .drawInspector =
            [](MeshComponent& component) {
               bool changed = false;
               changed |=
                  ImGui::DragFloat(Tr("内半径", "Inner Radius"), &component.ringInnerRadius_, 0.01f, 0.0f, 1000.0f);
               changed |=
                  ImGui::DragFloat(Tr("外半径", "Outer Radius"), &component.ringOuterRadius_, 0.01f, 0.001f, 1000.0f);
               std::int32_t segments = static_cast<std::int32_t>(component.ringSegments_);

               if (ImGui::DragInt(Tr("分割数", "Segments"), &segments, 1.0f, 3, 512)) {
                  component.ringSegments_ = static_cast<uint32_t>(std::max(segments, 3));
                  changed = true;
               }

               return changed;
            },
#endif
      },
      {
         .type = MeshComponent::PrimitiveType::Sphere,
         .name = "Sphere",
         .supportsPlaneOrientation = false,
         .supportsOriginY = true,
         .build =
            [](Mesh& mesh, const MeshComponent& component) {
               mesh.CreateSphere(
                  component.sphereRadius_, component.sphereStacks_, component.sphereSlices_, component.originY_);
            },
#ifdef USE_IMGUI
         .drawInspector =
            [](MeshComponent& component) {
               bool changed = false;
               changed |= ImGui::DragFloat(Tr("半径", "Radius"), &component.sphereRadius_, 0.01f, 0.001f, 1000.0f);
               std::int32_t stacks = static_cast<std::int32_t>(component.sphereStacks_);
               std::int32_t slices = static_cast<std::int32_t>(component.sphereSlices_);

               if (ImGui::DragInt(Tr("スタック", "Stacks"), &stacks, 1.0f, 2, 256)) {
                  component.sphereStacks_ = static_cast<uint32_t>(std::max(stacks, 2));
                  changed = true;
               }

               if (ImGui::DragInt(Tr("スライス", "Slices"), &slices, 1.0f, 3, 512)) {
                  component.sphereSlices_ = static_cast<uint32_t>(std::max(slices, 3));
                  changed = true;
               }

               return changed;
            },
#endif
      },
      {
         .type = MeshComponent::PrimitiveType::Box,
         .name = "Box",
         .supportsPlaneOrientation = false,
         .supportsOriginY = true,
         .build =
            [](Mesh& mesh, const MeshComponent& component) {
               mesh.CreateBox(component.boxSize_.x, component.boxSize_.y, component.boxSize_.z, component.originY_);
            },
#ifdef USE_IMGUI
         .drawInspector =
            [](MeshComponent& component) {
               bool changed = false;
               changed |= ImGui::DragFloat3(Tr("サイズ", "Size"), &component.boxSize_.x, 0.01f, 0.001f, 1000.0f);
               return changed;
            },
#endif
      },
      {
         .type = MeshComponent::PrimitiveType::Cylinder,
         .name = "Cylinder",
         .supportsPlaneOrientation = false,
         .supportsOriginY = true,
         .build =
            [](Mesh& mesh, const MeshComponent& component) {
               mesh.CreateCylinder(component.cylinderTopRadius_, component.cylinderBottomRadius_,
                  component.cylinderHeight_, component.cylinderSegments_, component.originY_);
            },
#ifdef USE_IMGUI
         .drawInspector =
            [](MeshComponent& component) {
               bool changed = false;
               changed |=
                  ImGui::DragFloat(Tr("上半径", "Top Radius"), &component.cylinderTopRadius_, 0.01f, 0.0f, 1000.0f);
               changed |= ImGui::DragFloat(
                  Tr("下半径", "Bottom Radius"), &component.cylinderBottomRadius_, 0.01f, 0.0f, 1000.0f);
               changed |= ImGui::DragFloat(Tr("高さ", "Height"), &component.cylinderHeight_, 0.01f, 0.001f, 1000.0f);
               std::int32_t segments = static_cast<std::int32_t>(component.cylinderSegments_);

               if (ImGui::DragInt(Tr("分割数", "Segments"), &segments, 1.0f, 3, 512)) {
                  component.cylinderSegments_ = static_cast<uint32_t>(std::max(segments, 3));
                  changed = true;
               }

               return changed;
            },
#endif
      },
      {
         .type = MeshComponent::PrimitiveType::Cone,
         .name = "Cone",
         .supportsPlaneOrientation = false,
         .supportsOriginY = true,
         .build =
            [](Mesh& mesh, const MeshComponent& component) {
               mesh.CreateCone(
                  component.coneRadius_, component.coneHeight_, component.coneSegments_, component.originY_);
            },
#ifdef USE_IMGUI
         .drawInspector =
            [](MeshComponent& component) {
               bool changed = false;
               changed |= ImGui::DragFloat(Tr("半径", "Radius"), &component.coneRadius_, 0.01f, 0.001f, 1000.0f);
               changed |= ImGui::DragFloat(Tr("高さ", "Height"), &component.coneHeight_, 0.01f, 0.001f, 1000.0f);
               std::int32_t segments = static_cast<std::int32_t>(component.coneSegments_);

               if (ImGui::DragInt(Tr("分割数", "Segments"), &segments, 1.0f, 3, 512)) {
                  component.coneSegments_ = static_cast<uint32_t>(std::max(segments, 3));
                  changed = true;
               }

               return changed;
            },
#endif
      },
      {
         .type = MeshComponent::PrimitiveType::Circle,
         .name = "Circle",
         .supportsPlaneOrientation = true,
         .supportsOriginY = false,
         .build =
            [](Mesh& mesh, const MeshComponent& component) {
               mesh.CreateCircle(component.circleRadius_, component.circleSegments_,
                  ToMeshPlaneOrientation(component.planeOrientation_));
            },
#ifdef USE_IMGUI
         .drawInspector =
            [](MeshComponent& component) {
               bool changed = false;
               changed |= ImGui::DragFloat(Tr("半径", "Radius"), &component.circleRadius_, 0.01f, 0.001f, 1000.0f);
               std::int32_t segments = static_cast<std::int32_t>(component.circleSegments_);

               if (ImGui::DragInt(Tr("分割数", "Segments"), &segments, 1.0f, 3, 512)) {
                  component.circleSegments_ = static_cast<uint32_t>(std::max(segments, 3));
                  changed = true;
               }

               return changed;
            },
#endif
      },
      {
         .type = MeshComponent::PrimitiveType::Plane,
         .name = "Plane",
         .supportsPlaneOrientation = true,
         .supportsOriginY = false,
         .build =
            [](Mesh& mesh, const MeshComponent& component) {
               mesh.CreatePlane(component.planeWidth_, component.planeDepth_, component.planeWidthSegments_,
                  component.planeDepthSegments_, ToMeshPlaneOrientation(component.planeOrientation_));
            },
#ifdef USE_IMGUI
         .drawInspector =
            [](MeshComponent& component) {
               bool changed = false;
               changed |= ImGui::DragFloat(Tr("幅", "Width"), &component.planeWidth_, 0.01f, 0.001f, 1000.0f);
               changed |= ImGui::DragFloat(Tr("奥行き", "Depth"), &component.planeDepth_, 0.01f, 0.001f, 1000.0f);
               std::int32_t widthSegments = static_cast<std::int32_t>(component.planeWidthSegments_);
               std::int32_t depthSegments = static_cast<std::int32_t>(component.planeDepthSegments_);

               if (ImGui::DragInt(Tr("幅の分割数", "Width Segments"), &widthSegments, 1.0f, 1, 512)) {
                  component.planeWidthSegments_ = static_cast<uint32_t>(std::max(widthSegments, 1));
                  changed = true;
               }

               if (ImGui::DragInt(Tr("奥行きの分割数", "Depth Segments"), &depthSegments, 1.0f, 1, 512)) {
                  component.planeDepthSegments_ = static_cast<uint32_t>(std::max(depthSegments, 1));
                  changed = true;
               }

               return changed;
            },
#endif
      },
      {
         .type = MeshComponent::PrimitiveType::Torus,
         .name = "Torus",
         .supportsPlaneOrientation = false,
         .supportsOriginY = true,
         .build =
            [](Mesh& mesh, const MeshComponent& component) {
               mesh.CreateTorus(component.torusMajorRadius_, component.torusMinorRadius_, component.torusMajorSegments_,
                  component.torusMinorSegments_, component.originY_);
            },
#ifdef USE_IMGUI
         .drawInspector =
            [](MeshComponent& component) {
               bool changed = false;
               changed |=
                  ImGui::DragFloat(Tr("主半径", "Major Radius"), &component.torusMajorRadius_, 0.01f, 0.001f, 1000.0f);
               changed |=
                  ImGui::DragFloat(Tr("副半径", "Minor Radius"), &component.torusMinorRadius_, 0.01f, 0.001f, 1000.0f);
               std::int32_t majorSegments = static_cast<std::int32_t>(component.torusMajorSegments_);
               std::int32_t minorSegments = static_cast<std::int32_t>(component.torusMinorSegments_);

               if (ImGui::DragInt(Tr("主分割数", "Major Segments"), &majorSegments, 1.0f, 3, 512)) {
                  component.torusMajorSegments_ = static_cast<uint32_t>(std::max(majorSegments, 3));
                  changed = true;
               }

               if (ImGui::DragInt(Tr("副分割数", "Minor Segments"), &minorSegments, 1.0f, 3, 512)) {
                  component.torusMinorSegments_ = static_cast<uint32_t>(std::max(minorSegments, 3));
                  changed = true;
               }

               return changed;
            },
#endif
      },
      {
         .type = MeshComponent::PrimitiveType::Triangle,
         .name = "Triangle",
         .supportsPlaneOrientation = true,
         .supportsOriginY = false,
         .build =
            [](Mesh& mesh, const MeshComponent& component) {
               mesh.CreateTriangle(component.triangleV0_, component.triangleV1_, component.triangleV2_,
                  ToMeshPlaneOrientation(component.planeOrientation_));
            },
#ifdef USE_IMGUI
         .drawInspector =
            [](MeshComponent& component) {
               bool changed = false;
               changed |= ImGui::DragFloat3("V0", &component.triangleV0_.x, 0.01f);
               changed |= ImGui::DragFloat3("V1", &component.triangleV1_.x, 0.01f);
               changed |= ImGui::DragFloat3("V2", &component.triangleV2_.x, 0.01f);
               return changed;
            },
#endif
      }};

   return definitions;
}

const PrimitiveMeshDefinition* PrimitiveMeshFactory::Find(MeshComponent::PrimitiveType type) {
   for (const auto& definition : GetDefinitions()) {
      if (definition.type == type) {
         return &definition;
      }
   }

   return nullptr;
}

const PrimitiveMeshDefinition* PrimitiveMeshFactory::Find(std::string_view name) {
   for (const auto& definition : GetDefinitions()) {
      if (name == definition.name) {
         return &definition;
      }
   }

   return nullptr;
}

std::unique_ptr<Mesh> PrimitiveMeshFactory::Create(const MeshComponent& component) {
   const auto* definition = Find(component.primitiveType_);

   if (!definition) {
      Logger::Warning("[PrimitiveMeshFactory] Primitive type is not registered");
      return nullptr;
   }

   // 編集済み頂点を残さず、選択中パラメーターからGPUバッファまで一括で再構築する。
   auto mesh = std::make_unique<Mesh>();
   definition->build(*mesh, component);
   return mesh;
}
} // namespace GameEngine
