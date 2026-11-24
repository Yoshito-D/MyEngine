#include "GameEngine/pch.h"
#include "GameEngine/Object/Component/Effects/TrailComponent.h"
#include "GameEngine/Graphics/Device/GraphicsDevice.h"
#include "GameEngine/Graphics/Device/ResourceHelper.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Scene/Camera/Camera.h"
#include "GameEngine/Utility/JsonValue.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#endif

namespace GameEngine {
namespace {

const bool kRegisteredTrail = ComponentRegistry::GetInstance().RegisterFactory(
   TrailComponent::kTypeName, [](Object& object) -> IObjectComponent* { return object.AddComponent<TrailComponent>(); },
   TrailComponent::kDisplayName, ObjectType::Model | ObjectType::Generic);

constexpr float kMinWidth = 0.001f;                    ///< 設定可能な全幅の下限（m）。
constexpr float kMinLifetime = 0.01f;                  ///< 設定可能な寿命の下限（秒）。
constexpr float kMinDistance = 0.001f;                 ///< 設定可能な履歴点の間隔の下限（m）。
constexpr float kPositionEpsilonSquared = 0.00000001f; ///< 同じ座標と見なす距離の二乗（m²）。
constexpr float kLengthGrowthSpeed = 18.0f;            ///< 表示長を伸ばす速さ（m/s）。
constexpr float kLengthRetractionSpeed = 11.0f;        ///< 表示長を縮める速さ（m/s）。
constexpr float kStopRetractionDuration = 0.28f;       ///< 放出を止めてから末端が先端まで移動する時間（秒）。
constexpr float kTailFadeFraction = 0.45f;             ///< 各帯の末端を先細りにする長さの割合。
constexpr UINT kVerticesPerPoint = 2;

struct TrailVertex {
   Vector3 position;
   Vector4 color;
   float side;
};

} // namespace

void TrailComponent::OnSceneLoaded(SceneWorld&) {
   Clear();
   targetLength_.reset();
   emitting_ = emitOnAwake_;
}

void TrailComponent::OnDisable() {
   Clear();
}

void TrailComponent::Clear() {
   for (auto& path : paths_) {
      path.points.clear();
      path.continues = false;
      path.length = 0.0f;
      path.retractionSpeed = 0.0f;
   }
}

void TrailComponent::SetEmitting(bool emitting) {
   if (emitting_ == emitting) return;

   emitting_ = emitting;

   if (!emitting) {
      for (auto& path : paths_) {
         path.continues = false;
         // 停止時の長さから縮める速度を決め、短い軌跡も同じ時間をかけて消す。
         path.retractionSpeed = path.length / kStopRetractionDuration;
      }
   }
}

void TrailComponent::SetTargetLength(float length) {
   if (std::isfinite(length)) {
      targetLength_ = std::max(0.0f, length);
   }
}

void TrailComponent::Update(float deltaTime) {
   if (!HasOwner() || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return;

   const Matrix4x4 world = emitting_ ? GetOwner().GetWorldMatrix() : MakeIdentity4x4();

   for (size_t index = 0; index < offsets_.size(); ++index) {
      auto& path = paths_[index];
      auto& points = path.points;

      for (auto& point : points) {
         point.age += deltaTime;
      }

      while (!points.empty() && points.front().age >= lifetime_) {
         points.pop_front();
      }

      if (emitting_) {
         RecordPoint(path, TransformCoordinate(offsets_[index], world));
      }

      if (targetLength_) {
         const float target = emitting_ ? *targetLength_ : 0.0f;
         const float retractionSpeed = emitting_ ? kLengthRetractionSpeed : path.retractionSpeed;
         const float change =
            std::clamp(target - path.length, -retractionSpeed * deltaTime, kLengthGrowthSpeed * deltaTime);
         path.length = TrimToLength(path, path.length + change);
      }
   }
}

void TrailComponent::RecordPoint(Path& path, const Vector3& position) {
   auto& points = path.points;

   if (!std::isfinite(position.LengthSquared())) {
      path.continues = false;
      return;
   }

   const float movementSquared = points.empty() ? 0.0f : (position - points.back().position).LengthSquared();

   if (!path.continues || points.empty() || movementSquared > breakDistance_ * breakDistance_) {
      points.push_back({position, 0.0f, true});
      path.continues = true;
   } else if (movementSquared > kPositionEpsilonSquared) {
      // 最後の点は発生点に追従させ、間隔を満たした点だけをワールド空間の固定履歴にする。
      if (points.size() == 1 || points.back().startsStrip ||
          (points.back().position - points[points.size() - 2].position).LengthSquared() >=
             minDistance_ * minDistance_) {
         points.push_back({position, 0.0f, false});
      } else {
         points.back() = {position, 0.0f, false};
      }
   }

   if (points.size() > kMaxPointsPerPath) {
      points.pop_front();
   }
}

float TrailComponent::TrimToLength(Path& path, float limit) {
   auto& points = path.points;

   if (points.empty()) {
      return 0.0f;
   }

   if (limit <= 0.0f) {
      // 次の放出で使える先端だけを残す。
      points.erase(points.begin(), points.end() - 1);
      return 0.0f;
   }

   float length = 0.0f;

   for (size_t index = points.size() - 1; index > 0; --index) {
      const auto& newer = points[index];

      if (newer.startsStrip) {
         continue;
      }

      auto& older = points[index - 1];
      const float segmentLength = (older.position - newer.position).Length();

      if (segmentLength > limit - length) {
         // 履歴点を一つずつ落とすと末端が跳ぶため、区間の途中へ末端を移す。
         const float fraction = (limit - length) / segmentLength;
         older.position = Vector3::Lerp(newer.position, older.position, fraction);
         older.age = std::lerp(newer.age, older.age, fraction);
         points.erase(points.begin(), points.begin() + (index - 1));
         return limit;
      }

      length += segmentLength;
   }

   return length;
}

std::optional<Vector3> TrailComponent::GetSortPosition() const {
   if (IsEnabled() && color_.w > 0.0f) {
      for (const auto& path : paths_) {
         for (size_t index = path.points.size(); index > 1; --index) {
            if (!path.points[index - 1].startsStrip) {
               return path.points[index - 1].position;
            }
         }
      }
   }

   return std::nullopt;
}

void TrailComponent::Draw(GraphicsDevice& device, const Camera& camera, std::uint32_t transformSlot) {
   if (!vertexBuffer_) {
      // フレーム末尾でGPU完了を待つため、コンポーネントごとの単一バッファを再利用できる。
      vertexBuffer_ = ResourceHelper::CreateBufferResource(
         device.GetDevice(), sizeof(TrailVertex) * kMaxPaths * kMaxPointsPerPath * kVerticesPerPoint);
   }

   if (!vertexBuffer_) return;

   TrailVertex* vertices = nullptr;
   const D3D12_RANGE noRead{0, 0};

   if (FAILED(vertexBuffer_->Map(0, &noRead, reinterpret_cast<void**>(&vertices)))) return;

   const auto viewProjection = camera.GetViewProjectionMatrix();
   const Vector3 cameraPosition = camera.GetTransform().translation;
   const Vector3 towardCamera = -camera.GetForward();
   const bool orthographic = camera.GetProjectionType() == Camera::ProjectionType::Orthographic;
   auto* commandList = device.GetCommandList();
   commandList->SetGraphicsRoot32BitConstants(transformSlot, sizeof(Matrix4x4) / sizeof(float), &viewProjection, 0);
   commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
   const D3D12_VERTEX_BUFFER_VIEW view{
      vertexBuffer_->GetGPUVirtualAddress(), static_cast<UINT>(vertexBuffer_->GetDesc().Width), sizeof(TrailVertex)};
   commandList->IASetVertexBuffers(0, 1, &view);
   UINT vertexCount = 0;

   for (const auto& path : paths_) {
      for (size_t start = 0; start < path.points.size();) {
         size_t end = start + 1;

         while (end < path.points.size() && !path.points[end].startsStrip) {
            ++end;
         }

         if (end - start < 2) {
            start = end;
            continue;
         }

         const UINT firstVertex = vertexCount;
         Vector3 previousSide{};
         float tailFadeLength = 0.0f;

         if (targetLength_) {
            for (size_t index = start + 1; index < end; ++index) {
               tailFadeLength += (path.points[index].position - path.points[index - 1].position).Length();
            }

            tailFadeLength *= kTailFadeFraction;
         }

         float distanceFromTail = 0.0f;
         for (size_t index = start; index < end; ++index) {
            const Point& point = path.points[index];
            const Vector3 before = path.points[index == start ? index : index - 1].position;
            const Vector3 after = path.points[index + 1 == end ? index : index + 1].position;
            const Vector3 incoming = (point.position - before).Normalize();
            const Vector3 outgoing = (after - point.position).Normalize();
            const Vector3 tangent = (incoming + outgoing).Normalize();
            const Vector3 viewDirection = orthographic ? towardCamera : (cameraPosition - point.position).Normalize();
            Vector3 side = tangent.Cross(viewDirection).Normalize();

            // 視線と軌跡が重なる点では前の幅方向を使い、帯の反転を抑える。
            if (side.LengthSquared() <= 0.0f) {
               side = previousSide;
            } else if (side.Dot(previousSide) < 0.0f) {
               side = -side;
            }

            previousSide = side;
            float taper = 1.0f;

            if (tailFadeLength > 0.0f) {
               distanceFromTail += (point.position - before).Length();
               taper = std::min(distanceFromTail / tailFadeLength, 1.0f);
            }

            const float remaining = taper * std::clamp(1.0f - point.age / lifetime_, 0.0f, 1.0f);
            const Vector3 halfWidth = side * (0.5f * width_ * remaining);
            const Vector4 color{color_.x, color_.y, color_.z, color_.w * remaining * remaining};
            vertices[vertexCount++] = {point.position - halfWidth, color, -1.0f};
            vertices[vertexCount++] = {point.position + halfWidth, color, 1.0f};
         }

         commandList->DrawInstanced(vertexCount - firstVertex, 1, firstVertex, 0);
         start = end;
      }
   }

   const D3D12_RANGE written{0, sizeof(TrailVertex) * vertexCount};
   vertexBuffer_->Unmap(0, &written);
}

nlohmann::json TrailComponent::Serialize() const {
   auto offsets = nlohmann::json::array();

   for (const auto& offset : offsets_) {
      offsets.push_back(JsonValue::WriteVector3(offset));
   }

   return {{"emitOnAwake", emitOnAwake_}, {"width", width_}, {"lifetime", lifetime_}, {"minDistance", minDistance_},
      {"breakDistance", breakDistance_}, {"offsets", std::move(offsets)},
      {"color", {color_.x, color_.y, color_.z, color_.w}}};
}

void TrailComponent::Deserialize(const nlohmann::json& data) {
   ReadSettings(data);
   Clear();
   targetLength_.reset();
   emitting_ = emitOnAwake_;
}

void TrailComponent::ApplySharedParameters(const nlohmann::json& data) {
   const bool previousEmitOnAwake = emitOnAwake_;
   ReadSettings(data);

   if (previousEmitOnAwake != emitOnAwake_) {
      SetEmitting(emitOnAwake_);
   }
}

void TrailComponent::ReadSettings(const nlohmann::json& data) {
   emitOnAwake_ = JsonValue::ReadBool(data, "emitOnAwake", emitOnAwake_);
   width_ = std::max(kMinWidth, JsonValue::ReadFloat(data, "width", width_));
   lifetime_ = std::max(kMinLifetime, JsonValue::ReadFloat(data, "lifetime", lifetime_));
   minDistance_ = std::max(kMinDistance, JsonValue::ReadFloat(data, "minDistance", minDistance_));
   breakDistance_ = std::max(minDistance_, JsonValue::ReadFloat(data, "breakDistance", breakDistance_));

   if (const auto color = data.find("color"); color != data.end()) {
      color_ = JsonValue::ReadVector4(*color);
      color_.w = std::clamp(color_.w, 0.0f, 1.0f);
   }

   const auto offsets = data.find("offsets");

   if (offsets == data.end() || !offsets->is_array() || offsets->empty() || offsets->size() > kMaxPaths) return;

   const size_t previousSize = offsets_.size();
   offsets_.resize(offsets->size());

   for (size_t index = 0; index < offsets_.size(); ++index) {
      const Vector3 previous = offsets_[index];
      offsets_[index] = JsonValue::ReadVector3((*offsets)[index]);

      if ((offsets_[index] - previous).LengthSquared() > 0.0f) {
         paths_[index] = {};
      }
   }

   for (size_t index = offsets_.size(); index < previousSize; ++index) {
      paths_[index] = {};
   }
}

#ifdef USE_IMGUI
void TrailComponent::DrawInspector() {
   const std::string header = MakeObjectComponentHeaderLabel(kTypeName);

   if (!DrawParameterInspectorHeader(header.c_str())) return;

   if (ImGui::Checkbox(LocalizeEditorText("開始時に放出", "Emit on Awake"), &emitOnAwake_)) {
      SetEmitting(emitOnAwake_);
   }

   const float maxValue = std::numeric_limits<float>::max();
   ImGui::ColorEdit4(LocalizeEditorText("色", "Color"), &color_.x);
   constexpr auto clamp = ImGuiSliderFlags_AlwaysClamp;
   ImGui::DragFloat(LocalizeEditorText("幅 (m)", "Width (m)"), &width_, 0.01f, kMinWidth, maxValue, "%.3f", clamp);
   ImGui::DragFloat(
      LocalizeEditorText("寿命 (s)", "Lifetime (s)"), &lifetime_, 0.01f, kMinLifetime, maxValue, "%.3f", clamp);
   ImGui::DragFloat(LocalizeEditorText("記録間隔 (m)", "Sample Spacing (m)"), &minDistance_, 0.01f, kMinDistance,
      maxValue, "%.3f", clamp);
   ImGui::DragFloat(LocalizeEditorText("分割距離 (m)", "Break Distance (m)"), &breakDistance_, 0.1f, minDistance_,
      maxValue, "%.3f", clamp);
   breakDistance_ = std::max(breakDistance_, minDistance_);

   for (size_t index = 0; index < offsets_.size(); ++index) {
      ImGui::PushID(static_cast<std::int32_t>(index));

      if (ImGui::DragFloat3(LocalizeEditorText("発生位置 (m)", "Local Offset (m)"), &offsets_[index].x, 0.01f)) {
         paths_[index] = {};
      }

      ImGui::SameLine();
      const bool remove = offsets_.size() > 1 && ImGui::SmallButton(LocalizeEditorText("削除", "Remove"));
      ImGui::PopID();

      if (remove) {
         offsets_.erase(offsets_.begin() + index);
         Clear();
         break;
      }
   }

   if (offsets_.size() < kMaxPaths && ImGui::Button(LocalizeEditorText("発生位置を追加", "Add Offset"))) {
      offsets_.push_back({});
   }
}
#endif

} // namespace GameEngine
