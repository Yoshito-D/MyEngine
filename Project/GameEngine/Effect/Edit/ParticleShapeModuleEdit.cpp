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

   constexpr float kPi = 3.14159265358979323846f;

   bool enabled = shapeModule->IsEnabled();
   if (ImGui::Checkbox("Enabled##Shape", &enabled)) {
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
	  if (ImGui::Combo("Shape Type", &currentShapeType, shapeTypeNames, IM_ARRAYSIZE(shapeTypeNames))) {
		 shapeModule->SetShapeType(static_cast<GameEngine::ShapeModule::ShapeType>(currentShapeType));
	  }

	  // Emit From
	  static const char* emitFromNames[] = {
		  "Volume",
		  "Shell",
		  "Edge"
	  };

	  int currentEmitFrom = static_cast<int>(shapeModule->GetEmitFrom());
	  if (ImGui::Combo("Emit From", &currentEmitFrom, emitFromNames, IM_ARRAYSIZE(emitFromNames))) {
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
			if (ImGui::DragFloat("Angle", &angle, 1.0f, 0.0f, 90.0f)) {
			   shapeModule->SetAngle(angle);
			}

			float radius = shapeModule->GetRadius();
			if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 100.0f)) {
			   shapeModule->SetRadius(radius);
			}

			float length = shapeModule->GetLength();
			if (ImGui::DragFloat("Length", &length, 0.1f, 0.1f, 100.0f)) {
			   shapeModule->SetLength(length);
			}
			break;
		 }

		 case GameEngine::ShapeModule::ShapeType::Box: {
			Vector3 boxSize = shapeModule->GetBoxSize();
			float size[3] = { boxSize.x, boxSize.y, boxSize.z };
			if (ImGui::DragFloat3("Box Size", size, 0.1f, 0.1f, 100.0f)) {
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
			if (ImGui::DragFloat("Arc", &arc, 1.0f, 0.0f, 360.0f)) {
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
	  if (ImGui::DragFloat3("Position", pos, 0.1f)) {
		 shapeModule->SetPosition(Vector3(pos[0], pos[1], pos[2]));
	  }

	  Vector3 rotation = shapeModule->GetRotation();
	  float rot[3] = { rotation.x, rotation.y, rotation.z };
	  if (ImGui::DragFloat3("Rotation", rot, 1.0f)) {
		 shapeModule->SetRotation(Vector3(rot[0], rot[1], rot[2]));
	  }

	  Vector3 scale = shapeModule->GetScale();
	  float scl[3] = { scale.x, scale.y, scale.z };
	  if (ImGui::DragFloat3("Scale", scl, 0.1f, 0.1f, 10.0f)) {
		 shapeModule->SetScale(Vector3(scl[0], scl[1], scl[2]));
	  }

	  ImGui::Separator();

	  // Shape Visualization
	  static bool showShape = true;
	  static Vector4 shapeColor(1.0f, 1.0f, 0.0f, 1.0f);

	  ImGui::Checkbox("Show Shape", &showShape);

	  if (showShape) {
		 ImGui::ColorEdit4("Shape Color", &shapeColor.x);

		 Vector3 center = shapeModule->GetPosition();
		 Vector3 scaleVec = shapeModule->GetScale();
		 Vector3 rotationVec = shapeModule->GetRotation();

		 switch (shapeType) {
			case GameEngine::ShapeModule::ShapeType::Sphere: {
			   float scaledRadius = shapeModule->GetRadius() * scaleVec.x;
			   GameEngine::EngineContext::DrawSphere(center, scaledRadius, shapeColor);
			   break;
			}

			case GameEngine::ShapeModule::ShapeType::Hemisphere: {
			   float scaledRadius = shapeModule->GetRadius() * scaleVec.x;
			   Vector3 up(0.0f, 1.0f, 0.0f);
			   GameEngine::EngineContext::DrawHemisphere(center, scaledRadius, up, shapeColor);
			   break;
			}

			case GameEngine::ShapeModule::ShapeType::Cone: {
			   // Coneの角度とスケールを反映
			   float angle = shapeModule->GetAngle();
			   float length = shapeModule->GetLength() * scaleVec.y;
			   float radius = std::tan(angle * kPi / 180.0f) * length;

			   Vector3 direction(0.0f, 1.0f, 0.0f);
			   GameEngine::EngineContext::DrawCone(center, radius, length, direction, shapeColor);
			   break;
			}

			case GameEngine::ShapeModule::ShapeType::Box: {
			   Vector3 boxSize = shapeModule->GetBoxSize();
			   Vector3 scaledSize(
				  boxSize.x * scaleVec.x,
				  boxSize.y * scaleVec.y,
				  boxSize.z * scaleVec.z
			   );
			   GameEngine::EngineContext::DrawBox(center, scaledSize, shapeColor);
			   break;
			}

			case GameEngine::ShapeModule::ShapeType::Circle: {
			   float scaledRadius = shapeModule->GetRadius() * scaleVec.x;
			   float arc = shapeModule->GetArc();

			   // Arcに対応した円の描画
			   if (arc >= 360.0f) {
				  // 完全な円
				  Vector3 normal(0.0f, 1.0f, 0.0f);
				  GameEngine::EngineContext::DrawCircle(center, scaledRadius, normal, shapeColor);
			   } else {
				  // 円弧を線分で描画
				  const int segments = 32;
				  float angleStep = (arc * kPi / 180.0f) / segments;
				  float startAngle = -arc * 0.5f * kPi / 180.0f;

				  Vector3 right(1.0f, 0.0f, 0.0f);
				  Vector3 forward(0.0f, 0.0f, 1.0f);

				  // 中心から放射状の線
				  for (int i = 0; i <= segments; ++i) {
					 float angle = startAngle + angleStep * i;
					 Vector3 p = center +
						right * (scaledRadius * std::cos(angle)) +
						forward * (scaledRadius * std::sin(angle));

					 if (i == 0 || i == segments) {
						// 端点は中心からの線も引く
						GameEngine::EngineContext::DrawLine(center, p, shapeColor);
					 }

					 if (i > 0) {
						// 前の点との接続
						float prevAngle = startAngle + angleStep * (i - 1);
						Vector3 prevP = center +
						   right * (scaledRadius * std::cos(prevAngle)) +
						   forward * (scaledRadius * std::sin(prevAngle));
						GameEngine::EngineContext::DrawLine(prevP, p, shapeColor);
					 }
				  }
			   }
			   break;
			}

			case GameEngine::ShapeModule::ShapeType::Edge:
			case GameEngine::ShapeModule::ShapeType::Point:
			   // 点とエッジは小さい球で表示
			   GameEngine::EngineContext::DrawSphere(center, 0.1f, shapeColor);
			   break;
		 }
	  }
   }
#endif
}

}
