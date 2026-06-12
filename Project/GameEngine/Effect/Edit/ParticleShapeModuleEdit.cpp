#include "pch.h"
#include "ParticleShapeModuleEdit.h"
#include "Effect/Module/ShapeModule.h"
#include "Framework/EngineContext.h"
#include <numbers>

#ifdef USE_IMGUI
#include "../../../externals/imgui/imgui.h"
#endif

using namespace GameEngine;

namespace ParticleSystemEdit {

void EditShapeModule(GameEngine::ShapeModule* shapeModule) {
#ifdef USE_IMGUI
   if (!shapeModule) return;

   //constexpr float kPi = 3.14159265358979323846f;

   bool enabled = shapeModule->IsEnabled();
   if (ImGui::Checkbox("Enabled (有効)##Shape", &enabled)) {
	  shapeModule->SetEnabled(enabled);
   }

   if (enabled) {
	  // Shape Type
	  static const char* shapeTypeNames[] = {
		  "Sphere",
		  "Hemisphere",
		  "Cone",
		  "Box",
		  "Circle",
		  "Edge",
		  "Point"
	  };

	  int currentShapeType = static_cast<int>(shapeModule->GetShapeType());
	  if (ImGui::Combo("Shape Type (形状タイプ)", &currentShapeType, shapeTypeNames, IM_ARRAYSIZE(shapeTypeNames))) {
		 shapeModule->SetShapeType(static_cast<GameEngine::ShapeModule::ShapeType>(currentShapeType));
	  }

	  // Emit From
	  static const char* emitFromNames[] = {
		  "Volume",
		  "Shell",
		  "Edge"
	  };

	  int currentEmitFrom = static_cast<int>(shapeModule->GetEmitFrom());
	  if (ImGui::Combo("Emit From (放出元)", &currentEmitFrom, emitFromNames, IM_ARRAYSIZE(emitFromNames))) {
		 shapeModule->SetEmitFrom(static_cast<GameEngine::ShapeModule::EmitFrom>(currentEmitFrom));
	  }

	  ImGui::Separator();

	  // Shape-specific parameters
	  auto shapeType = shapeModule->GetShapeType();

	  switch (shapeType) {
		 case GameEngine::ShapeModule::ShapeType::Sphere:
		 case GameEngine::ShapeModule::ShapeType::Hemisphere: {
			float radius = shapeModule->GetRadius();
			if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 100.0f)) {
			   shapeModule->SetRadius(radius);
			}
			break;
		 }

		 case GameEngine::ShapeModule::ShapeType::Cone: {
			float angle = shapeModule->GetAngle();
			if (ImGui::DragFloat("Angle (角度)", &angle, 1.0f, 0.0f, 90.0f)) {
			   shapeModule->SetAngle(angle);
			}

			float radius = shapeModule->GetRadius();
			if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 100.0f)) {
			   shapeModule->SetRadius(radius);
			}

			float length = shapeModule->GetLength();
			if (ImGui::DragFloat("Length (長さ)", &length, 0.1f, 0.1f, 100.0f)) {
			   shapeModule->SetLength(length);
			}
			break;
		 }

		 case GameEngine::ShapeModule::ShapeType::Box: {
			Vector3 boxSize = shapeModule->GetBoxSize();
			float size[3] = { boxSize.x, boxSize.y, boxSize.z };
			if (ImGui::DragFloat3("Box Size (箱サイズ)", size, 0.1f, 0.1f, 100.0f)) {
			   shapeModule->SetBoxSize(Vector3(size[0], size[1], size[2]));
			}
			break;
		 }

		 case GameEngine::ShapeModule::ShapeType::Circle: {
			float radius = shapeModule->GetRadius();
			if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 100.0f)) {
			   shapeModule->SetRadius(radius);
			}

			float arc = shapeModule->GetArc();
			if (ImGui::DragFloat("Arc (円弧)", &arc, 1.0f, 0.0f, 360.0f)) {
			   shapeModule->SetArc(arc);
			}
			break;
		 }

		 default:
			break;
	  }

	  ImGui::Separator();

	  // Position, Rotation, Scale
	  Vector3 position = shapeModule->GetPosition();
	  float pos[3] = { position.x, position.y, position.z };
	  if (ImGui::DragFloat3("Position (位置)", pos, 0.1f)) {
		 shapeModule->SetPosition(Vector3(pos[0], pos[1], pos[2]));
	  }

	  Quaternion rotation = shapeModule->GetRotationQuaternion();
	  Vector3 euler = rotation.ToEuler();
	  float rot[3] = { euler.x, euler.y, euler.z };
	  // ImGuiのDragFloat3はオイラー角で回転を編集するため、クォータニオンをオイラー角に変換して表示
	  // Degreesで表示する場合は、以下のように変換
	  rot[0] = rot[0] * 180.0f / 3.14159265358979323846f;
	  rot[1] = rot[1] * 180.0f / 3.14159265358979323846f;
	  rot[2] = rot[2] * 180.0f / 3.14159265358979323846f;
	  if (ImGui::DragFloat3("Rotation (回転)", rot, 1.0f)) {
		 // 編集後は再度クォータニオンに変換して保存
		 rot[0] = rot[0] * 3.14159265358979323846f / 180.0f;
		 rot[1] = rot[1] * 3.14159265358979323846f / 180.0f;
		 rot[2] = rot[2] * 3.14159265358979323846f / 180.0f;

		 shapeModule->SetRotation(Vector3(rot[0], rot[1], rot[2]).ToQuaternion());
	  }

	  Vector3 scale = shapeModule->GetScale();
	  float scl[3] = { scale.x, scale.y, scale.z };
	  if (ImGui::DragFloat3("Scale (拡縮)", scl, 0.1f, 0.1f, 10.0f)) {
		 shapeModule->SetScale(Vector3(scl[0], scl[1], scl[2]));
	  }

	  ImGui::Separator();

   }
#endif
}

}
