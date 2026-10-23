#include "GameEngine/pch.h"
#include <cstdint>
#include "GameEngine/Editor/EditorSceneContext.h"

#ifdef USE_IMGUI

#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Component/Rendering/RenderComponent.h"
#include "GameEngine/Object/Component/Rendering/LightComponent.h"
#include "GameEngine/Object/Component/UI/UITextComponent.h"
#include "GameEngine/Effects/Particles/ParticleSystem.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Object/Model/Model.h"
#include "GameEngine/Object/Skybox/Skybox.h"
#include "GameEngine/Scene/Camera/Camera.h"
#include "GameEngine/Scene/Camera/Core/CinemachineBrain.h"
#include "GameEngine/Scene/Camera/Core/VirtualCamera.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameEngine/Object/Sprite/Sprite.h"
#include "GameEngine/Object/Text/UIText.h"
#include "GameEngine/Editor/ImGui/ImGuiHelper.h"
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Utility/Logger.h"
#include "GameEngine/Utility/JsonFile.h"
#include "ImGuizmo.h"
#include "imgui.h"
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>

#include "GameEngine/Editor/EditorSceneSession.h"
#include "GameEngine/Object/Component/Rendering/LightComponent.h"
#include "GameEngine/Object/Component/Animation/AnimationComponent.h"
#include "GameEngine/Object/Component/Effects/ParticleEmitterComponent.h"
#include "GameEngine/Scene/Camera/Core/CinemachineBrain.h"
#include "GameEngine/Editor/EditorSceneGeometry.h"

namespace GameEngine::EditorSceneGeometry {
ImGuizmo::OPERATION ToImGuizmoOperation(EditorSceneContext::GizmoOperation operation, bool restrictTo2D) {
   if (restrictTo2D) {
      // スクリーン空間の要素は深度軸を操作すると描画順まで変わるため、平面内の自由度だけを公開する。
      switch (operation) {
         case EditorSceneContext::GizmoOperation::Rotate:
            return ImGuizmo::ROTATE_Z;
         case EditorSceneContext::GizmoOperation::Scale:
            return ImGuizmo::SCALE_X | ImGuizmo::SCALE_Y;
         case EditorSceneContext::GizmoOperation::Translate:
         default:
            return ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y;
      }
   }

   switch (operation) {
      case EditorSceneContext::GizmoOperation::Rotate:
         return ImGuizmo::ROTATE;
      case EditorSceneContext::GizmoOperation::Scale:
         return ImGuizmo::SCALE;
      case EditorSceneContext::GizmoOperation::Translate:
      default:
         return ImGuizmo::TRANSLATE;
   }
}

ImGuizmo::MODE ToImGuizmoMode(EditorSceneContext::GizmoMode mode) {
   return mode == EditorSceneContext::GizmoMode::World ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
}

Transform MatrixToTransform(const Matrix4x4& matrix) {
   float translation[3]{};
   float rotationDegrees[3]{};
   float scale[3]{};
   ImGuizmo::DecomposeMatrixToComponents(&matrix.m[0][0], translation, rotationDegrees, scale);

   Transform transform{};
   transform.translation = Vector3(translation[0], translation[1], translation[2]);
   transform.scale = Vector3(scale[0], scale[1], scale[2]);

   // ImGuizmoのEuler角は表示上の中間値に留め、エンジン側では合成時に安定するQuaternionを正規化して保持する。
   const Vector3 eulerRadians =
      Vector3(ToRadians(rotationDegrees[0]), ToRadians(rotationDegrees[1]), ToRadians(rotationDegrees[2]));
   transform.SetRotationQuaternion(eulerRadians.ToQuaternion().Normalize());
   return transform;
}

float AbsDiff(float lhs, float rhs) {
   return std::abs(lhs - rhs);
}

std::string BuildDuplicateName(const std::string& name) {
   const std::string base = name.empty() ? "Object" : name;
   return base + "_Copy";
}

bool IsFiniteVector(const Vector3& value) {
   return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

Vector3 NormalizeOrFallback(const Vector3& value, const Vector3& fallback) {
   if (!IsFiniteVector(value) || value.LengthSquared() < kVectorLengthEpsilonSquared) {
      return fallback;
   }

   return value.Normalize();
}

Vector3 ExtractCameraPositionFromView(const Camera* camera) {
   if (!camera) {
      return {};
   }

   // ブレンド中のカメラではTransformより実際のView行列が描画位置を正確に表すため、逆行列から位置を取り出す。
   const Matrix4x4 cameraWorld = camera->GetViewMatrix().Inverse();
   const Vector3 position(cameraWorld.m[3][0], cameraWorld.m[3][1], cameraWorld.m[3][2]);
   return IsFiniteVector(position) ? position : camera->GetPosition();
}

Vector3 ExtractCameraForwardFromView(const Camera* camera) {
   if (!camera) {
      return Vector3(0.0f, 0.0f, 1.0f);
   }

   // 配置方向も現在描画中のViewに合わせ、TransformとViewの更新タイミング差で画面外へ生成されるのを避ける。
   const Matrix4x4 cameraWorld = camera->GetViewMatrix().Inverse();
   const Vector3 forward(cameraWorld.m[2][0], cameraWorld.m[2][1], cameraWorld.m[2][2]);
   return NormalizeOrFallback(forward, NormalizeOrFallback(camera->GetForward(), Vector3(0.0f, 0.0f, 1.0f)));
}

bool IsProjectedInsideCamera(const Camera* camera, const Vector3& worldPosition) {
   if (!camera || !IsFiniteVector(worldPosition)) {
      return false;
   }

   const Vector4 clip = TransformVectorByMatrix(
      Vector4(worldPosition.x, worldPosition.y, worldPosition.z, 1.0f), camera->GetViewProjectionMatrix());

   if (!std::isfinite(clip.x) || !std::isfinite(clip.y) || !std::isfinite(clip.z) || !std::isfinite(clip.w) ||
       std::abs(clip.w) < kHomogeneousCoordinateEpsilon) {
      return false;
   }

   const float ndcX = clip.x / clip.w;
   const float ndcY = clip.y / clip.w;
   const float ndcZ = clip.z / clip.w;

   // クリップ境界ぎりぎりを避け、生成直後のオブジェクトが確実に選択できる余白を残す。
   return ndcX >= -kSafeNormalizedDeviceCoordinateLimit && ndcX <= kSafeNormalizedDeviceCoordinateLimit &&
          ndcY >= -kSafeNormalizedDeviceCoordinateLimit && ndcY <= kSafeNormalizedDeviceCoordinateLimit &&
          ndcZ >= 0.0f && ndcZ <= 1.0f;
}

bool UsesScreenRenderSpace(const Object* object) {
   // UITextはTextRendererがスクリーン空間で描画する。
   if (dynamic_cast<const UIText*>(object)) {
      return true;
   }

   if (!dynamic_cast<const Sprite*>(object)) {
      return false;
   }

   const auto* renderComponent = object->GetComponent<RenderComponent>();
   return renderComponent && renderComponent->DescribeSettings().renderSpace == RenderComponent::RenderSpace::Screen;
}

Vector2 GetEditorScreenCameraSize(float viewportWidth, float viewportHeight) {
   float outputWidth = std::max(viewportWidth, 1.0f);
   float outputHeight = std::max(viewportHeight, 1.0f);

   // スクリーン空間オブジェクトはバックバッファへ描画されるため、実出力の縦横比を使う。
   if (auto* graphicsDevice = EngineContext::GetGraphicsDevice()) {
      const uint32_t width = graphicsDevice->GetBackBufferWidth();
      const uint32_t height = graphicsDevice->GetBackBufferHeight();

      if (width > 0 && height > 0) {
         outputWidth = static_cast<float>(width);
         outputHeight = static_cast<float>(height);
      }
   }

   const float referenceWidth = static_cast<float>(Window::kUiReferenceWidth);
   const float referenceHeight = static_cast<float>(Window::kUiReferenceHeight);
   const float outputScale = std::min(outputWidth / referenceWidth, outputHeight / referenceHeight);
   return Vector2(outputWidth / outputScale, outputHeight / outputScale);
}

Vector2 GetEditorScreenLayoutSize() {
   return Vector2(static_cast<float>(Window::kUiReferenceWidth), static_cast<float>(Window::kUiReferenceHeight));
}

Vector3 CalculateScreenAnchorOffset(Sprite::AnchorPoint anchorPoint, const Vector2& screenSize) {
   const float halfWidth = screenSize.x * 0.5f;
   const float halfHeight = screenSize.y * 0.5f;

   switch (anchorPoint) {
      case Sprite::AnchorPoint::TopLeft:
         return Vector3(-halfWidth, halfHeight, 0.0f);
      case Sprite::AnchorPoint::TopCenter:
         return Vector3(0.0f, halfHeight, 0.0f);
      case Sprite::AnchorPoint::TopRight:
         return Vector3(halfWidth, halfHeight, 0.0f);
      case Sprite::AnchorPoint::MiddleLeft:
         return Vector3(-halfWidth, 0.0f, 0.0f);
      case Sprite::AnchorPoint::MiddleRight:
         return Vector3(halfWidth, 0.0f, 0.0f);
      case Sprite::AnchorPoint::BottomLeft:
         return Vector3(-halfWidth, -halfHeight, 0.0f);
      case Sprite::AnchorPoint::BottomCenter:
         return Vector3(0.0f, -halfHeight, 0.0f);
      case Sprite::AnchorPoint::BottomRight:
         return Vector3(halfWidth, -halfHeight, 0.0f);
      case Sprite::AnchorPoint::MiddleCenter:
      default:
         return Vector3(0.0f, 0.0f, 0.0f);
   }
}

Vector3 CalculateScreenAnchorOffset(UIAnchor anchorPoint, const Vector2& screenSize) {
   const float halfWidth = screenSize.x * 0.5f;
   const float halfHeight = screenSize.y * 0.5f;

   // UITextはレイアウト系と同じY下向き規約を使うため、Sprite用アンカーとは上下の符号が逆になる。
   switch (anchorPoint) {
      case UIAnchor::TopLeft:
         return Vector3(-halfWidth, -halfHeight, 0.0f);
      case UIAnchor::TopCenter:
         return Vector3(0.0f, -halfHeight, 0.0f);
      case UIAnchor::TopRight:
         return Vector3(halfWidth, -halfHeight, 0.0f);
      case UIAnchor::MiddleLeft:
         return Vector3(-halfWidth, 0.0f, 0.0f);
      case UIAnchor::MiddleRight:
         return Vector3(halfWidth, 0.0f, 0.0f);
      case UIAnchor::BottomLeft:
         return Vector3(-halfWidth, halfHeight, 0.0f);
      case UIAnchor::BottomCenter:
         return Vector3(0.0f, halfHeight, 0.0f);
      case UIAnchor::BottomRight:
         return Vector3(halfWidth, halfHeight, 0.0f);
      case UIAnchor::MiddleCenter:
      default:
         return Vector3(0.0f, 0.0f, 0.0f);
   }
}

Vector3 GetScreenRenderOffset(const Object* object, const Vector2& screenSize) {
   if (const auto* sprite = dynamic_cast<const Sprite*>(object)) {
      return CalculateScreenAnchorOffset(sprite->GetScreenAnchorPoint(), screenSize);
   }

   if (const auto* uiText = dynamic_cast<const UIText*>(object)) {
      if (const auto* textComponent = uiText->GetComponent<UITextComponent>()) {
         return CalculateScreenAnchorOffset(textComponent->GetStyle().screenAnchor, screenSize);
      }
   }

   return Vector3(0.0f, 0.0f, 0.0f);
}

Vector3 ToEditorScreenWorldPosition(const Vector3& screenTranslation, const Vector3& anchorOffset) {
   return Vector3(
      anchorOffset.x + screenTranslation.x, anchorOffset.y + screenTranslation.y, anchorOffset.z + screenTranslation.z);
}

Vector3 FromEditorScreenWorldPosition(const Vector3& worldPosition, const Vector3& anchorOffset) {
   return Vector3(worldPosition.x - anchorOffset.x, worldPosition.y - anchorOffset.y, worldPosition.z - anchorOffset.z);
}

Matrix4x4 MakeScreenSpaceProjectionMatrix(const Vector2& screenSize, bool useDownwardYAxis) {
   const float top = useDownwardYAxis ? -screenSize.y * 0.5f : screenSize.y * 0.5f;
   const float bottom = -top;
   return MakeOrthographicMatrix(-screenSize.x * 0.5f, top, screenSize.x * 0.5f, bottom, 0.0f, kScreenSpaceFarClip);
}

Transform BuildScreenSpacePlacementTransform() {
   Transform transform{};
   transform.translation.z = kDefaultScreenSpaceDepth;
   return transform;
}

}
#endif
