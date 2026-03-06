#include "CameraSequenceEditor.h"
#include "Framework/EngineContext.h"
#include <sstream>

#ifdef USE_IMGUI
#include "../../../externals/imgui/imgui.h"
#endif

using namespace GameEngine;

bool CameraSequenceEditor::LoadFromFile(const std::string& filePath, const std::string& groupName) {
   // JSONファイルを読み込み
   if (!GameEngine::EngineContext::LoadJsonFile(filePath)) {
	  return false;
   }

   // シーケンスタイプを読み込み
   auto typeStr = GameEngine::EngineContext::GetJsonData<std::string>(groupName, "SequenceType");
   if (typeStr.has_value()) {
	  if (typeStr.value() == "Keyframe") {
		 sequenceType_ = SequenceType::Keyframe;
	  } else if (typeStr.value() == "Orbital") {
		 sequenceType_ = SequenceType::Orbital;
	  }
   }

   // シーケンス名を読み込み
   auto name = GameEngine::EngineContext::GetJsonData<std::string>(groupName, "SequenceName");
   if (name.has_value()) {
	  sequenceName_ = name.value();
   }

   // シーケンスタイプに応じて読み込み
   if (sequenceType_ == SequenceType::Keyframe) {
	  // キーフレーム数を取得
	  auto keyframeCount = GameEngine::EngineContext::GetJsonData<int>(groupName, "KeyframeCount");
	  if (!keyframeCount.has_value()) {
		 return false;
	  }

	  // キーフレームを読み込み
	  keyframes_.clear();
	  for (int i = 0; i < keyframeCount.value(); ++i) {
		 keyframes_.push_back(JsonToKeyframe(groupName, i));
	  }
   } else if (sequenceType_ == SequenceType::Orbital) {
	  // 軌道パラメータを読み込み
	  orbitalParams_ = JsonToOrbitalParams(groupName);
   }

   return true;
}

bool CameraSequenceEditor::SaveToFile(const std::string& filePath, const std::string& groupName) {
   // 既存のグループをクリア
   GameEngine::EngineContext::ClearJsonGroup(groupName);

   // シーケンスタイプを保存
   std::string typeStr = (sequenceType_ == SequenceType::Keyframe) ? "Keyframe" : "Orbital";
   GameEngine::EngineContext::SetJsonData(groupName, "SequenceType", typeStr);

   // シーケンス名を保存
   GameEngine::EngineContext::SetJsonData(groupName, "SequenceName", sequenceName_);

   // シーケンスタイプに応じて保存
   if (sequenceType_ == SequenceType::Keyframe) {
	  // キーフレーム数を保存
	  GameEngine::EngineContext::SetJsonData(groupName, "KeyframeCount", static_cast<int>(keyframes_.size()));

	  // 各キーフレームを保存
	  for (size_t i = 0; i < keyframes_.size(); ++i) {
		 KeyframeToJson(keyframes_[i], groupName, i);
	  }
   } else if (sequenceType_ == SequenceType::Orbital) {
	  // 軌道パラメータを保存
	  OrbitalParamsToJson(orbitalParams_, groupName);
   }

   // ファイルに保存
   return GameEngine::EngineContext::SaveJsonFile(filePath, 4);
}

void CameraSequenceEditor::ApplyToSequencer(CameraSequencer* sequencer) {
   if (!sequencer || sequenceType_ != SequenceType::Keyframe) {
	  return;
   }

   sequencer->SetKeyframes(keyframes_);
}

void CameraSequenceEditor::ApplyToOrbitalController(OrbitalCameraController* controller) {
   if (!controller || sequenceType_ != SequenceType::Orbital) {
	  return;
   }

   controller->SetOrbitParams(orbitalParams_);
}

void CameraSequenceEditor::CaptureFromSequencer(const CameraSequencer* sequencer) {
   if (!sequencer) {
	  return;
   }

   sequenceType_ = SequenceType::Keyframe;
}

void CameraSequenceEditor::CaptureFromOrbitalController(const OrbitalCameraController* controller) {
   if (!controller) {
	  return;
   }

   sequenceType_ = SequenceType::Orbital;
}

void CameraSequenceEditor::ShowEditorWindow(GameEngine::Camera* camera) {
#ifdef USE_IMGUI
   ImGui::Begin("Camera Sequence Editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

   // シーケンス名
   char nameBuffer[256];
   strncpy_s(nameBuffer, sequenceName_.c_str(), sizeof(nameBuffer) - 1);
   if (ImGui::InputText("Sequence Name", nameBuffer, sizeof(nameBuffer))) {
	  sequenceName_ = nameBuffer;
   }

   // シーケンスタイプ選択
   const char* types[] = { "Keyframe", "Orbital" };
   int currentType = static_cast<int>(sequenceType_);
   if (ImGui::Combo("Sequence Type", &currentType, types, IM_ARRAYSIZE(types))) {
	  sequenceType_ = static_cast<SequenceType>(currentType);
   }

   ImGui::Separator();

   // ファイル操作
   static char filePathBuffer[512] = "resources/camera_sequences/sequence.json";
   ImGui::InputText("File Path", filePathBuffer, sizeof(filePathBuffer));

   if (ImGui::Button("Load")) {
	  if (LoadFromFile(filePathBuffer)) {
		 ImGui::Text("Loaded successfully!");
	  } else {
		 ImGui::Text("Failed to load!");
	  }
   }
   ImGui::SameLine();
   if (ImGui::Button("Save")) {
	  if (SaveToFile(filePathBuffer)) {
		 ImGui::Text("Saved successfully!");
	  } else {
		 ImGui::Text("Failed to save!");
	  }
   }
   ImGui::SameLine();
   if (ImGui::Button("Reload")) {
	  if (LoadFromFile(filePathBuffer)) {
		 if (camera) {
			if (sequenceType_ == SequenceType::Keyframe && previewSequencer_) {
			   ApplyToSequencer(previewSequencer_.get());
			} else if (sequenceType_ == SequenceType::Orbital && previewOrbital_) {
			   ApplyToOrbitalController(previewOrbital_.get());
			}
		 }
		 ImGui::Text("Reloaded successfully!");
	  }
   }

   ImGui::Separator();

   // エディター表示
   if (sequenceType_ == SequenceType::Keyframe) {
	  ShowKeyframeEditor(camera);
   } else if (sequenceType_ == SequenceType::Orbital) {
	  ShowOrbitalEditor(camera);
   }

   ImGui::End();
#endif
}

void CameraSequenceEditor::ShowKeyframeEditor(GameEngine::Camera* camera) {
#ifdef USE_IMGUI
   ImGui::Text("Keyframe Editor");
   ImGui::Text("Keyframe Count: %zu", keyframes_.size());

   // キーフレーム追加
   if (ImGui::Button("Add Keyframe")) {
	  CameraKeyframe newKeyframe;
	  newKeyframe.position = Vector3(0, 5, -20);
	  newKeyframe.rotation = Vector3(0, 0, 0);
	  newKeyframe.fov = 0.45f;
	  newKeyframe.duration = 3.0f;
	  newKeyframe.easingType = CameraKeyframe::EasingType::Linear;
	  keyframes_.push_back(newKeyframe);
   }

   ImGui::Separator();

   // キーフレームリスト
   for (size_t i = 0; i < keyframes_.size(); ++i) {
	  ImGui::PushID(static_cast<int>(i));
	  
	  bool nodeOpen = ImGui::TreeNode(("Keyframe " + std::to_string(i)).c_str());
	  
	  ImGui::SameLine();
	  if (ImGui::SmallButton("X")) {
		 RemoveKeyframe(i);
		 ImGui::PopID();
		 break;
	  }

	  if (nodeOpen) {
		 auto& kf = keyframes_[i];

		 // 位置
		 ImGui::DragFloat3("Position", &kf.position.x, 0.1f);
		 
		 // 回転
		 ImGui::DragFloat3("Rotation", &kf.rotation.x, 0.01f);
		 
		 // FOV
		 ImGui::SliderFloat("FOV", &kf.fov, 0.1f, 1.5f);
		 
		 // 時間
		 ImGui::DragFloat("Duration", &kf.duration, 0.1f, 0.1f, 60.0f);
		 
		 // イージング
		 const char* easingTypes[] = { "Linear", "EaseIn", "EaseOut", "EaseInOut", "Bounce", "EaseOutBack" };
		 int currentEasing = static_cast<int>(kf.easingType);
		 if (ImGui::Combo("Easing Type", &currentEasing, easingTypes, IM_ARRAYSIZE(easingTypes))) {
			kf.easingType = static_cast<CameraKeyframe::EasingType>(currentEasing);
		 }
		 
		 ImGui::DragFloat("Easing Power", &kf.easingPower, 0.1f, 0.5f, 5.0f);
		 
		 // フェード
		 ImGui::Checkbox("Use Fade", &kf.useFade);
		 if (kf.useFade) {
			ImGui::DragFloat("Fade Duration", &kf.fadeDuration, 0.1f, 0.1f, 5.0f);
			
			// フェード色（RGBA）
			float color[4] = {
			   ((kf.fadeColor >> 24) & 0xFF) / 255.0f,
			   ((kf.fadeColor >> 16) & 0xFF) / 255.0f,
			   ((kf.fadeColor >> 8) & 0xFF) / 255.0f,
			   (kf.fadeColor & 0xFF) / 255.0f
			};
			if (ImGui::ColorEdit4("Fade Color", color)) {
			   kf.fadeColor = 
				  (static_cast<uint32_t>(color[0] * 255) << 24) |
				  (static_cast<uint32_t>(color[1] * 255) << 16) |
				  (static_cast<uint32_t>(color[2] * 255) << 8) |
				  static_cast<uint32_t>(color[3] * 255);
			}
		 }

		 ImGui::TreePop();
	  }

	  ImGui::PopID();
   }

   // プレビュー
   if (camera) {
	  ImGui::Separator();
	  ImGui::Checkbox("Show Preview", &showPreview_);
	  
	  if (showPreview_) {
		 if (!previewSequencer_) {
			previewSequencer_ = std::make_unique<CameraSequencer>(camera);
			ApplyToSequencer(previewSequencer_.get());
		 }

		 if (ImGui::Button("Play Preview")) {
			ApplyToSequencer(previewSequencer_.get());
			previewSequencer_->Play(true);  // ループ再生
		 }
		 ImGui::SameLine();
		 if (ImGui::Button("Stop Preview")) {
			previewSequencer_->Stop();
		 }

		 if (previewSequencer_->IsPlaying()) {
			previewSequencer_->Update();
			ImGui::Text("Playing... (%.1f/%.1f)", 
					   previewSequencer_->GetElapsedTime(),
					   previewSequencer_->GetTotalDuration());
		 }
	  }
   }
#endif
}

void CameraSequenceEditor::ShowOrbitalEditor(GameEngine::Camera* camera) {
#ifdef USE_IMGUI
   ImGui::Text("Orbital Camera Editor");

   // 注視点
   ImGui::DragFloat3("Target Position", &orbitalParams_.targetPosition.x, 0.1f);
   
   // 半径
   ImGui::DragFloat("Radius", &orbitalParams_.radius, 0.5f, 1.0f, 100.0f);
   
   // 開始角度（Y軸）
   ImGui::SliderAngle("Start Angle Y", &orbitalParams_.startAngleY, 0.0f, 360.0f);
   
   // 終了角度（Y軸）
   ImGui::SliderAngle("End Angle Y", &orbitalParams_.endAngleY, 0.0f, 360.0f);
   
   // 開始角度（X軸）
   ImGui::SliderAngle("Start Angle X", &orbitalParams_.startAngleX, -90.0f, 90.0f);
   
   // 終了角度（X軸）
   ImGui::SliderAngle("End Angle X", &orbitalParams_.endAngleX, -90.0f, 90.0f);
   
   // 時間
   ImGui::DragFloat("Duration", &orbitalParams_.duration, 0.5f, 1.0f, 120.0f);
   
   // イージング
   const char* easingTypes[] = { "Linear", "EaseIn", "EaseOut", "EaseInOut", "Bounce", "EaseOutBack" };
   int currentEasing = static_cast<int>(orbitalParams_.easingType);
   if (ImGui::Combo("Easing Type", &currentEasing, easingTypes, IM_ARRAYSIZE(easingTypes))) {
	  orbitalParams_.easingType = static_cast<CameraKeyframe::EasingType>(currentEasing);
   }
   
   ImGui::DragFloat("Easing Power", &orbitalParams_.easingPower, 0.1f, 0.5f, 5.0f);
   
   // FOV
   ImGui::SliderFloat("FOV", &orbitalParams_.fov, 0.1f, 1.5f);
   
   // 注視設定
   ImGui::Checkbox("Look at Target", &orbitalParams_.lookAtTarget);

   // プレビュー
   if (camera) {
	  ImGui::Separator();
	  ImGui::Checkbox("Show Preview", &showPreview_);
	  
	  if (showPreview_) {
		 if (!previewOrbital_) {
			previewOrbital_ = std::make_unique<OrbitalCameraController>(camera);
			ApplyToOrbitalController(previewOrbital_.get());
		 }

		 if (ImGui::Button("Play Preview")) {
			ApplyToOrbitalController(previewOrbital_.get());
			previewOrbital_->Start(true);  // ループ再生
		 }
		 ImGui::SameLine();
		 if (ImGui::Button("Stop Preview")) {
			previewOrbital_->Stop();
		 }

		 if (previewOrbital_->IsMoving()) {
			previewOrbital_->Update();
			ImGui::Text("Playing... (%.1f%%)", previewOrbital_->GetProgress() * 100.0f);
		 }
	  }
   }
#endif
}

void CameraSequenceEditor::AddKeyframe(const CameraKeyframe& keyframe) {
   keyframes_.push_back(keyframe);
}

void CameraSequenceEditor::RemoveKeyframe(size_t index) {
   if (index < keyframes_.size()) {
	  keyframes_.erase(keyframes_.begin() + index);
   }
}

CameraKeyframe* CameraSequenceEditor::GetKeyframe(size_t index) {
   if (index < keyframes_.size()) {
	  return &keyframes_[index];
   }
   return nullptr;
}

void CameraSequenceEditor::KeyframeToJson(const CameraKeyframe& keyframe, const std::string& groupName, size_t index) {
   std::string prefix = "Keyframe" + std::to_string(index) + "_";

   GameEngine::EngineContext::SetJsonData(groupName, prefix + "PositionX", keyframe.position.x);
   GameEngine::EngineContext::SetJsonData(groupName, prefix + "PositionY", keyframe.position.y);
   GameEngine::EngineContext::SetJsonData(groupName, prefix + "PositionZ", keyframe.position.z);
   
   GameEngine::EngineContext::SetJsonData(groupName, prefix + "RotationX", keyframe.rotation.x);
   GameEngine::EngineContext::SetJsonData(groupName, prefix + "RotationY", keyframe.rotation.y);
   GameEngine::EngineContext::SetJsonData(groupName, prefix + "RotationZ", keyframe.rotation.z);
   
   GameEngine::EngineContext::SetJsonData(groupName, prefix + "FOV", keyframe.fov);
   GameEngine::EngineContext::SetJsonData(groupName, prefix + "Duration", keyframe.duration);
   GameEngine::EngineContext::SetJsonData(groupName, prefix + "EasingType", EasingTypeToString(keyframe.easingType));
   GameEngine::EngineContext::SetJsonData(groupName, prefix + "EasingPower", keyframe.easingPower);
   
   GameEngine::EngineContext::SetJsonData(groupName, prefix + "UseFade", keyframe.useFade);
   GameEngine::EngineContext::SetJsonData(groupName, prefix + "FadeDuration", keyframe.fadeDuration);
   GameEngine::EngineContext::SetJsonData(groupName, prefix + "FadeColor", static_cast<int>(keyframe.fadeColor));
}

CameraKeyframe CameraSequenceEditor::JsonToKeyframe(const std::string& groupName, size_t index) {
   std::string prefix = "Keyframe" + std::to_string(index) + "_";

   CameraKeyframe keyframe;
   
   keyframe.position.x = GameEngine::EngineContext::GetJsonDataOr(groupName, prefix + "PositionX", 0.0f);
   keyframe.position.y = GameEngine::EngineContext::GetJsonDataOr(groupName, prefix + "PositionY", 0.0f);
   keyframe.position.z = GameEngine::EngineContext::GetJsonDataOr(groupName, prefix + "PositionZ", 0.0f);
   
   keyframe.rotation.x = GameEngine::EngineContext::GetJsonDataOr(groupName, prefix + "RotationX", 0.0f);
   keyframe.rotation.y = GameEngine::EngineContext::GetJsonDataOr(groupName, prefix + "RotationY", 0.0f);
   keyframe.rotation.z = GameEngine::EngineContext::GetJsonDataOr(groupName, prefix + "RotationZ", 0.0f);
   
   keyframe.fov = GameEngine::EngineContext::GetJsonDataOr(groupName, prefix + "FOV", 0.45f);
   keyframe.duration = GameEngine::EngineContext::GetJsonDataOr(groupName, prefix + "Duration", 3.0f);
   
   auto easingStr = GameEngine::EngineContext::GetJsonDataOr(groupName, prefix + "EasingType", std::string("Linear"));
   keyframe.easingType = StringToEasingType(easingStr);
   
   keyframe.easingPower = GameEngine::EngineContext::GetJsonDataOr(groupName, prefix + "EasingPower", 2.0f);
   keyframe.useFade = GameEngine::EngineContext::GetJsonDataOr(groupName, prefix + "UseFade", false);
   keyframe.fadeDuration = GameEngine::EngineContext::GetJsonDataOr(groupName, prefix + "FadeDuration", 1.0f);
   
   auto fadeColor = GameEngine::EngineContext::GetJsonDataOr(groupName, prefix + "FadeColor", static_cast<int>(0x000000ff));
   keyframe.fadeColor = static_cast<uint32_t>(fadeColor);

   return keyframe;
}

void CameraSequenceEditor::OrbitalParamsToJson(const OrbitalCameraController::OrbitParams& params, const std::string& groupName) {
   GameEngine::EngineContext::SetJsonData(groupName, "Orbital_TargetX", params.targetPosition.x);
   GameEngine::EngineContext::SetJsonData(groupName, "Orbital_TargetY", params.targetPosition.y);
   GameEngine::EngineContext::SetJsonData(groupName, "Orbital_TargetZ", params.targetPosition.z);
   GameEngine::EngineContext::SetJsonData(groupName, "Orbital_Radius", params.radius);
   GameEngine::EngineContext::SetJsonData(groupName, "Orbital_StartAngleY", params.startAngleY);
   GameEngine::EngineContext::SetJsonData(groupName, "Orbital_EndAngleY", params.endAngleY);
   GameEngine::EngineContext::SetJsonData(groupName, "Orbital_StartAngleX", params.startAngleX);
   GameEngine::EngineContext::SetJsonData(groupName, "Orbital_EndAngleX", params.endAngleX);
   GameEngine::EngineContext::SetJsonData(groupName, "Orbital_Duration", params.duration);
   GameEngine::EngineContext::SetJsonData(groupName, "Orbital_EasingType", EasingTypeToString(params.easingType));
   GameEngine::EngineContext::SetJsonData(groupName, "Orbital_EasingPower", params.easingPower);
   GameEngine::EngineContext::SetJsonData(groupName, "Orbital_FOV", params.fov);
   GameEngine::EngineContext::SetJsonData(groupName, "Orbital_LookAtTarget", params.lookAtTarget);
}

OrbitalCameraController::OrbitParams CameraSequenceEditor::JsonToOrbitalParams(const std::string& groupName) {
   OrbitalCameraController::OrbitParams params;
   
   params.targetPosition.x = GameEngine::EngineContext::GetJsonDataOr(groupName, "Orbital_TargetX", 0.0f);
   params.targetPosition.y = GameEngine::EngineContext::GetJsonDataOr(groupName, "Orbital_TargetY", 0.0f);
   params.targetPosition.z = GameEngine::EngineContext::GetJsonDataOr(groupName, "Orbital_TargetZ", 0.0f);
   params.radius = GameEngine::EngineContext::GetJsonDataOr(groupName, "Orbital_Radius", 10.0f);
   params.startAngleY = GameEngine::EngineContext::GetJsonDataOr(groupName, "Orbital_StartAngleY", 0.0f);
   params.endAngleY = GameEngine::EngineContext::GetJsonDataOr(groupName, "Orbital_EndAngleY", 6.28318f);
   params.startAngleX = GameEngine::EngineContext::GetJsonDataOr(groupName, "Orbital_StartAngleX", 0.0f);
   params.endAngleX = GameEngine::EngineContext::GetJsonDataOr(groupName, "Orbital_EndAngleX", 0.0f);
   params.duration = GameEngine::EngineContext::GetJsonDataOr(groupName, "Orbital_Duration", 5.0f);
   
   auto easingStr = GameEngine::EngineContext::GetJsonDataOr(groupName, "Orbital_EasingType", std::string("Linear"));
   params.easingType = StringToEasingType(easingStr);
   
   params.easingPower = GameEngine::EngineContext::GetJsonDataOr(groupName, "Orbital_EasingPower", 2.0f);
   params.fov = GameEngine::EngineContext::GetJsonDataOr(groupName, "Orbital_FOV", 0.45f);
   params.lookAtTarget = GameEngine::EngineContext::GetJsonDataOr(groupName, "Orbital_LookAtTarget", true);

   return params;
}

std::string CameraSequenceEditor::EasingTypeToString(CameraKeyframe::EasingType type) {
   switch (type) {
	  case CameraKeyframe::EasingType::Linear: return "Linear";
	  case CameraKeyframe::EasingType::EaseIn: return "EaseIn";
	  case CameraKeyframe::EasingType::EaseOut: return "EaseOut";
	  case CameraKeyframe::EasingType::EaseInOut: return "EaseInOut";
	  case CameraKeyframe::EasingType::Bounce: return "Bounce";
	  case CameraKeyframe::EasingType::EaseOutBack: return "EaseOutBack";
	  default: return "Linear";
   }
}

CameraKeyframe::EasingType CameraSequenceEditor::StringToEasingType(const std::string& str) {
   if (str == "Linear") return CameraKeyframe::EasingType::Linear;
   if (str == "EaseIn") return CameraKeyframe::EasingType::EaseIn;
   if (str == "EaseOut") return CameraKeyframe::EasingType::EaseOut;
   if (str == "EaseInOut") return CameraKeyframe::EasingType::EaseInOut;
   if (str == "Bounce") return CameraKeyframe::EasingType::Bounce;
   if (str == "EaseOutBack") return CameraKeyframe::EasingType::EaseOutBack;
   return CameraKeyframe::EasingType::Linear;
}
