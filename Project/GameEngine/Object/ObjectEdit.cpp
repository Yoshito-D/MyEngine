#include "ObjectEdit.h"
#include "../../../externals/imgui/imgui.h"
#include "../Sprite/Sprite.h"
#include "../Model/Model.h"
#include "Core/Graphics/Material.h"
#include <string>

namespace GameEngine {
#ifdef USE_IMGUI
namespace ObjectEdit {

/// @brief マテリアル編集のためのヘルパー関数
/// @param label ラベル
/// @param material 編集対象のマテリアル
/// @param allowLightingEdit ライティングモード編集を許可するか
void EditMaterial(const std::string& label, GameEngine::Material* material, bool allowLightingEdit = true) {
   if (material == nullptr) return;

   ImGui::PushID(material);

   if (ImGui::TreeNode(label.c_str())) {
	  // 色の編集
	  Vector4 color = material->GetColor();
	  if (ImGui::ColorEdit4("Color", reinterpret_cast<float*>(&color))) {
		 material->SetColor(color);
	  }

	  // ライティングモードの編集
	  if (allowLightingEdit) {
		 static const char* lightingModes[] = { "None", "Lambert", "HalfLambert", "Phong", "BlinnPhong" };
		 int mode = static_cast<int>(material->GetLightingMode());
		 if (ImGui::Combo("Lighting Mode", &mode, lightingModes, IM_ARRAYSIZE(lightingModes))) {
			material->SetLightingMode(static_cast<GameEngine::Material::LightingMode>(mode));
		 }

		 // 光沢度の編集（Phong/BlinnPhongの場合のみ表示）
		 if (mode == GameEngine::Material::LightingMode::PHONG || mode == GameEngine::Material::LightingMode::BLINNPHONG) {
			float shininess = material->GetShininess();
			if (ImGui::DragFloat("Shininess", &shininess, 0.1f, 0.0f, 100.0f)) {
			   material->SetShininess(shininess);
			}
		 }
	  }

	  // UVTransformの編集
	  if (ImGui::TreeNode("UV Transform")) {
		 // スケールの編集
		 Vector2 uvScale = material->GetUVScale();
		 if (ImGui::DragFloat2("Scale", &uvScale.x, 0.01f, 0.01f)) {
			material->SetUVScale(uvScale);
		 }

		 // 回転の編集
		 float uvRotation = material->GetUVRotation();
		 if (ImGui::SliderAngle("Rotation", &uvRotation)) {
			material->SetUVRotation(uvRotation);
		 }

		 // 平行移動の編集
		 Vector2 uvTranslation = material->GetUVTranslation();
		 if (ImGui::DragFloat2("Translation", &uvTranslation.x, 0.01f)) {
			material->SetUVTranslation(uvTranslation);
		 }

		 // リセットボタン
		 if (ImGui::Button("Reset UV Transform")) {
			material->ResetUVTransform();
		 }

		 ImGui::TreePop();
	  }

	  ImGui::TreePop();
   }

   ImGui::PopID();
}

void SpriteEdit(const std::string& name, GameEngine::Sprite* sprite) {
   if (sprite == nullptr) return;

   if (ImGui::Begin(name.c_str())) {
	  // Transform parameters
	  if (ImGui::TreeNode("Transform")) {
		 Vector2 position = sprite->GetPosition();
		 if (ImGui::DragFloat2("Position", &position.x, 1.0f)) {
			sprite->SetPosition(position);
		 }

		 Vector2 scale = sprite->GetScale();
		 if (ImGui::DragFloat2("Scale", &scale.x, 0.01f, 0.0f)) {
			sprite->SetScale(scale);
		 }

		 float rotation = sprite->GetRotation();
		 if (ImGui::SliderAngle("Rotation", &rotation)) {
			sprite->SetRotation(rotation);
		 }

		 ImGui::TreePop();
	  }

	  // Size parameters
	  if (ImGui::TreeNode("Size")) {
		 Vector2 size = sprite->GetSize();
		 if (ImGui::DragFloat2("Size", &size.x, 0.1f, 0.0f)) {
			sprite->SetSize(size);
		 }

		 Vector2 anchorPoint = sprite->GetAnchorPoint();
		 if (ImGui::DragFloat2("Anchor Point", &anchorPoint.x, 0.01f, 0.0f, 1.0f)) {
			sprite->SetAnchorPoint(anchorPoint);
		 }

		 ImGui::TreePop();
	  }

	  // Texture parameters
	  if (ImGui::TreeNode("Texture")) {
		 // 実際のテクスチャパラメータを取得
		 Vector2 textureLeftTop = sprite->GetTextureLeftTop();
		 Vector2 textureSize = sprite->GetTextureSize();

		 if (ImGui::DragFloat2("Texture Left Top", &textureLeftTop.x, 1.0f, 0.0f)) {
			sprite->SetTextureLeftTop(textureLeftTop);
		 }

		 if (ImGui::DragFloat2("Texture Size", &textureSize.x, 1.0f, 0.0f)) {
			sprite->SetTextureSize(textureSize);
		 }

		 // 両方同時に設定するためのボタン
		 if (ImGui::Button("Apply Texture UV")) {
			sprite->SetTextureUV(textureLeftTop, textureSize);
		 }

		 ImGui::TreePop();
	  }

	  // Flip parameters
	  if (ImGui::TreeNode("Flip")) {
		 bool flipX = sprite->IsFlipX();
		 if (ImGui::Checkbox("Flip X", &flipX)) {
			sprite->SetFlipX(flipX);
		 }

		 bool flipY = sprite->IsFlipY();
		 if (ImGui::Checkbox("Flip Y", &flipY)) {
			sprite->SetFlipY(flipY);
		 }

		 ImGui::TreePop();
	  }

	  // Material parameters
	  if (sprite->GetMaterialCount() > 0) {
		 GameEngine::Material* material = sprite->GetMaterial();
		 EditMaterial("Material", material, false); // スプライトはライティングなし
	  }
   }
   ImGui::End();
}

void ModelEdit(const std::string& name, GameEngine::Model* model) {
   if (model == nullptr) return;

   if (ImGui::Begin(name.c_str())) {
	  // Transform parameters
	  if (ImGui::TreeNode("Transform")) {
		 Vector3 translation = model->GetPosition();
		 if (ImGui::DragFloat3("Translation", &translation.x, 0.1f)) {
			model->SetPosition(translation);
		 }

		 Vector3 rotation = model->GetRotation();
		 if (ImGui::DragFloat3("Rotation", &rotation.x, 0.01f)) {
			model->SetRotation(rotation);
		 }

		 Vector3 scale = model->GetScale();
		 if (ImGui::DragFloat3("Scale", &scale.x, 0.01f, 0.0f)) {
			model->SetScale(scale);
		 }

		 ImGui::TreePop();
	  }

	  // Material parameters
	  if (model->GetMaterialCount() > 0) {
		 if (ImGui::TreeNode("Materials")) {
			for (size_t i = 0; i < model->GetMaterialCount(); ++i) {
			   GameEngine::Material* material = model->GetMaterial(i);
			   std::string matLabel = "Material " + std::to_string(i);
			   EditMaterial(matLabel, material, true); // モデルはライティングあり
			}
			ImGui::TreePop();
		 }
	  }

	  // Model Asset Info
	  if (ImGui::TreeNode("Model Asset")) {
		 GameEngine::ModelAsset* modelAsset = model->GetModelAsset();
		 if (modelAsset != nullptr) {
			ImGui::Text("Model Asset: Loaded");
			// ここで ModelAsset の詳細情報を表示することも可能
		 } else {
			ImGui::Text("Model Asset: None");
		 }
		 ImGui::TreePop();
	  }
   }
   ImGui::End();
}
}
#endif // USE_IMGUI
}