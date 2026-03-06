#include "LevelEditor.h"
#include "Framework/EngineContext.h"
#include "Utility/JsonDataManager.h"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include "../../../externals/nlohmann/json.hpp"

#ifdef USE_IMGUI
#include "../../../externals/imgui/imgui.h"
#endif

using namespace GameEngine;
using json = nlohmann::json;

LevelEditor::LevelEditor()
   : enabled_(true)
   , dataChanged_(false)
   , debugDrawEnabled_(true)
   , lastFileWriteTime_(0)
   , drawPlanetSpheres_(true)
   , drawGravityRadius_(true)
   , drawOrbitPaths_(true)
   , drawLabels_(true)
   , drawRabbits_(true)
   , drawStar_(true)
   , drawBoxes_(true) {
   
   // ファイルパスバッファを初期化
   strcpy_s(filePathBuffer_, "resources/levels/default_level.json");
   
   // デフォルトの新規データを初期化
   newPlanetData_.position = Vector3(0.0f, 0.0f, 0.0f);
   newPlanetData_.planetRadius = 5.0f;
   newPlanetData_.gravitationalRadius = 25.0f;
   newPlanetData_.id = 0;
   newPlanetData_.type = PlanetType::Static;
   newPlanetData_.orbitCenter = Vector3(0.0f, 0.0f, 0.0f);
   newPlanetData_.orbitRadius = 10.0f;
   newPlanetData_.orbitSpeed = 1.0f;
   newPlanetData_.orbitAxis = Vector3(0.0f, 1.0f, 0.0f);
   newPlanetData_.waveDirection = Vector3(1.0f, 0.0f, 0.0f);
   newPlanetData_.waveAmplitude = 5.0f;
   newPlanetData_.pendulumAngle = 45.0f;
   
   newRabbitData_.planetId = 0;
   newRabbitData_.offset = Vector3(0.0f, 1.0f, 0.0f);
   newRabbitData_.radius = 0.3f;
   
   newBoxData_.planetId = 0;
   newBoxData_.offset = Vector3(0.0f, 1.0f, 0.0f);
   newBoxData_.size = 0.8f;
   newBoxData_.mass = 1.0f;
   
   // スターデータの初期化
   levelData_.star.planetId = 0;
   levelData_.star.offset = Vector3(0.0f, 1.0f, 0.0f);
   levelData_.star.radius = 0.5f;
   
   // プレイヤーデータの初期化
   levelData_.player.planetId = 0;
   levelData_.player.offset = Vector3(0.0f, 1.0f, 0.0f);
}

LevelEditor::~LevelEditor() {}

void LevelEditor::ShowEditor() {
#ifdef USE_IMGUI
   if (!enabled_) {
	  return;
   }

   ImGui::Begin("Level Editor", &enabled_);
   
   if (ImGui::BeginTabBar("EditorTabs")) {
	  if (ImGui::BeginTabItem("Planets")) {
		 ShowPlanetEditor();
		 ImGui::EndTabItem();
	  }
	  
	  if (ImGui::BeginTabItem("Rabbits")) {
		 ShowRabbitEditor();
		 ImGui::EndTabItem();
	  }
	  
	  if (ImGui::BeginTabItem("Star")) {
		 ShowStarEditor();
		 ImGui::EndTabItem();
	  }
	  
	  if (ImGui::BeginTabItem("Boxes")) {
		 ShowBoxEditor();
		 ImGui::EndTabItem();
	  }
	  
	  if (ImGui::BeginTabItem("Player")) {
		 ShowPlayerEditor();
		 ImGui::EndTabItem();
	  }
	  
	  if (ImGui::BeginTabItem("File")) {
		 ShowFileOperations();
		 ImGui::EndTabItem();
	  }
	  
	  if (ImGui::BeginTabItem("Debug Draw")) {
		 ShowDebugDrawSettings();
		 ImGui::EndTabItem();
	  }
	  
	  ImGui::EndTabBar();
   }
   
   ImGui::End();
#endif
}

void LevelEditor::ShowPlanetEditor() {
#ifdef USE_IMGUI
   ImGui::Text("Planets: %zu", levelData_.planets.size());
   ImGui::Separator();
   
   // 既存の惑星リスト
   if (ImGui::TreeNode("Planet List")) {
	  for (size_t i = 0; i < levelData_.planets.size(); ++i) {
		 auto& planet = levelData_.planets[i];
		 
		 ImGui::PushID(static_cast<int>(i));
		 
		 std::string label = "Planet " + std::to_string(planet.id);
		 if (ImGui::TreeNode(label.c_str())) {
			bool changed = false;
			
			changed |= ImGui::DragFloat3("Position", &planet.position.x, 0.1f);
			changed |= ImGui::DragFloat("Planet Radius", &planet.planetRadius, 0.1f, 0.1f, 100.0f);
			changed |= ImGui::DragFloat("Gravity Radius", &planet.gravitationalRadius, 0.1f, 0.1f, 100.0f);
			ImGui::Text("ID: %d", planet.id);
			
			// 惑星の種類
			const char* planetTypes[] = { "Static", "Orbit", "Pendulum", "Wave" };
			int currentType = static_cast<int>(planet.type);
			if (ImGui::Combo("Type", &currentType, planetTypes, 4)) {
			   planet.type = static_cast<PlanetType>(currentType);
			   changed = true;
			}
			
			// 種類に応じたパラメータ表示
			if (planet.type == PlanetType::Orbit) {
			   ImGui::Separator();
			   ImGui::Text("Orbit Parameters");
			   changed |= ImGui::DragFloat3("Orbit Center", &planet.orbitCenter.x, 0.1f);
			   changed |= ImGui::DragFloat("Orbit Radius", &planet.orbitRadius, 0.1f, 0.1f, 100.0f);
			   changed |= ImGui::DragFloat("Orbit Speed", &planet.orbitSpeed, 0.01f, 0.01f, 10.0f);
			   changed |= ImGui::DragFloat3("Orbit Axis", &planet.orbitAxis.x, 0.01f);
			} else if (planet.type == PlanetType::Pendulum) {
			   ImGui::Separator();
			   ImGui::Text("Pendulum Parameters");
			   changed |= ImGui::DragFloat3("Pendulum Center", &planet.orbitCenter.x, 0.1f);
			   changed |= ImGui::DragFloat("Pendulum Radius", &planet.orbitRadius, 0.1f, 0.1f, 100.0f);
			   changed |= ImGui::DragFloat("Pendulum Angle", &planet.pendulumAngle, 1.0f, 0.0f, 180.0f);
			   changed |= ImGui::DragFloat("Swing Speed", &planet.orbitSpeed, 0.01f, 0.01f, 10.0f);
			} else if (planet.type == PlanetType::Wave) {
			   ImGui::Separator();
			   ImGui::Text("Wave Parameters");
			   changed |= ImGui::DragFloat3("Wave Direction", &planet.waveDirection.x, 0.01f);
			   changed |= ImGui::DragFloat("Wave Amplitude", &planet.waveAmplitude, 0.1f, 0.1f, 50.0f);
			   changed |= ImGui::DragFloat("Wave Speed", &planet.orbitSpeed, 0.01f, 0.01f, 10.0f);
			}
			
			if (changed) {
			   dataChanged_ = true;
			}
			
			ImGui::Separator();
			if (ImGui::Button("Delete")) {
			   levelData_.planets.erase(levelData_.planets.begin() + i);
			   dataChanged_ = true;
			   ImGui::TreePop();
			   ImGui::PopID();
			   break;
			}
			
			ImGui::TreePop();
		 }
		 
		 ImGui::PopID();
	  }
	  ImGui::TreePop();
   }
   
   ImGui::Separator();
   
   // 新規惑星追加
   if (ImGui::TreeNode("Add New Planet")) {
	  ImGui::DragFloat3("Position", &newPlanetData_.position.x, 0.1f);
	  ImGui::DragFloat("Planet Radius", &newPlanetData_.planetRadius, 0.1f, 0.1f, 100.0f);
	  ImGui::DragFloat("Gravity Radius", &newPlanetData_.gravitationalRadius, 0.1f, 0.1f, 100.0f);
	  
	  // 惑星の種類
	  const char* planetTypes[] = { "Static", "Orbit", "Pendulum", "Wave" };
	  int currentType = static_cast<int>(newPlanetData_.type);
	  if (ImGui::Combo("Type", &currentType, planetTypes, 4)) {
		 newPlanetData_.type = static_cast<PlanetType>(currentType);
	  }
	  
	  // 種類に応じたパラメータ表示
	  if (newPlanetData_.type == PlanetType::Orbit) {
		 ImGui::Separator();
		 ImGui::Text("Orbit Parameters");
		 ImGui::DragFloat3("Orbit Center", &newPlanetData_.orbitCenter.x, 0.1f);
		 ImGui::DragFloat("Orbit Radius", &newPlanetData_.orbitRadius, 0.1f, 0.1f, 100.0f);
		 ImGui::DragFloat("Orbit Speed", &newPlanetData_.orbitSpeed, 0.01f, 0.01f, 10.0f);
		 ImGui::DragFloat3("Orbit Axis", &newPlanetData_.orbitAxis.x, 0.01f);
		 ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Position is initial point on orbit");
	  } else if (newPlanetData_.type == PlanetType::Pendulum) {
		 ImGui::Separator();
		 ImGui::Text("Pendulum Parameters");
		 ImGui::DragFloat3("Pendulum Center", &newPlanetData_.orbitCenter.x, 0.1f);
		 ImGui::DragFloat("Pendulum Radius", &newPlanetData_.orbitRadius, 0.1f, 0.1f, 100.0f);
		 ImGui::DragFloat("Pendulum Angle", &newPlanetData_.pendulumAngle, 1.0f, 0.0f, 180.0f);
		 ImGui::DragFloat("Swing Speed", &newPlanetData_.orbitSpeed, 0.01f, 0.01f, 10.0f);
	  } else if (newPlanetData_.type == PlanetType::Wave) {
		 ImGui::Separator();
		 ImGui::Text("Wave Parameters");
		 ImGui::DragFloat3("Wave Direction", &newPlanetData_.waveDirection.x, 0.01f);
		 ImGui::DragFloat("Wave Amplitude", &newPlanetData_.waveAmplitude, 0.1f, 0.1f, 50.0f);
		 ImGui::DragFloat("Wave Speed", &newPlanetData_.orbitSpeed, 0.01f, 0.01f, 10.0f);
		 ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Position is center of wave motion");
	  }
	  
	  ImGui::Separator();
	  
	  if (ImGui::Button("Add Planet", ImVec2(120, 0))) {
		 newPlanetData_.id = GetNextPlanetId();
		 levelData_.planets.push_back(newPlanetData_);
		 dataChanged_ = true;
	 
		 // リセット（タイプは維持）
		 PlanetType savedType = newPlanetData_.type;
		 newPlanetData_.position = Vector3(0.0f, 0.0f, 0.0f);
		 newPlanetData_.planetRadius = 5.0f;
		 newPlanetData_.gravitationalRadius = 25.0f;
		 newPlanetData_.type = savedType;
	  }
	  
	  ImGui::TreePop();
   }
#endif
}

void LevelEditor::ShowRabbitEditor() {
#ifdef USE_IMGUI
   ImGui::Text("Rabbits: %zu", levelData_.rabbits.size());
   ImGui::Separator();
   
   if (levelData_.planets.empty()) {
	  ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.0f, 1.0f), "Please create planets first!");
	  return;
   }
   
   // 既存のうさぎリスト
   if (ImGui::TreeNode("Rabbit List")) {
	  for (size_t i = 0; i < levelData_.rabbits.size(); ++i) {
		 auto& rabbit = levelData_.rabbits[i];
		 
		 ImGui::PushID(static_cast<int>(i));
		 
		 std::string label = "Rabbit " + std::to_string(i + 1);
		 if (ImGui::TreeNode(label.c_str())) {
			bool changed = false;
			
			// 惑星選択（IDではなくコンボボックス）
			char comboPreview[256] = "Select Planet";
			int currentPlanetIndex = -1;
			for (size_t j = 0; j < levelData_.planets.size(); ++j) {
			   if (levelData_.planets[j].id == rabbit.planetId) {
				  currentPlanetIndex = static_cast<int>(j);
				  snprintf(comboPreview, sizeof(comboPreview), "Planet %d", levelData_.planets[j].id);
				  break;
			   }
			}
			
			// 有効なインデックスが見つからない場合は最初の惑星を選択
			if (currentPlanetIndex == -1 && !levelData_.planets.empty()) {
			   rabbit.planetId = levelData_.planets[0].id;
			   currentPlanetIndex = 0;
			   snprintf(comboPreview, sizeof(comboPreview), "Planet %d", levelData_.planets[0].id);
			}
			
			if (ImGui::BeginCombo("Planet", comboPreview)) {
			   for (size_t j = 0; j < levelData_.planets.size(); ++j) {
				  const auto& planet = levelData_.planets[j];
				  char label_planet[64];
				  snprintf(label_planet, sizeof(label_planet), "Planet %d", planet.id);
				  
				  bool isSelected = (static_cast<int>(j) == currentPlanetIndex);
				  if (ImGui::Selectable(label_planet, isSelected)) {
					 rabbit.planetId = planet.id;
					 changed = true;
				  }
				  
				  if (isSelected) {
					 ImGui::SetItemDefaultFocus();
				  }
			   }
			   ImGui::EndCombo();
			}
			
			changed |= ImGui::DragFloat3("Offset", &rabbit.offset.x, 0.1f);
			changed |= ImGui::DragFloat("Radius", &rabbit.radius, 0.01f, 0.1f, 10.0f);
			
			if (changed) {
			   dataChanged_ = true;
			}
			
			ImGui::Separator();
			if (ImGui::Button("Delete")) {
			   levelData_.rabbits.erase(levelData_.rabbits.begin() + i);
			   dataChanged_ = true;
			   ImGui::TreePop();
			   ImGui::PopID();
			   break;
			}
			
			ImGui::TreePop();
		 }
		 
		 ImGui::PopID();
	  }
	  ImGui::TreePop();
   }
   
   ImGui::Separator();
   
   // 新規うさぎ追加
   if (ImGui::TreeNode("Add New Rabbit")) {
	  // 惑星選択（コンボボックス）
	  char comboPreview[256] = "Select Planet";
	  int currentPlanetIndex = -1;
	  for (size_t i = 0; i < levelData_.planets.size(); ++i) {
		 if (levelData_.planets[i].id == newRabbitData_.planetId) {
			currentPlanetIndex = static_cast<int>(i);
			snprintf(comboPreview, sizeof(comboPreview), "Planet %d", levelData_.planets[i].id);
			break;
		 }
	  }
	  
	  // 有効なインデックスが見つからない場合は最初の惑星を選択
	  if (currentPlanetIndex == -1 && !levelData_.planets.empty()) {
		 newRabbitData_.planetId = levelData_.planets[0].id;
		 currentPlanetIndex = 0;
		 snprintf(comboPreview, sizeof(comboPreview), "Planet %d", levelData_.planets[0].id);
	  }
	  
	  if (ImGui::BeginCombo("Planet", comboPreview)) {
		 for (size_t i = 0; i < levelData_.planets.size(); ++i) {
			const auto& planet = levelData_.planets[i];
			char label[64];
			snprintf(label, sizeof(label), "Planet %d", planet.id);
			
			bool isSelected = (static_cast<int>(i) == currentPlanetIndex);
			if (ImGui::Selectable(label, isSelected)) {
			   newRabbitData_.planetId = planet.id;
			}
			
			if (isSelected) {
			   ImGui::SetItemDefaultFocus();
			}
		 }
		 ImGui::EndCombo();
	  }
	  	  
	  ImGui::DragFloat3("Offset", &newRabbitData_.offset.x, 0.1f);
	  ImGui::DragFloat("Radius", &newRabbitData_.radius, 0.01f, 0.1f, 10.0f);
	  	
	  ImGui::Separator();
	  ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Offset is direction from planet center");
	
	  if (ImGui::Button("Add Rabbit", ImVec2(120, 0))) {
		 levelData_.rabbits.push_back(newRabbitData_);
		 dataChanged_ = true;
		 
		 // リセット（惑星IDは維持）
		 newRabbitData_.offset = Vector3(0.0f, 1.0f, 0.0f);
		 newRabbitData_.radius = 0.3f;
	  }
	  
	  ImGui::TreePop();
   }
#endif
}

void LevelEditor::ShowStarEditor() {
#ifdef USE_IMGUI
   ImGui::Text("Star Configuration");
   ImGui::Separator();
   
   if (levelData_.planets.empty()) {
	  ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.0f, 1.0f), "Please create planets first!");
	  
	  // 惑星がない場合はスターデータをリセット
	  if (levelData_.hasStarData) {
		 levelData_.hasStarData = false;
		 dataChanged_ = true;
	  }
	  return;
   }
   
   ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Star is always present in the scene.");
   ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Configure which planet it is placed on.");
   ImGui::Separator();
   
   bool changed = false;
   changed |= ImGui::Checkbox("Place Star", &levelData_.hasStarData);
   
   if (levelData_.hasStarData) {
	  ImGui::Separator();
	  ImGui::Text("Star Placement");
	  
	  // 惑星選択用のバッファを作成
	  char comboPreview[256] = "Select Planet";
	  
	  // 有効な惑星IDを持つかチェック
	  int currentPlanetIndex = -1;
	  for (size_t i = 0; i < levelData_.planets.size(); ++i) {
		 if (levelData_.planets[i].id == levelData_.star.planetId) {
			currentPlanetIndex = static_cast<int>(i);
			snprintf(comboPreview, sizeof(comboPreview), "Planet %d", levelData_.planets[i].id);
			break;
		 }
	  }
	  
	  // 有効なインデックスが見つからない場合は最初の惑星を選択
	  if (currentPlanetIndex == -1 && !levelData_.planets.empty()) {
		 levelData_.star.planetId = levelData_.planets[0].id;
		 currentPlanetIndex = 0;
		 snprintf(comboPreview, sizeof(comboPreview), "Planet %d", levelData_.planets[0].id);
		 changed = true; // 自動選択されたので変更フラグを立てる
	  }
	  
	  if (ImGui::BeginCombo("Target Planet", comboPreview)) {
		 for (size_t i = 0; i < levelData_.planets.size(); ++i) {
			const auto& planet = levelData_.planets[i];
			char label[64];
			snprintf(label, sizeof(label), "Planet %d", planet.id);
			
			bool isSelected = (static_cast<int>(i) == currentPlanetIndex);
			if (ImGui::Selectable(label, isSelected)) {
			   levelData_.star.planetId = planet.id;
			   changed = true;
			}
			
			if (isSelected) {
			   ImGui::SetItemDefaultFocus();
			}
		 }
		 ImGui::EndCombo();
	  }
	  
	  changed |= ImGui::DragFloat3("Offset", &levelData_.star.offset.x, 0.1f);
	  changed |= ImGui::DragFloat("Radius", &levelData_.star.radius, 0.01f, 0.1f, 10.0f);
	  
	  ImGui::Separator();
	  ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Offset is direction from planet center");
	  ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Normalized automatically when placed");
   } else {
	  ImGui::Separator();
	  ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Star is hidden (placed off-screen)");
   }
   
   if (changed) {
	  dataChanged_ = true;
   }
#endif
}

void LevelEditor::ShowBoxEditor() {
#ifdef USE_IMGUI
   ImGui::Text("Boxes: %zu", levelData_.boxes.size());
   ImGui::Separator();
   
   if (levelData_.planets.empty()) {
	  ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.0f, 1.0f), "Please create planets first!");
	  return;
   }
   
   // 既存のボックスリスト
   if (ImGui::TreeNode("Box List")) {
	  for (size_t i = 0; i < levelData_.boxes.size(); ++i) {
		 auto& box = levelData_.boxes[i];
		 
		 ImGui::PushID(static_cast<int>(i));
		 
		 std::string label = "Box " + std::to_string(i + 1);
		 if (ImGui::TreeNode(label.c_str())) {
			bool changed = false;
			
			// 惑星選択（IDではなくコンボボックス）
			char comboPreview[256] = "Select Planet";
			int currentPlanetIndex = -1;
			for (size_t j = 0; j < levelData_.planets.size(); ++j) {
			   if (levelData_.planets[j].id == box.planetId) {
				  currentPlanetIndex = static_cast<int>(j);
				  snprintf(comboPreview, sizeof(comboPreview), "Planet %d", levelData_.planets[j].id);
				  break;
			   }
			}
			
			// 有効なインデックスが見つからない場合は最初の惑星を選択
			if (currentPlanetIndex == -1 && !levelData_.planets.empty()) {
			   box.planetId = levelData_.planets[0].id;
			   currentPlanetIndex = 0;
			   snprintf(comboPreview, sizeof(comboPreview), "Planet %d", levelData_.planets[0].id);
			}
			
			if (ImGui::BeginCombo("Planet", comboPreview)) {
			   for (size_t j = 0; j < levelData_.planets.size(); ++j) {
				  const auto& planet = levelData_.planets[j];
				  char label_planet[64];
				  snprintf(label_planet, sizeof(label_planet), "Planet %d", planet.id);
				  
				  bool isSelected = (static_cast<int>(j) == currentPlanetIndex);
				  if (ImGui::Selectable(label_planet, isSelected)) {
					 box.planetId = planet.id;
					 changed = true;
				  }
				  
				  if (isSelected) {
					 ImGui::SetItemDefaultFocus();
				  }
			   }
			   ImGui::EndCombo();
			}
			
			changed |= ImGui::DragFloat3("Offset", &box.offset.x, 0.1f);
			changed |= ImGui::DragFloat("Size", &box.size, 0.1f, 0.1f, 10.0f);
			changed |= ImGui::DragFloat("Mass", &box.mass, 0.1f, 0.1f, 100.0f);
			
			if (changed) {
			   dataChanged_ = true;
			}
			
			ImGui::Separator();
			if (ImGui::Button("Delete")) {
			   levelData_.boxes.erase(levelData_.boxes.begin() + i);
			   dataChanged_ = true;
			   ImGui::TreePop();
			   ImGui::PopID();
			   break;
			}
			
			ImGui::TreePop();
		 }
		 
		 ImGui::PopID();
	  }
	  ImGui::TreePop();
   }
   
   ImGui::Separator();
   
   // 新規ボックス追加
   if (ImGui::TreeNode("Add New Box")) {
	  // 惑星選択（コンボボックス）
	  char comboPreview[256] = "Select Planet";
	  int currentPlanetIndex = -1;
	  for (size_t i = 0; i < levelData_.planets.size(); ++i) {
		 if (levelData_.planets[i].id == newBoxData_.planetId) {
			currentPlanetIndex = static_cast<int>(i);
			snprintf(comboPreview, sizeof(comboPreview), "Planet %d", levelData_.planets[i].id);
			break;
		 }
	  }
	  
	  // 有効なインデックスが見つからない場合は最初の惑星を選択
	  if (currentPlanetIndex == -1 && !levelData_.planets.empty()) {
		 newBoxData_.planetId = levelData_.planets[0].id;
		 currentPlanetIndex = 0;
		 snprintf(comboPreview, sizeof(comboPreview), "Planet %d", levelData_.planets[0].id);
	  }
	  
	  if (ImGui::BeginCombo("Planet", comboPreview)) {
		 for (size_t i = 0; i < levelData_.planets.size(); ++i) {
			const auto& planet = levelData_.planets[i];
			char label[64];
			snprintf(label, sizeof(label), "Planet %d", planet.id);
			
			bool isSelected = (static_cast<int>(i) == currentPlanetIndex);
			if (ImGui::Selectable(label, isSelected)) {
			   newBoxData_.planetId = planet.id;
			}
			
			if (isSelected) {
			   ImGui::SetItemDefaultFocus();
			}
		 }
		 ImGui::EndCombo();
	  }
	  	  	
	  ImGui::DragFloat3("Offset", &newBoxData_.offset.x, 0.1f);
	  ImGui::DragFloat("Size", &newBoxData_.size, 0.1f, 0.1f, 10.0f);
	  ImGui::DragFloat("Mass", &newBoxData_.mass, 0.1f, 0.1f, 100.0f);
	  	
	  ImGui::Separator();
	  ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Offset is direction from planet center");
	
	  if (ImGui::Button("Add Box", ImVec2(120, 0))) {
		 levelData_.boxes.push_back(newBoxData_);
		 dataChanged_ = true;
		 
		 // リセット（惑星IDは維持）
		 newBoxData_.offset = Vector3(0.0f, 1.0f, 0.0f);
		 newBoxData_.size = 0.8f;
		 newBoxData_.mass = 1.0f;
	  }
	  
	  ImGui::TreePop();
   }
#endif
}

void LevelEditor::ShowPlayerEditor() {
#ifdef USE_IMGUI
   ImGui::Text("Player Configuration");
   ImGui::Separator();
   
   if (levelData_.planets.empty()) {
	  ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Please create planets first!");
	  
	  // 惑星がない場合はプレイヤーデータをリセット
	  if (levelData_.hasPlayerData) {
		 levelData_.hasPlayerData = false;
		 dataChanged_ = true;
	  }
	  return;
   }
   
   ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Set player starting position.");
   ImGui::Separator();
   
   bool changed = false;
   changed |= ImGui::Checkbox("Set Player Start Position", &levelData_.hasPlayerData);
   
   if (levelData_.hasPlayerData) {
	  ImGui::Separator();
	  ImGui::Text("Player Start Position");
	  
	  // 惑星選択用のバッファを作成
	  char comboPreview[256] = "Select Planet";
	  
	  // 有効な惑星IDを持つかチェック
	  int currentPlanetIndex = -1;
	  for (size_t i = 0; i < levelData_.planets.size(); ++i) {
		 if (levelData_.planets[i].id == levelData_.player.planetId) {
			currentPlanetIndex = static_cast<int>(i);
			snprintf(comboPreview, sizeof(comboPreview), "Planet %d", levelData_.planets[i].id);
			break;
		 }
	  }
	  
	  // 有効なインデックスが見つからない場合は最初の惑星を選択
	  if (currentPlanetIndex == -1 && !levelData_.planets.empty()) {
		 levelData_.player.planetId = levelData_.planets[0].id;
		 currentPlanetIndex = 0;
		 snprintf(comboPreview, sizeof(comboPreview), "Planet %d", levelData_.planets[0].id);
		 changed = true; // 自動選択されたので変更フラグを立てる
	  }
	  
	  if (ImGui::BeginCombo("Start Planet", comboPreview)) {
		 for (size_t i = 0; i < levelData_.planets.size(); ++i) {
			const auto& planet = levelData_.planets[i];
			char label[64];
			snprintf(label, sizeof(label), "Planet %d", planet.id);
			
			bool isSelected = (static_cast<int>(i) == currentPlanetIndex);
			if (ImGui::Selectable(label, isSelected)) {
			   levelData_.player.planetId = planet.id;
			   changed = true;
			}
			
			if (isSelected) {
			   ImGui::SetItemDefaultFocus();
			}
		 }
		 ImGui::EndCombo();
	  }
	  
	  changed |= ImGui::DragFloat3("Offset", &levelData_.player.offset.x, 0.1f);
	  
	  ImGui::Separator();
	  ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Offset is direction from planet center");
	  ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Normalized automatically when placed");
   } else {
	  ImGui::Separator();
	  ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Player starts at first planet by default");
   }
   
   if (changed) {
	  dataChanged_ = true;
   }
#endif
}

void LevelEditor::ShowFileOperations() {
#ifdef USE_IMGUI
   ImGui::Text("File Operations");
   ImGui::Separator();
   
   ImGui::InputText("File Path", filePathBuffer_, sizeof(filePathBuffer_));
   
   if (ImGui::Button("Save to File", ImVec2(120, 0))) {
	  if (SaveToJson(filePathBuffer_)) {
		 ImGui::OpenPopup("SaveSuccess");
	  } else {
		 ImGui::OpenPopup("SaveFailed");
	  }
   }
   
   ImGui::SameLine();
   
   if (ImGui::Button("Load from File", ImVec2(120, 0))) {
	  if (LoadFromJson(filePathBuffer_)) {
		 ImGui::OpenPopup("LoadSuccess");
	  } else {
		 ImGui::OpenPopup("LoadFailed");
	  }
   }
   
   // ポップアップ
   if (ImGui::BeginPopupModal("SaveSuccess", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
	  ImGui::Text("Saved successfully!");
	  if (ImGui::Button("OK", ImVec2(120, 0))) {
		 ImGui::CloseCurrentPopup();
	  }
	  ImGui::EndPopup();
   }
   
   if (ImGui::BeginPopupModal("SaveFailed", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
	  ImGui::Text("Failed to save!");
	  if (ImGui::Button("OK", ImVec2(120, 0))) {
		 ImGui::CloseCurrentPopup();
	  }
	  ImGui::EndPopup();
   }
   
   if (ImGui::BeginPopupModal("LoadSuccess", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
	  ImGui::Text("Loaded successfully!");
	  if (ImGui::Button("OK", ImVec2(120, 0))) {
		 ImGui::CloseCurrentPopup();
	  }
	  ImGui::EndPopup();
   }
   
   if (ImGui::BeginPopupModal("LoadFailed", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
	  ImGui::Text("Failed to load!");
	  if (ImGui::Button("OK", ImVec2(120, 0))) {
		 ImGui::CloseCurrentPopup();
	  }
	  ImGui::EndPopup();
   }
   
   ImGui::Separator();
   
   if (ImGui::Button("Clear All Data", ImVec2(120, 0))) {
	  ClearLevelData();
   }
   
   if (dataChanged_) {
	  ImGui::Separator();
	  ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "* Data has been modified");
   }
#endif
}

void LevelEditor::ShowDebugDrawSettings() {
#ifdef USE_IMGUI
   ImGui::Text("Debug Draw Settings");
   ImGui::Separator();
   
   ImGui::Checkbox("Enable Debug Draw", &debugDrawEnabled_);
   ImGui::Separator();
   
   if (debugDrawEnabled_) {
	  ImGui::Checkbox("Draw Planet Spheres", &drawPlanetSpheres_);
	  ImGui::Checkbox("Draw Gravity Radius", &drawGravityRadius_);
	  ImGui::Checkbox("Draw Orbit Paths", &drawOrbitPaths_);
	  ImGui::Checkbox("Draw Labels", &drawLabels_);
	  ImGui::Checkbox("Draw Rabbits", &drawRabbits_);
	  ImGui::Checkbox("Draw Star", &drawStar_);
	  ImGui::Checkbox("Draw Boxes", &drawBoxes_);
   }
#endif
}

void LevelEditor::Draw() {
   if (!debugDrawEnabled_) {
	  return;
   }
   
   // 惑星が存在しない場合は何も描画しない
   if (levelData_.planets.empty()) {
	  return;
   }
   
   // 惑星のデバッグ描画
   for (const auto& planet : levelData_.planets) {
	  DrawPlanetDebug(planet);
   }
   
   // うさぎのデバッグ描画
   if (drawRabbits_ && !levelData_.rabbits.empty()) {
	  for (const auto& rabbit : levelData_.rabbits) {
		 DrawRabbitDebug(rabbit);
	  }
   }
   
   // 星のデバッグ描画
   if (drawStar_ && levelData_.hasStarData) {
	  DrawStarDebug(levelData_.star);
   }
   
   // ボックスのデバッグ描画
   if (drawBoxes_ && !levelData_.boxes.empty()) {
	  for (const auto& box : levelData_.boxes) {
		 DrawBoxDebug(box);
	  }
   }
}

void LevelEditor::DrawPlanetDebug(const PlanetData& planet) {
   // 惑星の色（種類によって変える）
   Vector4 planetColor;
   switch (planet.type) {
	  case PlanetType::Static:
		 planetColor = Vector4(0.0f, 1.0f, 0.0f, 1.0f); // 緑
		 break;
	  case PlanetType::Orbit:
		 planetColor = Vector4(0.0f, 0.5f, 1.0f, 1.0f); // 青
		 break;
	  case PlanetType::Pendulum:
		 planetColor = Vector4(1.0f, 1.0f, 0.0f, 1.0f); // 黄色
		 break;
	  case PlanetType::Wave:
		 planetColor = Vector4(1.0f, 0.5f, 0.0f, 1.0f); // オレンジ
		 break;
	  default:
		 planetColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f); // 白
		 break;
   }
   
   // 惑星の球体を描画
   if (drawPlanetSpheres_) {
	  EngineContext::DrawSphere(planet.position, planet.planetRadius, planetColor);
   }
   
   // 重力範囲を描画
   if (drawGravityRadius_) {
	  Vector4 gravityColor = Vector4(planetColor.x * 0.5f, planetColor.y * 0.5f, planetColor.z * 0.5f, 0.5f);
	  EngineContext::DrawSphere(planet.position, planet.gravitationalRadius, gravityColor);
   }
   
   // 軌道パスを描画
   if (drawOrbitPaths_) {
	  if (planet.type == PlanetType::Orbit) {
		 // 円軌道を描画
		 Vector3 normalizedAxis = planet.orbitAxis;
		 if (normalizedAxis.Length() > 0.001f) {
			normalizedAxis = normalizedAxis.Normalize();
		 } else {
			normalizedAxis = Vector3(0.0f, 1.0f, 0.0f);
		 }
		 EngineContext::DrawCircle(planet.orbitCenter, planet.orbitRadius, normalizedAxis, Vector4(1.0f, 1.0f, 0.0f, 1.0f));
		 
		 // 中心から現在位置への線
		 EngineContext::DrawLine(planet.orbitCenter, planet.position, Vector4(0.5f, 0.5f, 0.5f, 1.0f));
		 
	  } else if (planet.type == PlanetType::Pendulum) {
		 // 振り子の支点から現在位置への線
		 EngineContext::DrawLine(planet.orbitCenter, planet.position, Vector4(1.0f, 1.0f, 0.0f, 1.0f));
		 
		 // 振り子の範囲を円弧で表示
		 EngineContext::DrawCircle(planet.orbitCenter, planet.orbitRadius, Vector3(0.0f, 0.0f, 1.0f), Vector4(1.0f, 1.0f, 0.0f, 0.5f));
		 
	  } else if (planet.type == PlanetType::Wave) {
		 // 波の方向を線で表示
		 Vector3 waveDir = planet.waveDirection;
		 if (waveDir.Length() > 0.001f) {
			waveDir = waveDir.Normalize();
		 }
		 Vector3 start = planet.position - waveDir * planet.waveAmplitude;
		 Vector3 end = planet.position + waveDir * planet.waveAmplitude;
		 EngineContext::DrawLine(start, end, Vector4(1.0f, 0.5f, 0.0f, 1.0f));
	  }
   }
   
   // ラベル表示
   if (drawLabels_) {
	  // TODO: テキスト描画が実装されている場合
   }
}

void LevelEditor::DrawRabbitDebug(const RabbitData& rabbit) {
   // 所属する惑星を探す
   const PlanetData* planet = nullptr;
   for (const auto& p : levelData_.planets) {
	  if (p.id == rabbit.planetId) {
		 planet = &p;
		 break;
	  }
   }
   
   if (!planet) {
	  return;
   }
   
   // うさぎの位置を計算
   Vector3 direction = rabbit.offset;
   if (direction.Length() > 0.001f) {
	  direction = direction.Normalize();
   } else {
	  direction = Vector3(0.0f, 1.0f, 0.0f);
   }
   Vector3 rabbitPos = planet->position + direction * (planet->planetRadius + rabbit.radius);
   
   // うさぎを小さな球で表示
   EngineContext::DrawSphere(rabbitPos, rabbit.radius, Vector4(1.0f, 0.0f, 1.0f, 1.0f));
   
   // 惑星からうさぎへの線
   EngineContext::DrawLine(planet->position, rabbitPos, Vector4(1.0f, 0.0f, 1.0f, 0.5f));
}

void LevelEditor::DrawStarDebug(const StarData& star) {
   // 所属する惑星を探す
   const PlanetData* planet = nullptr;
   for (const auto& p : levelData_.planets) {
	  if (p.id == star.planetId) {
		 planet = &p;
		 break;
	  }
   }
   
   if (!planet) {
	  return;
   }
   
   // 星の位置を計算
   Vector3 direction = star.offset;
   if (direction.Length() > 0.001f) {
	  direction = direction.Normalize();
   } else {
	  direction = Vector3(0.0f, 1.0f, 0.0f);
   }
   Vector3 starPos = planet->position + direction * (planet->planetRadius + star.radius);
   
   // 星を球で表示（黄色）   EngineContext::DrawSphere(starPos, star.radius, Vector4(1.0f, 1.0f, 0.0f, 1.0f));
   
   // 惑星から星への線
   EngineContext::DrawLine(planet->position, starPos, Vector4(1.0f, 1.0f, 0.0f, 0.5f));
}

void LevelEditor::DrawBoxDebug(const BoxData& box) {
   // 所属する惑星を探す
   const PlanetData* planet = nullptr;
   for (const auto& p : levelData_.planets) {
	  if (p.id == box.planetId) {
		 planet = &p;
		 break;
	  }
   }
   
   if (!planet) {
	  return;
   }
   
   // ボックスの位置を計算
   Vector3 direction = box.offset;
   if (direction.Length() > 0.001f) {
	  direction = direction.Normalize();
   } else {
	  direction = Vector3(0.0f, 1.0f, 0.0f);
   }
   Vector3 boxPos = planet->position + direction * (planet->planetRadius + box.size * 0.5f);
   
   // ボックスを立方体で表示（オレンジ色）
   EngineContext::DrawSphere(boxPos, box.size * 0.5f, Vector4(1.0f, 0.5f, 0.0f, 1.0f));
   
   // 惑星からボックスへの線
   EngineContext::DrawLine(planet->position, boxPos, Vector4(1.0f, 0.5f, 0.0f, 0.5f));
}

void LevelEditor::Update(const std::string& filePath) {
   // ファイルパスが空の場合は何もしない
   if (filePath.empty()) {
	  return;
   }
   
   // ファイルが存在しない場合は何もしない
   if (!std::filesystem::exists(filePath)) {
	  return;
   }
   
   try {
	  // ファイルの最終更新時刻を取得
	  auto fileTime = std::filesystem::last_write_time(filePath);
	  long long currentFileTime = fileTime.time_since_epoch().count();
	  
	  // ファイルが更新されていたら再読み込み
	  // 初回（lastFileWriteTime_ == 0）は読み込まない
	  if (currentFileTime != lastFileWriteTime_ && lastFileWriteTime_ != 0) {
		 // データが変更されている場合は警告を表示して読み込みをスキップ
		 if (dataChanged_) {
			#ifdef USE_IMGUI
			OutputDebugStringA("Hot reload skipped: unsaved changes detected\n");
			#endif
			return;
		 }
		 
		 // ファイルを読み込む
		 LoadFromJson(filePath);
	  }
   } catch (const std::exception& e) {
	  #ifdef USE_IMGUI
	  OutputDebugStringA("Update file check failed: ");
	  OutputDebugStringA(e.what());
	  OutputDebugStringA("\n");
	  #endif
   } catch (...) {
	  // エラーを無視して続行
   }
}

void LevelEditor::ClearLevelData() {
   levelData_.planets.clear();
   levelData_.rabbits.clear();
   levelData_.boxes.clear();
   levelData_.hasStarData = false;
   dataChanged_ = true;
}

int LevelEditor::GetNextPlanetId() const {
   int maxId = -1;
   for (const auto& planet : levelData_.planets) {
	  if (planet.id > maxId) {
		 maxId = planet.id;
	  }
   }
   return maxId + 1;
}

LevelEditor::PlanetData* LevelEditor::FindPlanetById(int id) {
   for (auto& planet : levelData_.planets) {
	  if (planet.id == id) {
		 return &planet;
	  }
   }
   return nullptr;
}

bool LevelEditor::SaveToJson(const std::string& filePath) {
   try {
	  // ディレクトリが存在しない場合は作成
	  std::filesystem::path path(filePath);
	  if (path.has_parent_path()) {
		 std::filesystem::create_directories(path.parent_path());
	  }
	  
	  json j;
	  
	  // 惑星データを保存
	  j["planets"] = json::array();
	  for (const auto& planet : levelData_.planets) {
		 json planetJson;
		 planetJson["id"] = planet.id;
		 planetJson["position"] = {planet.position.x, planet.position.y, planet.position.z};
		 planetJson["planetRadius"] = planet.planetRadius;
		 planetJson["gravitationalRadius"] = planet.gravitationalRadius;
		 planetJson["type"] = static_cast<int>(planet.type);
		 planetJson["orbitCenter"] = {planet.orbitCenter.x, planet.orbitCenter.y, planet.orbitCenter.z};
		 planetJson["orbitRadius"] = planet.orbitRadius;
		 planetJson["orbitSpeed"] = planet.orbitSpeed;
		 planetJson["orbitAxis"] = {planet.orbitAxis.x, planet.orbitAxis.y, planet.orbitAxis.z};
		 planetJson["waveDirection"] = {planet.waveDirection.x, planet.waveDirection.y, planet.waveDirection.z};
		 planetJson["waveAmplitude"] = planet.waveAmplitude;
		 planetJson["pendulumAngle"] = planet.pendulumAngle;
		 j["planets"].push_back(planetJson);
	  }
	  
	  // うさぎデータを保存
	  j["rabbits"] = json::array();
	  for (const auto& rabbit : levelData_.rabbits) {
		 json rabbitJson;
		 rabbitJson["planetId"] = rabbit.planetId;
		 rabbitJson["offset"] = {rabbit.offset.x, rabbit.offset.y, rabbit.offset.z};
		 rabbitJson["radius"] = rabbit.radius;
		 j["rabbits"].push_back(rabbitJson);
	  }
	  
	  // 星データを保存
	  j["hasStarData"] = levelData_.hasStarData;
	  if (levelData_.hasStarData) {
		 json starJson;
		 starJson["planetId"] = levelData_.star.planetId;
		 starJson["offset"] = {levelData_.star.offset.x, levelData_.star.offset.y, levelData_.star.offset.z};
		 starJson["radius"] = levelData_.star.radius;
		 j["star"] = starJson;
	  }
	  
	  // プレイヤーデータを保存
	  j["hasPlayerData"] = levelData_.hasPlayerData;
	  if (levelData_.hasPlayerData) {
		 json playerJson;
		 playerJson["planetId"] = levelData_.player.planetId;
		 playerJson["offset"] = {levelData_.player.offset.x, levelData_.player.offset.y, levelData_.player.offset.z};
		 j["player"] = playerJson;
	  }
	  
	  // ボックスデータを保存
	  j["boxes"] = json::array();
	  for (const auto& box : levelData_.boxes) {
		 json boxJson;
		 boxJson["planetId"] = box.planetId;
		 boxJson["offset"] = {box.offset.x, box.offset.y, box.offset.z};
		 boxJson["size"] = box.size;
		 boxJson["mass"] = box.mass;
		 j["boxes"].push_back(boxJson);
	  }
	  
	  // ファイルに書き出し
	  std::ofstream file(filePath);
	  if (!file.is_open()) {
		 return false;
	  }
	  
	  file << j.dump(4);
	  file.close();
	  
	  lastLoadedFilePath_ = filePath;
	  dataChanged_ = false;
	  
	  // ファイルの最終更新時刻を記録
	  auto fileTime = std::filesystem::last_write_time(filePath);
	  lastFileWriteTime_ = fileTime.time_since_epoch().count();
	  
	  return true;
   } catch (...) {
	  return false;
   }
}

bool LevelEditor::LoadFromJson(const std::string& filePath) {
   try {
	  // ファイルから読み込み
	  std::ifstream file(filePath);
	  if (!file.is_open()) {
		 return false;
	  }
	  
	  json j;
	  file >> j;
	  file.close();
	  
	  // データをクリア
	  ClearLevelData();
	  
	  // 惑星データを読み込み
	  if (j.contains("planets") && j["planets"].is_array()) {
		 for (const auto& planetJson : j["planets"]) {
			PlanetData planet;
			planet.id = planetJson.value("id", 0);
			
			// 位置を安全に読み込み
			if (planetJson.contains("position") && planetJson["position"].is_array() && planetJson["position"].size() >= 3) {
			   auto pos = planetJson["position"];
			   planet.position = Vector3(pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>());
			} else {
			   planet.position = Vector3(0.0f, 0.0f, 0.0f);
			}
			
			planet.planetRadius = planetJson.value("planetRadius", 5.0f);
			planet.gravitationalRadius = planetJson.value("gravitationalRadius", 25.0f);
			planet.type = static_cast<PlanetType>(planetJson.value("type", 0));
			
			// 軌道中心を安全に読み込み
			if (planetJson.contains("orbitCenter") && planetJson["orbitCenter"].is_array() && planetJson["orbitCenter"].size() >= 3) {
			   auto orbitCenter = planetJson["orbitCenter"];
			   planet.orbitCenter = Vector3(orbitCenter[0].get<float>(), orbitCenter[1].get<float>(), orbitCenter[2].get<float>());
			} else {
			   planet.orbitCenter = Vector3(0.0f, 0.0f, 0.0f);
			}
			
			planet.orbitRadius = planetJson.value("orbitRadius", 10.0f);
			planet.orbitSpeed = planetJson.value("orbitSpeed", 1.0f);
			
			// 軌道軸を安全に読み込み
			if (planetJson.contains("orbitAxis") && planetJson["orbitAxis"].is_array() && planetJson["orbitAxis"].size() >= 3) {
			   auto orbitAxis = planetJson["orbitAxis"];
			   planet.orbitAxis = Vector3(orbitAxis[0].get<float>(), orbitAxis[1].get<float>(), orbitAxis[2].get<float>());
			} else {
			   planet.orbitAxis = Vector3(0.0f, 1.0f, 0.0f);
			}
			
			// 波の方向を安全に読み込み
			if (planetJson.contains("waveDirection") && planetJson["waveDirection"].is_array() && planetJson["waveDirection"].size() >= 3) {
			   auto waveDir = planetJson["waveDirection"];
			   planet.waveDirection = Vector3(waveDir[0].get<float>(), waveDir[1].get<float>(), waveDir[2].get<float>());
			} else {
			   planet.waveDirection = Vector3(1.0f, 0.0f, 0.0f);
			}
			
			planet.waveAmplitude = planetJson.value("waveAmplitude", 5.0f);
			planet.pendulumAngle = planetJson.value("pendulumAngle", 45.0f);
			
			levelData_.planets.push_back(planet);
		 }
	  }
	  
	  // うさぎデータを読み込み
	  if (j.contains("rabbits") && j["rabbits"].is_array()) {
		 for (const auto& rabbitJson : j["rabbits"]) {
			RabbitData rabbit;
			rabbit.planetId = rabbitJson.value("planetId", 0);
			
			// オフセットを安全に読み込み
			if (rabbitJson.contains("offset") && rabbitJson["offset"].is_array() && rabbitJson["offset"].size() >= 3) {
			   auto offset = rabbitJson["offset"];
			   rabbit.offset = Vector3(offset[0].get<float>(), offset[1].get<float>(), offset[2].get<float>());
			} else {
			   rabbit.offset = Vector3(0.0f, 1.0f, 0.0f);
			}
			
			rabbit.radius = rabbitJson.value("radius", 0.3f);
			
			levelData_.rabbits.push_back(rabbit);
		 }
	  }
	  
	  // 星データを読み込み
	  levelData_.hasStarData = j.value("hasStarData", false);
	  if (levelData_.hasStarData && j.contains("star") && j["star"].is_object()) {
		 auto starJson = j["star"];
		 levelData_.star.planetId = starJson.value("planetId", 0);
	 
		 // オフセットを安全に読み込み
		 if (starJson.contains("offset") && starJson["offset"].is_array() && starJson["offset"].size() >= 3) {
			auto offset = starJson["offset"];
			levelData_.star.offset = Vector3(offset[0].get<float>(), offset[1].get<float>(), offset[2].get<float>());
		 } else {
			levelData_.star.offset = Vector3(0.0f, 1.0f, 0.0f);
		 }
	 
		 levelData_.star.radius = starJson.value("radius", 0.5f);
	  }
	  
	  // プレイヤーデータを読み込み
	  levelData_.hasPlayerData = j.value("hasPlayerData", false);
	  if (levelData_.hasPlayerData && j.contains("player") && j["player"].is_object()) {
		 auto playerJson = j["player"];
		 levelData_.player.planetId = playerJson.value("planetId", 0);
	 
		 // オフセットを安全に読み込み
		 if (playerJson.contains("offset") && playerJson["offset"].is_array() && playerJson["offset"].size() >= 3) {
			auto offset = playerJson["offset"];
			levelData_.player.offset = Vector3(offset[0].get<float>(), offset[1].get<float>(), offset[2].get<float>());
		 } else {
			levelData_.player.offset = Vector3(0.0f, 1.0f, 0.0f);
		 }
	  }
	  
	  // ボックスデータを読み込み
	  if (j.contains("boxes") && j["boxes"].is_array()) {
		 for (const auto& boxJson : j["boxes"]) {
			BoxData box;
			box.planetId = boxJson.value("planetId", 0);
			
			// オフセットを安全に読み込み
			if (boxJson.contains("offset") && boxJson["offset"].is_array() && boxJson["offset"].size() >= 3) {
			   auto offset = boxJson["offset"];
			   box.offset = Vector3(offset[0].get<float>(), offset[1].get<float>(), offset[2].get<float>());
			} else {
			   box.offset = Vector3(0.0f, 1.0f, 0.0f);
			}
			
			box.size = boxJson.value("size", 0.8f);
			box.mass = boxJson.value("mass", 1.0f);
			
			levelData_.boxes.push_back(box);
		 }
	  }
	  
	  lastLoadedFilePath_ = filePath;
	  dataChanged_ = false;
	  
	  // ファイルの最終更新時刻を記録
	  auto fileTime = std::filesystem::last_write_time(filePath);
	  lastFileWriteTime_ = fileTime.time_since_epoch().count();
	  
	  return true;
   } catch (const std::exception& e) {
	  // エラーログを出力（デバッグ用）
	  #ifdef USE_IMGUI
	  OutputDebugStringA("LoadFromJson failed: ");
	  OutputDebugStringA(e.what());
	  OutputDebugStringA("\n");
	  #endif
	  return false;
   } catch (...) {
	  return false;
   }
}
