#include "pch.h"

#ifdef USE_IMGUI

#include "ColorOverLifetimeModule.h"
#include "EmissionModule.h"
#include "ForceOverLifetimeModule.h"
#include "LimitVelocityOverLifetimeModule.h"
#include "MainModule.h"
#include "NoiseModule.h"
#include "ParticleMeshModule.h"
#include "RendererModule.h"
#include "RotationOverLifetimeModule.h"
#include "ShapeModule.h"
#include "SizeOverLifetimeModule.h"
#include "TextureSheetAnimationModule.h"
#include "TrailModule.h"
#include "UVTransformModule.h"
#include "VelocityOverLifetimeModule.h"
#include "Core/Graphics/Texture.h"
#include "Framework/EngineContext.h"
#include "Utility/ImGuiHelper.h"
#include "Utility/MathUtils/ColorUtils.h"

#include "imgui.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace GameEngine {
namespace {

constexpr float kInspectorColumnWidth = 140.0f;

const char* L(const ImGuiHelper::LocalizedText& text) {
   return ImGuiHelper::Localize(text);
}

uint32_t PackColor(const Vector4& color) {
   const auto channel = [](float value) -> uint32_t {
	  return static_cast<uint32_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f);
	  };

   return (channel(color.w) << 24) |
	  (channel(color.x) << 16) |
	  (channel(color.y) << 8) |
	  channel(color.z);
}

void ClampRange(float& minValue, float& maxValue) {
   // インスペクターで下限を上限より大きくした場合も、乱数分布へ正しい順序を渡す。
   if (minValue > maxValue) {
	  minValue = maxValue;
   }
}

void ClampRange(Vector3& minValue, Vector3& maxValue) {
   // ベクトル乱数は成分ごとに独立して抽選するため、各軸を個別に正規化する。
   if (minValue.x > maxValue.x) minValue.x = maxValue.x;
   if (minValue.y > maxValue.y) minValue.y = maxValue.y;
   if (minValue.z > maxValue.z) minValue.z = maxValue.z;
}

bool DrawRandomFloat(
   const std::string& label,
   RandomFloat& value,
   float speed,
   float minValue,
   float maxValue,
   float columnWidth = kInspectorColumnWidth) {
   bool changed = false;
   ImGui::PushID(label.c_str());
   ImGui::SeparatorText(label.c_str());

   bool randomize = value.randomize;
   if (ImGuiHelper::DrawCheckbox(L({ "ランダム", "Random" }), randomize, columnWidth)) {
	  value.randomize = randomize;
	  changed = true;
   }

   if (value.randomize) {
	  float min = value.minValue;
	  float max = value.maxValue;
	  if (ImGuiHelper::DrawRangeFloat(L({ "範囲", "Range" }), min, max, minValue, maxValue, columnWidth, speed)) {
		 value.minValue = min;
		 value.maxValue = max;
		 changed = true;
	  }
   } else {
	  float scalar = value.minValue;
	  if (ImGuiHelper::DrawFloatControl(L({ "値", "Value" }), scalar, value.minValue, columnWidth, speed, minValue, maxValue)) {
		 // 固定モードでは両端を同期し、後からRandomへ切り替えた際に古いmax値が突然復活しないようにする。
		 value.minValue = scalar;
		 value.maxValue = scalar;
		 changed = true;
	  }
   }

   ImGui::PopID();
   return changed;
}

bool DrawRandomVector2(
   const std::string& label,
   RandomVector2& value,
   float speed,
   float minValue,
   float maxValue,
   float columnWidth = kInspectorColumnWidth) {
   bool changed = false;
   ImGui::PushID(label.c_str());
   ImGui::SeparatorText(label.c_str());

   bool randomize = value.randomize;
   if (ImGuiHelper::DrawCheckbox(L({ "ランダム", "Random" }), randomize, columnWidth)) {
	  value.randomize = randomize;
	  changed = true;
   }

   if (value.randomize) {
	  Vector2 minVec = value.minValue;
	  Vector2 maxVec = value.maxValue;
	  // 片側ずつ編集するUIなので、その場で反対端へ制約して常に有効な成分別範囲を保つ。
	  if (ImGuiHelper::DrawVec2Control(L({ "最小", "Min" }), minVec, 0.0f, columnWidth, speed, minValue, maxValue)) {
		 if (minVec.x > maxVec.x) minVec.x = maxVec.x;
		 if (minVec.y > maxVec.y) minVec.y = maxVec.y;
		 value.minValue = minVec;
		 changed = true;
	  }
	  if (ImGuiHelper::DrawVec2Control(L({ "最大", "Max" }), maxVec, 0.0f, columnWidth, speed, minValue, maxValue)) {
		 if (maxVec.x < value.minValue.x) maxVec.x = value.minValue.x;
		 if (maxVec.y < value.minValue.y) maxVec.y = value.minValue.y;
		 value.maxValue = maxVec;
		 changed = true;
	  }
   } else {
	  Vector2 vectorValue = value.minValue;
	  if (ImGuiHelper::DrawVec2Control(L({ "値", "Value" }), vectorValue, 0.0f, columnWidth, speed, minValue, maxValue)) {
		 value.minValue = vectorValue;
		 value.maxValue = vectorValue;
		 changed = true;
	  }
   }

   ImGui::PopID();
   return changed;
}

bool DrawRandomVector3(
   const std::string& label,
   RandomVector3& value,
   float speed,
   float minValue,
   float maxValue,
   float columnWidth = kInspectorColumnWidth) {
   bool changed = false;
   ImGui::PushID(label.c_str());
   ImGui::SeparatorText(label.c_str());

   bool randomize = value.randomize;
   if (ImGuiHelper::DrawCheckbox(L({ "ランダム", "Random" }), randomize, columnWidth)) {
	  value.randomize = randomize;
	  changed = true;
   }

   if (value.randomize) {
	  Vector3 minVec = value.minValue;
	  Vector3 maxVec = value.maxValue;
	  if (ImGuiHelper::DrawVec3Control(L({ "最小", "Min" }), minVec, 0.0f, columnWidth, speed, minValue, maxValue)) {
		 ClampRange(minVec, maxVec);
		 value.minValue = minVec;
		 changed = true;
	  }
	  if (ImGuiHelper::DrawVec3Control(L({ "最大", "Max" }), maxVec, 0.0f, columnWidth, speed, minValue, maxValue)) {
		 Vector3 minCurrent = value.minValue;
		 if (maxVec.x < minCurrent.x) maxVec.x = minCurrent.x;
		 if (maxVec.y < minCurrent.y) maxVec.y = minCurrent.y;
		 if (maxVec.z < minCurrent.z) maxVec.z = minCurrent.z;
		 value.maxValue = maxVec;
		 changed = true;
	  }
   } else {
	  Vector3 vectorValue = value.minValue;
	  if (ImGuiHelper::DrawVec3Control(L({ "値", "Value" }), vectorValue, 0.0f, columnWidth, speed, minValue, maxValue)) {
		 value.minValue = vectorValue;
		 value.maxValue = vectorValue;
		 changed = true;
	  }
   }

   ImGui::PopID();
   return changed;
}

bool DrawRandomEulerDegrees(
   const std::string& label,
   RandomVector3& value,
   float speedDegrees = 1.0f,
   float minDegrees = -360.0f,
   float maxDegrees = 360.0f,
   float columnWidth = kInspectorColumnWidth) {
   bool changed = false;
   ImGui::PushID(label.c_str());
   ImGui::SeparatorText(label.c_str());

   bool randomize = value.randomize;
   if (ImGuiHelper::DrawCheckbox(L({ "ランダム", "Random" }), randomize, columnWidth)) {
	  value.randomize = randomize;
	  changed = true;
   }

   // 保存・計算はradian、Inspectorだけdegreeへ変換し、編集しやすさと内部単位の一貫性を両立する。
   if (value.randomize) {
	  Vector3 minDeg = ImGuiHelper::RadiansToDegrees(value.minValue);
	  Vector3 maxDeg = ImGuiHelper::RadiansToDegrees(value.maxValue);
	  if (ImGuiHelper::DrawVec3Control(L({ "最小 (deg)", "Min (deg)" }), minDeg, 0.0f, columnWidth, speedDegrees, minDegrees, maxDegrees, "%.1f")) {
		 ClampRange(minDeg, maxDeg);
		 value.minValue = ImGuiHelper::DegreesToRadians(minDeg);
		 changed = true;
	  }
	  if (ImGuiHelper::DrawVec3Control(L({ "最大 (deg)", "Max (deg)" }), maxDeg, 0.0f, columnWidth, speedDegrees, minDegrees, maxDegrees, "%.1f")) {
		 Vector3 minCurrent = ImGuiHelper::RadiansToDegrees(value.minValue);
		 if (maxDeg.x < minCurrent.x) maxDeg.x = minCurrent.x;
		 if (maxDeg.y < minCurrent.y) maxDeg.y = minCurrent.y;
		 if (maxDeg.z < minCurrent.z) maxDeg.z = minCurrent.z;
		 value.maxValue = ImGuiHelper::DegreesToRadians(maxDeg);
		 changed = true;
	  }
   } else {
	  Vector3 eulerDegrees = ImGuiHelper::RadiansToDegrees(value.minValue);
	  if (ImGuiHelper::DrawVec3Control(L({ "値 (deg)", "Value (deg)" }), eulerDegrees, 0.0f, columnWidth, speedDegrees, minDegrees, maxDegrees, "%.1f")) {
		 Vector3 radians = ImGuiHelper::DegreesToRadians(eulerDegrees);
		 value.minValue = radians;
		 value.maxValue = radians;
		 changed = true;
	  }
   }

   ImGui::PopID();
   return changed;
}

bool DrawRandomColor(const std::string& label, RandomColor& value, float columnWidth = kInspectorColumnWidth) {
   bool changed = false;
   ImGui::PushID(label.c_str());
   ImGui::SeparatorText(label.c_str());

   bool randomize = value.randomize;
   if (ImGuiHelper::DrawCheckbox(L({ "ランダム", "Random" }), randomize, columnWidth)) {
	  value.randomize = randomize;
	  changed = true;
   }

   // UIはfloat RGBA、RandomColorはpacked整数なので、表示の入口と確定時だけ相互変換する。
   if (value.randomize) {
	  Vector4 minColor = ConvertUIntToColor(value.minValue);
	  Vector4 maxColor = ConvertUIntToColor(value.maxValue);
	  if (ImGuiHelper::DrawColorEdit4(L({ "最小", "Min" }), minColor, columnWidth)) {
		 value.minValue = PackColor(minColor);
		 changed = true;
	  }
	  if (ImGuiHelper::DrawColorEdit4(L({ "最大", "Max" }), maxColor, columnWidth)) {
		 value.maxValue = PackColor(maxColor);
		 changed = true;
	  }
   } else {
	  Vector4 color = ConvertUIntToColor(value.minValue);
	  if (ImGuiHelper::DrawColorEdit4(L({ "色", "Color" }), color, columnWidth)) {
		 uint32_t packed = PackColor(color);
		 value.minValue = packed;
		 value.maxValue = packed;
		 changed = true;
	  }
   }

   ImGui::PopID();
   return changed;
}

template <typename Module>
bool DrawModuleEnabled(Module& module, const char* id) {
   // 全モジュールで同じ有効化UIとID規則を共有し、ラベル衝突を防ぐ。
   ImGui::PushID(id);
   bool enabled = module.IsEnabled();
   const bool changed = ImGuiHelper::DrawCheckbox(L({ "有効", "Enabled" }), enabled, kInspectorColumnWidth);
   if (changed) {
	  module.SetEnabled(enabled);
   }
   ImGui::PopID();
   return enabled;
}

} // namespace

void MainModule::DrawInspector() {
   float duration = GetDuration();
   if (ImGuiHelper::DrawFloatControl(L({ "再生時間", "Duration" }), duration, 5.0f, kInspectorColumnWidth, 0.1f, 0.1f, 60.0f)) {
	  SetDuration(duration);
   }

   bool looping = IsLooping();
   if (ImGuiHelper::DrawCheckbox(L({ "ループ", "Looping" }), looping, kInspectorColumnWidth)) {
	  SetLooping(looping);
   }

   bool playOnAwake = GetPlayOnAwake();
   if (ImGuiHelper::DrawCheckbox(L({ "開始時に再生", "Play On Awake" }), playOnAwake, kInspectorColumnWidth)) {
	  SetPlayOnAwake(playOnAwake);
   }

   RandomFloat lifetime = GetStartLifetime();
   if (DrawRandomFloat(L({ "開始寿命", "Start Lifetime" }), lifetime, 0.1f, 0.1f, 20.0f)) {
	  SetStartLifetime(lifetime);
   }

   StartSpeedMode speedMode = GetStartSpeedMode();

   if (GetStartSpeedMode() == StartSpeedMode::Directional) {
	  RandomFloat speed = GetStartSpeed();
	  if (DrawRandomFloat(L({ "開始速度", "Start Speed" }), speed, 0.1f, 0.0f, 50.0f)) {
		 SetStartSpeed(speed);
	  }
   } else {
	  RandomVector3 velocity = GetStartVelocity();
	  if (DrawRandomVector3(L({ "開始速度ベクトル", "Start Velocity" }), velocity, 0.1f, -50.0f, 50.0f)) {
		 SetStartVelocity(velocity);
	  }
   }

   if (ImGuiHelper::DrawLocalizedEnumCombo(
	  L({ "開始速度モード", "Start Speed Mode" }),
	  speedMode,
	  {
		 { StartSpeedMode::Directional, { "方向速度", "Directional" } },
		 { StartSpeedMode::Vector3, { "ベクトル", "Vector3" } },
	  },
	  kInspectorColumnWidth)) {
	  SetStartSpeedMode(speedMode);
   }

   RandomVector3 size = GetStartSize();
   if (DrawRandomVector3(L({ "開始サイズ", "Start Size" }), size, 0.01f, 0.0f, 10.0f)) {
	  SetStartSize(size);
   }

   RandomVector3 rotation = GetStartRotation();
   if (DrawRandomEulerDegrees(L({ "開始回転", "Start Rotation" }), rotation)) {
	  SetStartRotation(rotation);
   }

   RandomColor color = GetStartColor();
   if (DrawRandomColor(L({ "開始色", "Start Color" }), color)) {
	  SetStartColor(color);
   }

   ImGui::Separator();

   RandomFloat gravity = GetGravityModifierRange();
   if (DrawRandomFloat(L({ "重力倍率", "Gravity Modifier" }), gravity, 0.1f, -10.0f, 10.0f)) {
	  SetGravityModifierRange(gravity);
   }

   float timeScale = GetTimeScale();
   if (ImGuiHelper::DrawFloatControl(L({ "時間倍率", "Time Scale" }), timeScale, 1.0f, kInspectorColumnWidth, 0.05f, 0.0f, 10.0f)) {
	  SetTimeScale(timeScale);
   }

   ImGui::Separator();

   SimulationSpace simulationSpace = GetSimulationSpace();
   if (ImGuiHelper::DrawLocalizedEnumCombo(
	  L({ "シミュレーション空間", "Simulation Space" }),
	  simulationSpace,
	  {
		 { SimulationSpace::World, { "ワールド", "World" } },
		 { SimulationSpace::Local, { "ローカル", "Local" } },
	  },
	  kInspectorColumnWidth)) {
	  SetSimulationSpace(simulationSpace);
   }

   ImGui::Separator();

   int maxParticles = static_cast<int>(GetMaxParticles());
   if (ImGuiHelper::DrawIntControl(L({ "最大粒子数", "Max Particles" }), maxParticles, 1000, kInspectorColumnWidth, 1.0f, 1, 10000)) {
	  SetMaxParticles(static_cast<uint32_t>(std::max(maxParticles, 1)));
   }
}

void EmissionModule::DrawInspector() {
   if (!DrawModuleEnabled(*this, "Emission")) {
	  return;
   }

   float rateOverTime = GetRateOverTime();
   if (ImGuiHelper::DrawFloatControl(L({ "時間あたり放出数", "Rate Over Time" }), rateOverTime, 10.0f, kInspectorColumnWidth, 0.5f, 0.0f, 200.0f)) {
	  SetRateOverTime(rateOverTime);
   }

   float rateOverDistance = GetRateOverDistance();
   if (ImGuiHelper::DrawFloatControl(L({ "距離あたり放出数", "Rate Over Distance" }), rateOverDistance, 0.0f, kInspectorColumnWidth, 0.1f, 0.0f, 50.0f)) {
	  SetRateOverDistance(rateOverDistance);
   }

   ImGui::Separator();
   ImGui::Text("%s (%zu)", L({ "バースト", "Bursts" }), GetBursts().size());

   auto& bursts = GetBursts();
   // 走査中のeraseで参照を無効化しないよう、削除対象だけ記録してループ後に反映する。
   int removeIndex = -1;
   for (int i = 0; i < static_cast<int>(bursts.size()); ++i) {
	  auto& burst = bursts[i];
	  ImGui::PushID(i);

	  std::string title = std::string(L({ "バースト", "Burst" })) + " [" + std::to_string(i) + "]";
	  bool open = ImGui::CollapsingHeader(title.c_str());
	  ImGui::SameLine();
	  if (ImGui::SmallButton(L({ "削除", "Remove" }))) {
		 removeIndex = i;
	  }

	  if (open) {
		 ImGui::Indent();

		 float time = burst.time;
		 if (ImGuiHelper::DrawFloatControl(L({ "時間", "Time" }), time, 0.0f, kInspectorColumnWidth, 0.05f, 0.0f, 999.0f, "%.2f")) {
			burst.time = time;
			// 時刻設定の変更後は実行時カウンターを破棄し、新しいスケジュールを先頭から評価し直す。
			ResetBurstStates();
		 }

		 int count = static_cast<int>(burst.count);
		 if (ImGuiHelper::DrawIntControl(L({ "数", "Count" }), count, 10, kInspectorColumnWidth, 1.0f, 1, 10000)) {
			burst.count = static_cast<uint32_t>(std::max(count, 1));
			ResetBurstStates();
		 }

		 int cycles = static_cast<int>(burst.cycles);
		 if (ImGuiHelper::DrawIntControl(L({ "回数", "Cycles" }), cycles, 1, kInspectorColumnWidth, 1.0f, 0, 1000)) {
			burst.cycles = static_cast<uint32_t>(std::max(cycles, 0));
			ResetBurstStates();
		 }

		 float interval = burst.interval;
		 if (ImGuiHelper::DrawFloatControl(L({ "間隔", "Interval" }), interval, 1.0f, kInspectorColumnWidth, 0.05f, 0.01f, 60.0f, "%.2f")) {
			burst.interval = interval;
			ResetBurstStates();
		 }

		 ImGui::TextDisabled("firedCount: %u  nextFireTime: %.2f", burst.firedCount, burst.nextFireTime);
		 ImGui::Unindent();
	  }

	  ImGui::PopID();
   }

   if (removeIndex >= 0) {
	  bursts.erase(bursts.begin() + removeIndex);
	  ResetBurstStates();
   }

   if (ImGui::Button(L({ "+ バースト追加", "+ Add Burst" }))) {
	  Burst burst{};
	  burst.time = 0.0f;
	  burst.count = 10;
	  burst.cycles = 1;
	  burst.interval = 1.0f;
	  AddBurst(burst);
	  ResetBurstStates();
   }
   ImGui::SameLine();
   if (ImGui::Button(L({ "バーストをクリア", "Clear Bursts" }))) {
	  ClearBursts();
   }
}

void ShapeModule::DrawInspector() {
   if (!DrawModuleEnabled(*this, "Shape")) {
	  return;
   }

   ShapeType shapeType = GetShapeType();
   if (ImGuiHelper::DrawLocalizedEnumCombo(
	  L({ "形状タイプ", "Shape Type" }),
	  shapeType,
	  {
		 { ShapeType::Sphere, { "球", "Sphere" } },
		 { ShapeType::Hemisphere, { "半球", "Hemisphere" } },
		 { ShapeType::Cone, { "円錐", "Cone" } },
		 { ShapeType::Box, { "箱", "Box" } },
		 { ShapeType::Circle, { "円", "Circle" } },
		 { ShapeType::Edge, { "エッジ", "Edge" } },
		 { ShapeType::Point, { "点", "Point" } },
		 { ShapeType::Cylinder, { "円柱", "Cylinder" } },
		 { ShapeType::Torus, { "トーラス", "Torus" } },
		 { ShapeType::SkinnedMesh, { "スキンメッシュ", "Skinned Mesh" } },
	  },
	  kInspectorColumnWidth)) {
	  SetShapeType(shapeType);
   }

   EmitFrom emitFrom = GetEmitFrom();
   if (ImGuiHelper::DrawLocalizedEnumCombo(
	  L({ "放出元", "Emit From" }),
	  emitFrom,
	  {
		 { EmitFrom::Volume, { "体積", "Volume" } },
		 { EmitFrom::Shell, { "表面", "Shell" } },
		 { EmitFrom::Edge, { "エッジ", "Edge" } },
	  },
	  kInspectorColumnWidth)) {
	  SetEmitFrom(emitFrom);
   }

   // 共有パラメーターの意味が形状ごとに異なるため、現在のShapeが利用する設定だけを表示する。
   switch (GetShapeType()) {
	  case ShapeType::Sphere:
	  case ShapeType::Hemisphere: {
		 float radius = GetRadius();
		 if (ImGuiHelper::DrawFloatControl(L({ "半径", "Radius" }), radius, 1.0f, kInspectorColumnWidth, 0.1f, 0.1f, 100.0f)) {
			SetRadius(radius);
		 }
		 break;
	  }
	  case ShapeType::Cone: {
		 float angle = GetAngle();
		 if (ImGuiHelper::DrawFloatControl(L({ "角度 (deg)", "Angle (deg)" }), angle, 25.0f, kInspectorColumnWidth, 1.0f, 0.0f, 90.0f)) {
			SetAngle(angle);
		 }

		 float radius = GetRadius();
		 if (ImGuiHelper::DrawFloatControl(L({ "半径", "Radius" }), radius, 1.0f, kInspectorColumnWidth, 0.1f, 0.1f, 100.0f)) {
			SetRadius(radius);
		 }

		 float length = GetLength();
		 if (ImGuiHelper::DrawFloatControl(L({ "長さ", "Length" }), length, 5.0f, kInspectorColumnWidth, 0.1f, 0.1f, 100.0f)) {
			SetLength(length);
		 }
		 break;
	  }
	  case ShapeType::Box: {
		 Vector3 boxSize = GetBoxSize();
		 if (ImGuiHelper::DrawVec3Control(L({ "箱サイズ", "Box Size" }), boxSize, 1.0f, kInspectorColumnWidth, 0.1f, 0.1f, 100.0f)) {
			SetBoxSize(boxSize);
		 }
		 break;
	  }
	  case ShapeType::Circle: {
		 float radius = GetRadius();
		 if (ImGuiHelper::DrawFloatControl(L({ "半径", "Radius" }), radius, 1.0f, kInspectorColumnWidth, 0.1f, 0.1f, 100.0f)) {
			SetRadius(radius);
		 }

		 float arc = GetArc();
		 if (ImGuiHelper::DrawFloatControl(L({ "円弧 (deg)", "Arc (deg)" }), arc, 360.0f, kInspectorColumnWidth, 1.0f, 0.0f, 360.0f)) {
			SetArc(arc);
		 }

		 float outwardVelocity = GetCircleOutwardVelocity();
		 if (ImGuiHelper::DrawFloatControl(L({ "外向き速度", "Outward Velocity" }), outwardVelocity, 0.0f, kInspectorColumnWidth, 0.1f, 0.0f, 100.0f)) {
			SetCircleOutwardVelocity(outwardVelocity);
		 }
		 break;
	  }
	  case ShapeType::Cylinder: {
		 float radius = GetRadius();
		 if (ImGuiHelper::DrawFloatControl(L({ "半径", "Radius" }), radius, 1.0f, kInspectorColumnWidth, 0.1f, 0.0f, 100.0f)) {
			SetRadius(radius);
		 }
		 float height = GetLength();
		 if (ImGuiHelper::DrawFloatControl(L({ "高さ", "Height" }), height, 5.0f, kInspectorColumnWidth, 0.1f, 0.0f, 100.0f)) {
			SetLength(height);
		 }
		 break;
	  }
	  case ShapeType::Torus: {
		 float majorRadius = GetTorusMajorRadius();
		 if (ImGuiHelper::DrawFloatControl(L({ "主半径", "Major Radius" }), majorRadius, 1.0f, kInspectorColumnWidth, 0.1f, 0.0f, 100.0f)) {
			SetTorusMajorRadius(majorRadius);
		 }
		 float minorRadius = GetRadius();
		 if (ImGuiHelper::DrawFloatControl(L({ "副半径", "Minor Radius" }), minorRadius, 1.0f, kInspectorColumnWidth, 0.1f, 0.0f, 100.0f)) {
			SetRadius(minorRadius);
		 }
		 break;
	  }
	  default:
		 break;
   }

   ImGui::Separator();

   Vector3 position = GetPosition();
   if (ImGuiHelper::DrawVec3Control(L({ "位置", "Position" }), position, 0.0f, kInspectorColumnWidth, 0.1f)) {
	  SetPosition(position);
   }

   Quaternion rotation = GetRotationQuaternion();
   if (ImGuiHelper::DrawQuaternionAsEulerDegrees(L({ "回転 (deg)", "Rotation (deg)" }), rotation, kInspectorColumnWidth, 0.1f)) {
	  SetRotation(rotation);
   }

   Vector3 scale = GetScale();
   if (ImGuiHelper::DrawVec3Control(L({ "スケール", "Scale" }), scale, 1.0f, kInspectorColumnWidth, 0.1f, 0.1f, 10.0f)) {
	  SetScale(scale);
   }
}

void VelocityOverLifetimeModule::DrawInspector() {
   if (!DrawModuleEnabled(*this, "VelocityOverLifetime")) {
	  return;
   }

   RandomVector3 linearVelocity = GetLinearVelocityRange();
   if (DrawRandomVector3(L({ "線形速度", "Linear Velocity" }), linearVelocity, 0.1f, -50.0f, 50.0f)) {
	  SetLinearVelocityRange(linearVelocity);
   }

   RandomFloat speedModifier = GetSpeedModifierRange();
   if (DrawRandomFloat(L({ "速度補正", "Speed Modifier" }), speedModifier, 0.01f, 0.0f, 5.0f)) {
	  SetSpeedModifierRange(speedModifier);
   }
}

void LimitVelocityOverLifetimeModule::DrawInspector() {
   if (!DrawModuleEnabled(*this, "LimitVelocity")) {
	  return;
   }

   RandomFloat speedLimit = GetSpeedLimitRange();
   if (DrawRandomFloat(L({ "速度上限", "Speed Limit" }), speedLimit, 0.1f, 0.0f, 100.0f)) {
	  SetSpeedLimitRange(speedLimit);
   }

   RandomFloat dampen = GetDampenRange();
   if (DrawRandomFloat(L({ "減衰", "Dampen" }), dampen, 0.01f, 0.0f, 1.0f)) {
	  SetDampenRange(dampen);
   }
}

void ForceOverLifetimeModule::DrawInspector() {
   if (!DrawModuleEnabled(*this, "ForceOverLifetime")) {
	  return;
   }

   RandomVector3 force = GetForceRange();
   if (DrawRandomVector3(L({ "力", "Force" }), force, 0.1f, -50.0f, 50.0f)) {
	  SetForceRange(force);
   }

   RandomFloat drag = GetDragRange();
   if (DrawRandomFloat(L({ "空気抵抗", "Drag" }), drag, 0.05f, 0.0f, 50.0f)) {
	  SetDragRange(drag);
   }

   bool attractorEnabled = IsAttractorEnabled();
   if (ImGuiHelper::DrawCheckbox(L({ "ポイントフォース", "Point Force" }), attractorEnabled, kInspectorColumnWidth)) {
	  SetAttractorEnabled(attractorEnabled);
   }
   if (attractorEnabled) {
	  Vector3 position = GetAttractorPosition();
	  if (ImGuiHelper::DrawVec3Control(L({ "中心", "Center" }), position, 0.0f, kInspectorColumnWidth, 0.1f)) {
		 SetAttractorPosition(position);
	  }
	  float strength = GetAttractorStrength();
	  if (ImGuiHelper::DrawFloatControl(L({ "強度 (+引力 / -斥力)", "Strength (+Attract / -Repel)" }), strength, 0.0f, kInspectorColumnWidth, 0.1f, -1000.0f, 1000.0f)) {
		 SetAttractorStrength(strength);
	  }
	  float radius = GetAttractorRadius();
	  if (ImGuiHelper::DrawFloatControl(L({ "作用半径 (0=無限)", "Radius (0=Infinite)" }), radius, 0.0f, kInspectorColumnWidth, 0.1f, 0.0f, 1000.0f)) {
		 SetAttractorRadius(radius);
	  }
	  float falloff = GetAttractorFalloff();
	  if (ImGuiHelper::DrawFloatControl(L({ "距離減衰", "Falloff" }), falloff, 1.0f, kInspectorColumnWidth, 0.1f, 0.0f, 8.0f)) {
		 SetAttractorFalloff(falloff);
	  }
   }
}

void ColorOverLifetimeModule::DrawInspector() {
   if (!DrawModuleEnabled(*this, "ColorOverLifetime")) {
	  return;
   }

   Vector4 startColor = GetStartColor();
   if (ImGuiHelper::DrawColorEdit4(L({ "開始色", "Start Color" }), startColor, kInspectorColumnWidth)) {
	  SetStartColor(startColor);
   }

   Vector4 endColor = GetEndColor();
   if (ImGuiHelper::DrawColorEdit4(L({ "終了色", "End Color" }), endColor, kInspectorColumnWidth)) {
	  SetEndColor(endColor);
   }
}

void SizeOverLifetimeModule::DrawInspector() {
   if (!DrawModuleEnabled(*this, "SizeOverLifetime")) {
	  return;
   }

   RandomFloat sizeMultiplier = GetSizeMultiplierRange();
   if (DrawRandomFloat(L({ "サイズ倍率", "Size Multiplier" }), sizeMultiplier, 0.01f, 0.0f, 10.0f)) {
	  SetSizeMultiplierRange(sizeMultiplier);
   }

   Vector3 startSize = GetStartSize();
   if (ImGuiHelper::DrawVec3Control(L({ "開始サイズ", "Start Size" }), startSize, 1.0f, kInspectorColumnWidth, 0.01f, 0.0f, 10.0f)) {
	  SetStartSize(startSize);
   }

   Vector3 endSize = GetEndSize();
   if (ImGuiHelper::DrawVec3Control(L({ "終了サイズ", "End Size" }), endSize, 0.0f, kInspectorColumnWidth, 0.01f, 0.0f, 10.0f)) {
	  SetEndSize(endSize);
   }
}

void RotationOverLifetimeModule::DrawInspector() {
   if (!DrawModuleEnabled(*this, "RotationOverLifetime")) {
	  return;
   }

   RandomVector3 angularVelocity{ GetAngularVelocityMin(), GetAngularVelocityMax(), GetAngularVelocityRandomize() };
   if (DrawRandomEulerDegrees(L({ "角速度", "Angular Velocity" }), angularVelocity, 1.0f, -360.0f, 360.0f)) {
	  SetAngularVelocityMin(angularVelocity.minValue);
	  SetAngularVelocityMax(angularVelocity.maxValue);
	  SetAngularVelocityRandomize(angularVelocity.randomize);
   }
}

void NoiseModule::DrawInspector() {
   if (!DrawModuleEnabled(*this, "Noise")) {
	  return;
   }

   RandomFloat strength = GetStrengthRange();
   if (DrawRandomFloat(L({ "強さ", "Strength" }), strength, 0.1f, 0.0f, 10.0f)) {
	  SetStrengthRange(strength);
   }

   RandomFloat frequency = GetFrequencyRange();
   if (DrawRandomFloat(L({ "周波数", "Frequency" }), frequency, 0.01f, 0.0f, 5.0f)) {
	  SetFrequencyRange(frequency);
   }

   RandomFloat scrollSpeed = GetScrollSpeedRange();
   if (DrawRandomFloat(L({ "スクロール速度", "Scroll Speed" }), scrollSpeed, 0.1f, 0.0f, 10.0f)) {
	  SetScrollSpeedRange(scrollSpeed);
   }
}

void UVTransformModule::DrawInspector() {
   if (!DrawModuleEnabled(*this, "UVTransform")) {
	  return;
   }

   // Scroll/Rotation/Scaleで同じValueMode定義を使い、各モード固有値は切替後も保持して再編集可能にする。
   const std::vector<std::pair<ValueMode, ImGuiHelper::LocalizedText>> valueModes = {
	  { ValueMode::Constant, { "固定", "Constant" } },
	  { ValueMode::RandomBetweenTwoConstants, { "2定数ランダム", "Random Between Two Constants" } },
	  { ValueMode::Curve, { "開始から終了", "Start To End" } },
   };

   ValueMode scrollMode = GetScrollMode();
   if (ImGuiHelper::DrawLocalizedEnumCombo(L({ "スクロールモード", "Scroll Mode" }), scrollMode, valueModes, kInspectorColumnWidth)) {
	  SetScrollMode(scrollMode);
   }
   if (GetScrollMode() == ValueMode::Constant) {
	  Vector2 value = GetScrollConstant();
	  if (ImGuiHelper::DrawVec2Control(L({ "スクロール", "Scroll" }), value, 0.0f, kInspectorColumnWidth, 0.01f, -10.0f, 10.0f)) {
		 SetScrollConstant(value);
	  }
   } else if (GetScrollMode() == ValueMode::RandomBetweenTwoConstants) {
	  RandomVector2 value = GetScrollRandom();
	  if (DrawRandomVector2(L({ "スクロール範囲", "Scroll Range" }), value, 0.01f, -10.0f, 10.0f)) {
		 SetScrollRandom(value);
	  }
   } else {
	  Vector2 start = GetScrollCurveStart();
	  Vector2 end = GetScrollCurveEnd();
	  if (ImGuiHelper::DrawVec2Control(L({ "開始スクロール", "Start Scroll" }), start, 0.0f, kInspectorColumnWidth, 0.01f, -10.0f, 10.0f)) {
		 SetScrollCurveStart(start);
	  }
	  if (ImGuiHelper::DrawVec2Control(L({ "終了スクロール", "End Scroll" }), end, 0.0f, kInspectorColumnWidth, 0.01f, -10.0f, 10.0f)) {
		 SetScrollCurveEnd(end);
	  }
   }

   ImGui::Separator();

   ValueMode rotationMode = GetRotationMode();
   if (ImGuiHelper::DrawLocalizedEnumCombo(L({ "回転モード", "Rotation Mode" }), rotationMode, valueModes, kInspectorColumnWidth)) {
	  SetRotationMode(rotationMode);
   }
   if (GetRotationMode() == ValueMode::Constant) {
	  float valueDegrees = ImGuiHelper::RadiansToDegrees(GetRotationConstant());
	  if (ImGuiHelper::DrawFloatControl(L({ "回転 (deg)", "Rotation (deg)" }), valueDegrees, 0.0f, kInspectorColumnWidth, 0.1f, -360.0f, 360.0f)) {
		 SetRotationConstant(ImGuiHelper::DegreesToRadians(valueDegrees));
	  }
   } else if (GetRotationMode() == ValueMode::RandomBetweenTwoConstants) {
	  RandomFloat value = GetRotationRandom();
	  value.minValue = ImGuiHelper::RadiansToDegrees(value.minValue);
	  value.maxValue = ImGuiHelper::RadiansToDegrees(value.maxValue);
	  if (DrawRandomFloat(L({ "回転範囲 (deg)", "Rotation Range (deg)" }), value, 0.1f, -360.0f, 360.0f)) {
		 value.minValue = ImGuiHelper::DegreesToRadians(value.minValue);
		 value.maxValue = ImGuiHelper::DegreesToRadians(value.maxValue);
		 SetRotationRandom(value);
	  }
   } else {
	  float startDegrees = ImGuiHelper::RadiansToDegrees(GetRotationCurveStart());
	  float endDegrees = ImGuiHelper::RadiansToDegrees(GetRotationCurveEnd());
	  if (ImGuiHelper::DrawFloatControl(L({ "開始回転 (deg)", "Start Rotation (deg)" }), startDegrees, 0.0f, kInspectorColumnWidth, 0.1f, -360.0f, 360.0f)) {
		 SetRotationCurveStart(ImGuiHelper::DegreesToRadians(startDegrees));
	  }
	  if (ImGuiHelper::DrawFloatControl(L({ "終了回転 (deg)", "End Rotation (deg)" }), endDegrees, 0.0f, kInspectorColumnWidth, 0.1f, -360.0f, 360.0f)) {
		 SetRotationCurveEnd(ImGuiHelper::DegreesToRadians(endDegrees));
	  }
   }

   ImGui::Separator();

   ValueMode scaleMode = GetScaleMode();
   if (ImGuiHelper::DrawLocalizedEnumCombo(L({ "スケールモード", "Scale Mode" }), scaleMode, valueModes, kInspectorColumnWidth)) {
	  SetScaleMode(scaleMode);
   }
   if (GetScaleMode() == ValueMode::Constant) {
	  Vector2 value = GetScaleConstant();
	  if (ImGuiHelper::DrawVec2Control(L({ "スケール", "Scale" }), value, 1.0f, kInspectorColumnWidth, 0.01f, 0.0f, 10.0f)) {
		 SetScaleConstant(value);
	  }
   } else if (GetScaleMode() == ValueMode::RandomBetweenTwoConstants) {
	  RandomVector2 value = GetScaleRandom();
	  if (DrawRandomVector2(L({ "スケール範囲", "Scale Range" }), value, 0.01f, 0.0f, 10.0f)) {
		 SetScaleRandom(value);
	  }
   } else {
	  Vector2 start = GetScaleCurveStart();
	  Vector2 end = GetScaleCurveEnd();
	  if (ImGuiHelper::DrawVec2Control(L({ "開始スケール", "Start Scale" }), start, 1.0f, kInspectorColumnWidth, 0.01f, 0.0f, 10.0f)) {
		 SetScaleCurveStart(start);
	  }
	  if (ImGuiHelper::DrawVec2Control(L({ "終了スケール", "End Scale" }), end, 1.0f, kInspectorColumnWidth, 0.01f, 0.0f, 10.0f)) {
		 SetScaleCurveEnd(end);
	  }
   }
}

void TextureSheetAnimationModule::DrawInspector() {
   if (!DrawModuleEnabled(*this, "TextureSheetAnimation")) {
	  return;
   }

   int tilesX = static_cast<int>(GetTilesX());
   if (ImGuiHelper::DrawIntControl(L({ "横分割数", "Tiles X" }), tilesX, 1, kInspectorColumnWidth, 1.0f, 1, 64)) {
	  SetTilesX(static_cast<uint32_t>(std::max(tilesX, 1)));
   }

   int tilesY = static_cast<int>(GetTilesY());
   if (ImGuiHelper::DrawIntControl(L({ "縦分割数", "Tiles Y" }), tilesY, 1, kInspectorColumnWidth, 1.0f, 1, 64)) {
	  SetTilesY(static_cast<uint32_t>(std::max(tilesY, 1)));
   }

   float frameOverTime = GetFrameOverTime();
   if (ImGuiHelper::DrawFloatControl(L({ "時間あたりフレーム", "Frame Over Time" }), frameOverTime, 1.0f, kInspectorColumnWidth, 0.1f, 0.0f, 100.0f)) {
	  SetFrameOverTime(frameOverTime);
   }

   int cycles = static_cast<int>(GetCycles());
   if (ImGuiHelper::DrawIntControl(L({ "サイクル", "Cycles" }), cycles, 1, kInspectorColumnWidth, 1.0f, 1, 100)) {
	  SetCycles(static_cast<uint32_t>(std::max(cycles, 1)));
   }

   // 単一行ではX方向だけが再生対象なので、選択モードに合わせて入力上限を変える。
   int frameCount = static_cast<int>(GetFrameCount());
   int maxFrameCount = static_cast<int>(GetTilesX() * GetTilesY());
   if (GetAnimationMode() == AnimationMode::SingleRow) {
	  maxFrameCount = static_cast<int>(GetTilesX());
   }
   if (ImGuiHelper::DrawIntControl(L({ "フレーム数 (0=全て)", "Frame Count (0=All)" }), frameCount, 0, kInspectorColumnWidth, 1.0f, 0, maxFrameCount)) {
	  SetFrameCount(static_cast<uint32_t>(std::max(frameCount, 0)));
   }

   AnimationMode animationMode = GetAnimationMode();
   if (ImGuiHelper::DrawLocalizedEnumCombo(
	  L({ "アニメーションモード", "Animation Mode" }),
	  animationMode,
	  {
		 { AnimationMode::WholeSheet, { "シート全体", "Whole Sheet" } },
		 { AnimationMode::SingleRow, { "単一行", "Single Row" } },
	  },
	  kInspectorColumnWidth)) {
	  SetAnimationMode(animationMode);
	  if (GetAnimationMode() == AnimationMode::WholeSheet) {
		 // 全体走査ではrowがframe番号から決まるため、単一行専用の乱数設定を無効化する。
		 SetRandomRow(false);
	  }
   }

   int startFrame = static_cast<int>(GetStartFrame());
   if (ImGuiHelper::DrawIntControl(L({ "開始フレーム", "Start Frame" }), startFrame, 0, kInspectorColumnWidth, 1.0f, 0, 1024)) {
	  SetStartFrame(static_cast<uint32_t>(std::max(startFrame, 0)));
   }

   if (GetAnimationMode() == AnimationMode::SingleRow) {
	  bool randomRow = GetRandomRow();
	  if (ImGuiHelper::DrawCheckbox(L({ "ランダム行", "Random Row" }), randomRow, kInspectorColumnWidth)) {
		 SetRandomRow(randomRow);
	  }

	  if (!randomRow) {
		 int rowIndex = static_cast<int>(GetRowIndex());
		 if (ImGuiHelper::DrawIntControl(L({ "行インデックス", "Row Index" }), rowIndex, 0, kInspectorColumnWidth, 1.0f, 0, 1024)) {
			SetRowIndex(static_cast<uint32_t>(std::max(rowIndex, 0)));
		 }
	  }
   }
}

void RendererModule::DrawInspector() {
   if (!DrawModuleEnabled(*this, "Renderer")) {
	  return;
   }

   RotationSpace rotationSpace = GetRotationSpace();
   if (ImGuiHelper::DrawLocalizedEnumCombo(
	  L({ "回転空間", "Rotation Space" }),
	  rotationSpace,
	  {
		 { RotationSpace::World, { "ワールド", "World" } },
		 { RotationSpace::Local, { "ローカル", "Local" } },
	  },
	  kInspectorColumnWidth)) {
	  SetRotationSpace(rotationSpace);
   }

   BillboardType billboardType = GetBillboardType();
   if (ImGuiHelper::DrawLocalizedEnumCombo(
	  L({ "ビルボード", "Billboard Type" }),
	  billboardType,
	  {
		 { BillboardType::None, { "なし", "None" } },
		 { BillboardType::View, { "ビュー", "View" } },
		 { BillboardType::Horizontal, { "水平", "Horizontal" } },
		 { BillboardType::Vertical, { "垂直", "Vertical" } },
		 { BillboardType::Velocity, { "速度方向", "Velocity" } },
	  },
	  kInspectorColumnWidth)) {
	  SetBillboardType(billboardType);
   }

   bool velocityStretchEnabled = IsVelocityStretchEnabled();
   if (ImGuiHelper::DrawCheckbox(L({ "速度ストレッチ", "Velocity Stretch" }), velocityStretchEnabled, kInspectorColumnWidth)) {
	  SetVelocityStretchEnabled(velocityStretchEnabled);
   }

   // Stretchを使わない時は無効な係数を隠し、設定が描画へ影響する条件をUI上でも明確にする。
   if (velocityStretchEnabled) {
	  float speedScale = GetSpeedScale();
	  if (ImGuiHelper::DrawFloatControl(L({ "速度スケール", "Speed Scale" }), speedScale, 1.0f, kInspectorColumnWidth, 0.1f, 0.0f, 10.0f)) {
		 SetSpeedScale(speedScale);
	  }

	  float lengthScale = GetLengthScale();
	  if (ImGuiHelper::DrawFloatControl(L({ "長さスケール", "Length Scale" }), lengthScale, 2.0f, kInspectorColumnWidth, 0.1f, 0.0f, 10.0f)) {
		 SetLengthScale(lengthScale);
	  }
   }

   SortMode sortMode = GetSortMode();
   if (ImGuiHelper::DrawLocalizedEnumCombo(
	  L({ "描画順", "Sort Mode" }),
	  sortMode,
	  {
		 { SortMode::Auto, { "自動", "Auto" } },
		 { SortMode::None, { "なし", "None" } },
		 { SortMode::BackToFront, { "後方から前方", "Back To Front" } },
		 { SortMode::FrontToBack, { "前方から後方", "Front To Back" } },
	  },
	  kInspectorColumnWidth)) {
	  SetSortMode(sortMode);
   }

   bool cameraFadeEnabled = IsCameraFadeEnabled();
   if (ImGuiHelper::DrawCheckbox(L({ "カメラフェード", "Camera Fade" }), cameraFadeEnabled, kInspectorColumnWidth)) {
	  SetCameraFadeEnabled(cameraFadeEnabled);
   }
   if (cameraFadeEnabled) {
	  float fadeNear = GetCameraFadeNear();
	  if (ImGuiHelper::DrawFloatControl(L({ "透明距離", "Invisible Distance" }), fadeNear, 0.25f, kInspectorColumnWidth, 0.01f, 0.0f, 100.0f)) {
		 SetCameraFadeNear(fadeNear);
	  }
	  float fadeFar = GetCameraFadeFar();
	  if (ImGuiHelper::DrawFloatControl(L({ "表示距離", "Visible Distance" }), fadeFar, 1.0f, kInspectorColumnWidth, 0.01f, 0.0f, 100.0f)) {
		 SetCameraFadeFar(fadeFar);
	  }
   }

}

void TrailModule::DrawInspector() {
	if (!DrawModuleEnabled(*this, "Trail")) {
	  return;
   }

	TrailMode mode = GetMode();
	if (ImGuiHelper::DrawLocalizedEnumCombo(
	  L({ "トレイル方式", "Trail Mode" }),
	  mode,
	  {
		 { TrailMode::ParticlePath, { "パーティクルの移動軌跡", "Particle Path" } },
		 { TrailMode::EmitterToParticle, { "エミッターからパーティクル", "Emitter to Particle" } },
	  },
	  kInspectorColumnWidth)) {
	  SetMode(mode);
	}

      // index 0を空のtextureNameへ対応させ、粒子本体のTextureを継承する状態を明示的に選べるようにする。
      std::vector<std::string> textureOptions{ L({ "パーティクルと同じ", "Same as Particle" }) };
      int selectedTextureIndex = 0;
      for (const std::string& textureName : EngineContext::GetTextureNames()) {
         Texture* texture = EngineContext::GetTexture(textureName);
         if (texture && texture->GetMetadata().IsCubemap()) {
            continue;
         }
         textureOptions.push_back(textureName);
         if (textureName == GetTextureName()) {
            selectedTextureIndex = static_cast<int>(textureOptions.size() - 1);
         }
      }
      if (ImGuiHelper::DrawCombo(
         L({ "トレイル画像", "Trail Texture" }), selectedTextureIndex, textureOptions, kInspectorColumnWidth)) {
         SetTextureName(selectedTextureIndex == 0 ? std::string() : textureOptions[selectedTextureIndex]);
      }

      Vector4 color = GetColor();
      if (ImGuiHelper::DrawColorEdit4(L({ "トレイル色", "Trail Color" }), color, kInspectorColumnWidth)) {
         SetColor(color);
      }

	  RandomFloat width = GetWidthRange();
	  if (DrawRandomFloat(L({ "リボン幅", "Ribbon Width" }), width, 0.01f, 0.001f, 100.0f)) {
		 SetWidthRange(width);
	  }
	  // Emitter接続モードは常に二点だけを使うため、履歴点数と点間距離の設定はPath時だけ意味を持つ。
	  if (GetMode() == TrailMode::ParticlePath) {
		 int maxPoints = static_cast<int>(GetMaxPoints());
		 if (ImGuiHelper::DrawIntControl(L({ "履歴点数", "History Points" }), maxPoints, 16, kInspectorColumnWidth, 1.0f, 2, 128)) {
			SetMaxPoints(static_cast<uint32_t>(maxPoints));
		 }
		 float minDistance = GetMinDistance();
		 if (ImGuiHelper::DrawFloatControl(L({ "点間の最小距離", "Minimum Point Distance" }), minDistance, 0.1f, kInspectorColumnWidth, 0.01f, 0.001f, 100.0f)) {
			SetMinDistance(minDistance);
		 }
	  }
      float retractionDuration = GetRetractionDuration();
      if (ImGuiHelper::DrawFloatControl(L({ "消滅時間", "Retraction Duration" }), retractionDuration, 0.5f, kInspectorColumnWidth, 0.01f, 0.0f, 10.0f)) {
         SetRetractionDuration(retractionDuration);
      }
      float tailWidthScale = GetTailWidthScale();
      if (ImGuiHelper::DrawFloatControl(L({ "末尾の幅倍率", "Tail Width Scale" }), tailWidthScale, 0.0f, kInspectorColumnWidth, 0.01f, 0.0f, 1.0f)) {
         SetTailWidthScale(tailWidthScale);
      }
      float textureTiling = GetTextureTiling();
      if (ImGuiHelper::DrawFloatControl(L({ "画像の繰り返し", "Texture Tiling" }), textureTiling, 1.0f, kInspectorColumnWidth, 0.1f, 1.0f, 64.0f)) {
         SetTextureTiling(textureTiling);
      }
}

void ParticleMeshModule::DrawInspector() {
	const bool wasEnabled = IsEnabled();
	if (!DrawModuleEnabled(*this, "ParticleMesh")) {
	  if (wasEnabled != IsEnabled()) {
		 // 無効化時も既定Quadへ戻す再構築が必要なので、早期return前にdirtyを立てる。
		 meshDirty_ = true;
	  }
	  return;
   }
	if (wasEnabled != IsEnabled()) {
	  meshDirty_ = true;
	}

   MeshType meshType = GetMeshType();
   if (ImGuiHelper::DrawLocalizedEnumCombo(
	  L({ "メッシュタイプ", "Mesh Type" }),
	  meshType,
	  {
		 { MeshType::Quad, { "四角形", "Quad" } },
		 { MeshType::Ring, { "リング", "Ring" } },
		 { MeshType::Sphere, { "球", "Sphere" } },
		 { MeshType::Box, { "箱", "Box" } },
		 { MeshType::Cylinder, { "円柱", "Cylinder" } },
		 { MeshType::Cone, { "円錐", "Cone" } },
		 { MeshType::Circle, { "円", "Circle" } },
		 { MeshType::Plane, { "平面", "Plane" } },
		 { MeshType::Torus, { "トーラス", "Torus" } },
		 { MeshType::Triangle, { "三角形", "Triangle" } },
	  },
	  kInspectorColumnWidth)) {
	  SetMeshType(meshType);
   }

   const bool supportsMeshOriginY =
	  meshType == MeshType::Sphere ||
	  meshType == MeshType::Box ||
	  meshType == MeshType::Cylinder ||
	  meshType == MeshType::Cone ||
	  meshType == MeshType::Torus;
   if (supportsMeshOriginY) {
	  float originY = GetOriginY();
	  if (ImGuiHelper::DrawSliderFloat(L({ "原点Y", "Origin Y" }), originY, 0.0f, 1.0f, kInspectorColumnWidth, "%.2f")) {
		 SetOriginY(originY);
	  }
   }

   switch (meshType) {
	  case MeshType::Ring: {
		 float inner = GetRingInnerRadius();
		 if (ImGuiHelper::DrawFloatControl(L({ "内半径", "Inner Radius" }), inner, 0.4f, kInspectorColumnWidth, 0.01f, 0.0f, 10.0f)) {
			SetRingInnerRadius(inner);
		 }
		 float outer = GetRingOuterRadius();
		 if (ImGuiHelper::DrawFloatControl(L({ "外半径", "Outer Radius" }), outer, 0.5f, kInspectorColumnWidth, 0.01f, 0.0f, 10.0f)) {
			SetRingOuterRadius(outer);
		 }
		 int segments = static_cast<int>(GetRingSegments());
		 if (ImGuiHelper::DrawIntControl(L({ "分割数", "Segments" }), segments, 32, kInspectorColumnWidth, 1.0f, 3, 128)) {
			SetRingSegments(static_cast<uint32_t>(std::max(segments, 3)));
		 }
		 break;
	  }
	  case MeshType::Sphere: {
		 float radius = GetSphereRadius();
		 if (ImGuiHelper::DrawFloatControl(L({ "半径", "Radius" }), radius, 0.5f, kInspectorColumnWidth, 0.01f, 0.01f, 10.0f)) {
			SetSphereRadius(radius);
		 }
		 int stacks = static_cast<int>(GetSphereStacks());
		 if (ImGuiHelper::DrawIntControl(L({ "スタック", "Stacks" }), stacks, 16, kInspectorColumnWidth, 1.0f, 2, 64)) {
			SetSphereStacks(static_cast<uint32_t>(std::max(stacks, 2)));
		 }
		 int slices = static_cast<int>(GetSphereSlices());
		 if (ImGuiHelper::DrawIntControl(L({ "スライス", "Slices" }), slices, 32, kInspectorColumnWidth, 1.0f, 3, 128)) {
			SetSphereSlices(static_cast<uint32_t>(std::max(slices, 3)));
		 }
		 break;
	  }
	  case MeshType::Box: {
		 Vector3 size = GetBoxSize();
		 if (ImGuiHelper::DrawVec3Control(L({ "サイズ", "Size" }), size, 1.0f, kInspectorColumnWidth, 0.01f, 0.01f, 10.0f)) {
			SetBoxSize(size);
		 }
		 break;
	  }
	  case MeshType::Cylinder: {
		 float topRadius = GetCylinderTopRadius();
		 if (ImGuiHelper::DrawFloatControl(L({ "上半径", "Top Radius" }), topRadius, 0.5f, kInspectorColumnWidth, 0.01f, 0.0f, 10.0f)) {
			SetCylinderTopRadius(topRadius);
		 }
		 float bottomRadius = GetCylinderBottomRadius();
		 if (ImGuiHelper::DrawFloatControl(L({ "下半径", "Bottom Radius" }), bottomRadius, 0.5f, kInspectorColumnWidth, 0.01f, 0.0f, 10.0f)) {
			SetCylinderBottomRadius(bottomRadius);
		 }
		 float height = GetCylinderHeight();
		 if (ImGuiHelper::DrawFloatControl(L({ "高さ", "Height" }), height, 1.0f, kInspectorColumnWidth, 0.01f, 0.01f, 20.0f)) {
			SetCylinderHeight(height);
		 }
		 int segments = static_cast<int>(GetCylinderSegments());
		 if (ImGuiHelper::DrawIntControl(L({ "分割数", "Segments" }), segments, 32, kInspectorColumnWidth, 1.0f, 3, 128)) {
			SetCylinderSegments(static_cast<uint32_t>(std::max(segments, 3)));
		 }
		 break;
	  }
	  case MeshType::Cone: {
		 float radius = GetConeRadius();
		 if (ImGuiHelper::DrawFloatControl(L({ "半径", "Radius" }), radius, 0.5f, kInspectorColumnWidth, 0.01f, 0.01f, 10.0f)) {
			SetConeRadius(radius);
		 }
		 float height = GetConeHeight();
		 if (ImGuiHelper::DrawFloatControl(L({ "高さ", "Height" }), height, 1.0f, kInspectorColumnWidth, 0.01f, 0.01f, 20.0f)) {
			SetConeHeight(height);
		 }
		 int segments = static_cast<int>(GetConeSegments());
		 if (ImGuiHelper::DrawIntControl(L({ "分割数", "Segments" }), segments, 32, kInspectorColumnWidth, 1.0f, 3, 128)) {
			SetConeSegments(static_cast<uint32_t>(std::max(segments, 3)));
		 }
		 break;
	  }
	  case MeshType::Circle: {
		 float radius = GetCircleRadius();
		 if (ImGuiHelper::DrawFloatControl(L({ "半径", "Radius" }), radius, 0.5f, kInspectorColumnWidth, 0.01f, 0.01f, 10.0f)) {
			SetCircleRadius(radius);
		 }
		 int segments = static_cast<int>(GetCircleSegments());
		 if (ImGuiHelper::DrawIntControl(L({ "分割数", "Segments" }), segments, 32, kInspectorColumnWidth, 1.0f, 3, 128)) {
			SetCircleSegments(static_cast<uint32_t>(std::max(segments, 3)));
		 }
		 break;
	  }
	  case MeshType::Plane: {
		 float width = GetPlaneWidth();
		 if (ImGuiHelper::DrawFloatControl(L({ "幅", "Width" }), width, 1.0f, kInspectorColumnWidth, 0.01f, 0.01f, 20.0f)) {
			SetPlaneWidth(width);
		 }
		 float depth = GetPlaneDepth();
		 if (ImGuiHelper::DrawFloatControl(L({ "奥行き", "Depth" }), depth, 1.0f, kInspectorColumnWidth, 0.01f, 0.01f, 20.0f)) {
			SetPlaneDepth(depth);
		 }
		 break;
	  }
	  case MeshType::Torus: {
		 float majorRadius = GetTorusMajorRadius();
		 if (ImGuiHelper::DrawFloatControl(L({ "主半径", "Major Radius" }), majorRadius, 0.5f, kInspectorColumnWidth, 0.01f, 0.01f, 10.0f)) {
			SetTorusMajorRadius(majorRadius);
		 }
		 float minorRadius = GetTorusMinorRadius();
		 if (ImGuiHelper::DrawFloatControl(L({ "副半径", "Minor Radius" }), minorRadius, 0.2f, kInspectorColumnWidth, 0.01f, 0.01f, 10.0f)) {
			SetTorusMinorRadius(minorRadius);
		 }
		 int majorSegments = static_cast<int>(GetTorusMajorSegments());
		 if (ImGuiHelper::DrawIntControl(L({ "主分割数", "Major Segments" }), majorSegments, 32, kInspectorColumnWidth, 1.0f, 3, 128)) {
			SetTorusMajorSegments(static_cast<uint32_t>(std::max(majorSegments, 3)));
		 }
		 int minorSegments = static_cast<int>(GetTorusMinorSegments());
		 if (ImGuiHelper::DrawIntControl(L({ "副分割数", "Minor Segments" }), minorSegments, 16, kInspectorColumnWidth, 1.0f, 3, 128)) {
			SetTorusMinorSegments(static_cast<uint32_t>(std::max(minorSegments, 3)));
		 }
		 break;
	  }
	  default:
		 break;
   }
}

} // namespace GameEngine

#endif
