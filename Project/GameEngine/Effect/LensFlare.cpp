#include "pch.h"
#include "LensFlare.h"
#include "Core/Graphics/GraphicsDevice.h"
#include "Scene/Camera/Camera.h"
#include "Core/Graphics/DirectionalLight.h"
#include "Utility/MathUtils.h"
#include "Utility/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include "ObjectEdit.h"

#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#endif

using json = nlohmann::json;

namespace {
Logger& log_ = Logger::GetInstance();
}

namespace GameEngine {

void LensFlare::Initialize(GraphicsDevice* device, int32_t width, int32_t height, Camera* camera) {
   device_ = device;
   camera_ = camera;
   screenSize_ = { static_cast<float>(width), static_cast<float>(height) };

   viewPortMatrix_ = MakeViewportMatrix(
	  0.0f, 0.0f,
	  static_cast<float>(width), static_cast<float>(height),
	  0.0f, 1.0f
   );

   // テクスチャのリストを初期化（実際のロードは外部で行う）
   loadedTextures_.clear();
   loadedTextures_.reserve(10);

   // エディタ状態の初期化
   editorState_ = {};
   editorState_.selectedTextureIndex = 0;
   editorState_.newScale = 100.0f;  // ピクセルサイズ
   editorState_.newIntensity = 1.0f;

   // オクルージョンクエリリソースの作成
   CreateOcclusionQueryResources();

   // JSON設定ファイルの自動読み込み
   LoadFromJson("resources/lensFlare/lensflare_config.json");
}
void LensFlare::Update(const Vector3& lightWorldPos, Camera* camera) {
   if (flareElements_.empty()) return;
   if (visiblePixels_ <= 0) return;
   if (!camera_) return;

   // 可視性係数を計算
   float visibilityFactor = GetVisibilityFactor();

   // 光源の3D座標をスクリーン座標（ピクセル座標）に変換
   Matrix4x4 viewProjMatrix = camera->GetViewProjectionMatrix();
   Vector3 lightNDC = TransformCoordinate(lightWorldPos, viewProjMatrix);
   Vector3 lightScreenPixels = TransformCoordinate(lightNDC, viewPortMatrix_);

   // ピクセル座標を2Dワールド座標系に変換
   // スクリーン中心が原点、右が+x、上が+yになるように変換
   Vector2 screenCenter = { screenSize_.x * 0.5f, screenSize_.y * 0.5f };
   Vector2 lightWorldPos2D = {
   lightScreenPixels.x - screenCenter.x,
   screenCenter.y - lightScreenPixels.y  // 上を+Yにするため反転
   };


   // レンズフレアの方向ベクトル（画面中心から光源方向）
   Vector2 direction = lightWorldPos2D;

   // レンズフレアの終点位置（光源の反対側）
   Vector2 lensflareEndPos = lightWorldPos2D * -3.0f;

   // 各フレア要素を更新
   for (auto& element : flareElements_) {
	  if (!element.visible) continue;

	  // スケールと輝度を可視性に応じて調整
	  float scaleFactor = element.scale * visibilityFactor;
	  float intensity = element.intensity * visibilityFactor;

	  // 位置を補間（2Dワールド座標系で）
	  Vector2 worldPosition = Easing::Lerp(
		 lightWorldPos2D,
		 lensflareEndPos,
		 element.distance
	  );

	  // スプライトの設定
	  // アンカーポイントを中心(0.5f, 0.5f)に設定することで、
	  // worldPositionがスプライトの中心になる
	  element.sprite->SetAnchorPoint(Vector2(0.5f, 0.5f));
	  element.sprite->SetScale(Vector2(scaleFactor, scaleFactor));
	  element.sprite->SetPosition(worldPosition);

	  // 2D用カメラで更新
	  Texture* texture = &loadedTextures_[element.textureIndex].texture;
	  element.sprite->Update(camera_, texture);

	  // アルファ値を調整
	  element.material->GetMaterialData()->color.w = intensity;
   }
}

void LensFlare::CreateOcclusionQueryResources() {
   HRESULT hr;

   // クエリヒープの作成
   D3D12_QUERY_HEAP_DESC queryHeapDesc{};
   queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
   queryHeapDesc.Count = 2; // 開始と終了用
   queryHeapDesc.NodeMask = 0;

   hr = device_->GetDevice()->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&queryHeap_));
   assert(SUCCEEDED(hr));

   // クエリ結果バッファの作成
   CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_READBACK);
   CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * 2);

   hr = device_->GetDevice()->CreateCommittedResource(
	  &heapProps,
	  D3D12_HEAP_FLAG_NONE,
	  &bufferDesc,
	  D3D12_RESOURCE_STATE_COPY_DEST,
	  nullptr,
	  IID_PPV_ARGS(&queryResultBuffer_)
   );
   assert(SUCCEEDED(hr));
}

void LensFlare::AddTexture(const std::string& name, Texture* texture) {
   if (!texture) return;

   LoadedTexture loadedTex;
   loadedTex.name = name;
   // テクスチャへの参照を保持（ポインタのみ）
   // 実際のTextureオブジェクトは外部で管理される
   loadedTex.texture = *texture;
   loadedTextures_.push_back(loadedTex);
}

void LensFlare::CreateFlareElement(
   const std::string& name,
   int textureIndex,
   uint32_t color,
   float distance,
   float scale,
   float intensity) {

   if (textureIndex < 0 || textureIndex >= static_cast<int>(loadedTextures_.size())) {
	  log_.Log("Invalid texture index for flare element: " + std::to_string(textureIndex), Logger::LogLevel::Warning);
	  return;
   }

   FlareElement element;
   element.name = name;
   element.textureIndex = textureIndex;
   element.distance = distance;
   element.scale = scale;
   element.intensity = intensity;

   // Materialを作成（colorで指定された色を使用）
   element.material = std::make_unique<Material>();
   element.material->Create(color, Material::LightingMode::NONE);

   // Spriteを作成（アンカーポイントを中心に設定）
   element.sprite = std::make_unique<Sprite>();
   element.sprite->Create(Vector2(scale, scale), element.material.get(), Vector2(0.5f, 0.5f));

   flareElements_.push_back(std::move(element));
}

#ifdef USE_IMGUI
void LensFlare::ImGuiEdit() {
   if (ImGui::Begin("Lens Flare Editor")) {
	  ImGui::Text("Loaded Textures: %zu", loadedTextures_.size());
	  ImGui::Text("Flare Elements: %zu", flareElements_.size());

	  ImGui::Separator();
	  ImGui::Text("Occlusion Query");
	  ImGui::Text("Visible Pixels: %llu", visiblePixels_);
	  ImGui::Text("Max Pixels: %llu", maxVisiblePixels_);
	  float visibility = maxVisiblePixels_ > 0 ?
		 static_cast<float>(visiblePixels_) / static_cast<float>(maxVisiblePixels_) : 0.0f;
	  ImGui::Text("Visibility: %.2f%%", visibility * 100.0f);

	  UINT64 minVal = 100;
	  UINT64 maxVal = 100000;
	  ImGui::SliderScalar("Max Visible Pixels", ImGuiDataType_U64, &maxVisiblePixels_, &minVal, &maxVal);

	  ImGui::Separator();
	  ImGui::Text("Add New Flare Element");

	  // テクスチャ選択
	  if (!loadedTextures_.empty()) {
		 if (ImGui::BeginCombo("Texture", loadedTextures_[editorState_.selectedTextureIndex].name.c_str())) {
			for (int i = 0; i < static_cast<int>(loadedTextures_.size()); ++i) {
			   bool isSelected = (editorState_.selectedTextureIndex == i);
			   if (ImGui::Selectable(loadedTextures_[i].name.c_str(), isSelected)) {
				  editorState_.selectedTextureIndex = i;
			   }
			   if (isSelected) {
				  ImGui::SetItemDefaultFocus();
			   }
			}
			ImGui::EndCombo();
		 }
	  }

	  ImGui::InputText("Name", editorState_.newName, sizeof(editorState_.newName));
	  ImGui::SliderFloat("Distance", &editorState_.newDistance, -1.0f, 1.0f);
	  ImGui::SliderFloat("Scale", &editorState_.newScale, 0.1f, 100.0f);
	  ImGui::SliderFloat("Intensity", &editorState_.newIntensity, 0.0f, 1.0f);

	  if (ImGui::Button("Add Element")) {
		 CreateFlareElement(
			editorState_.newName,
			editorState_.selectedTextureIndex,
			0xFFFFFFFF,
			editorState_.newDistance,
			editorState_.newScale,
			editorState_.newIntensity
		 );
	  }

	  ImGui::Separator();
	  ImGui::Text("Existing Elements");

	  // 既存要素の編集
	  for (size_t i = 0; i < flareElements_.size(); ++i) {
		 ImGui::PushID(static_cast<int>(i));
		 auto& element = flareElements_[i];

		 if (ImGui::TreeNode(element.name.c_str())) {
			ImGui::Checkbox("Visible", &element.visible);
			ImGui::SliderFloat("Distance", &element.distance, -1.0f, 1.0f);
			ImGui::SliderFloat("Scale", &element.scale, 0.1f, 100.0f);
			ImGui::SliderFloat("Intensity", &element.intensity, 0.0f, 1.0f);

			// カラーの編集
			Vector4& color = element.material->GetMaterialData()->color;
			float colorArray[4] = { color.x, color.y, color.z, color.w };
			if (ImGui::ColorEdit4("Color", colorArray)) {
			   color = Vector4(colorArray[0], colorArray[1], colorArray[2], colorArray[3]);
			}

			// テクスチャ選択
			if (!loadedTextures_.empty()) {
			   if (ImGui::BeginCombo("Texture", loadedTextures_[element.textureIndex].name.c_str())) {
				  for (int j = 0; j < static_cast<int>(loadedTextures_.size()); ++j) {
					 bool isSelected = (element.textureIndex == j);
					 if (ImGui::Selectable(loadedTextures_[j].name.c_str(), isSelected)) {
						element.textureIndex = j;
					 }
					 if (isSelected) {
						ImGui::SetItemDefaultFocus();
					 }
				  }
				  ImGui::EndCombo();
			   }
			}

			if (ImGui::Button("Remove")) {
			   flareElements_.erase(flareElements_.begin() + i);
			   ImGui::TreePop();
			   ImGui::PopID();
			   break;
			}

			ImGui::TreePop();
		 }
		 ImGui::PopID();
	  }

	  ImGui::Separator();
	  if (ImGui::Button("Save to JSON")) {
		 SaveToJson("resources/lensFlare/lensflare_config.json");
	  }
	  ImGui::SameLine();
	  if (ImGui::Button("Load from JSON")) {
		 LoadFromJson("resources/lensFlare/lensflare_config.json");
	  }
   }
   ImGui::End();
}
#endif

void LensFlare::BeginOcclusionQuery(ID3D12GraphicsCommandList* cmdList, const Vector3& lightWorldPos, Camera* camera) {
   // 光源がカメラの後ろにある場合はクエリを実行しない
   Matrix4x4 viewProjMatrix = camera->GetViewProjectionMatrix();
   Vector3 lightNDC = TransformCoordinate(lightWorldPos, viewProjMatrix);

   // NDC座標でZ値をチェック（0.0～1.0の範囲内かどうか）
   if (lightNDC.z < 0.0f || lightNDC.z > 1.0f) {
	  isQueryActive_ = false;
	  visiblePixels_ = 0;
	  return;
   }

   // スクリーン座標に変換して画面内チェック
   Vector3 lightScreenPos = TransformCoordinate(lightNDC, viewPortMatrix_);

   if (!IsLightInScreen(Vector2(lightScreenPos.x, lightScreenPos.y))) {
	  isQueryActive_ = false;
	  visiblePixels_ = 0;
	  return;
   }

   // オクルージョンクエリ開始
   cmdList->BeginQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_OCCLUSION, 0);
   isQueryActive_ = true;
}

void LensFlare::EndOcclusionQuery(ID3D12GraphicsCommandList* cmdList) {
   if (!isQueryActive_) {
	  return;
   }

   // オクルージョンクエリ終了
   cmdList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_OCCLUSION, 0);

   // クエリ結果をバッファに解決
   cmdList->ResolveQueryData(
	  queryHeap_.Get(),
	  D3D12_QUERY_TYPE_OCCLUSION,
	  0,
	  1,
	  queryResultBuffer_.Get(),
	  0
   );

   isQueryActive_ = false;
}

void LensFlare::ResolveOcclusionQuery() {
   if (!queryResultBuffer_) {
	  return;
   }

   // クエリ結果を読み取る
   UINT64* pData = nullptr;
   D3D12_RANGE readRange{ 0, sizeof(UINT64) };
   HRESULT hr = queryResultBuffer_->Map(0, &readRange, reinterpret_cast<void**>(&pData));

   if (SUCCEEDED(hr) && pData) {
	  visiblePixels_ = *pData;
	  D3D12_RANGE writeRange{ 0, 0 }; // 何も書き込まない
	  queryResultBuffer_->Unmap(0, &writeRange);
   }
}

bool LensFlare::IsLightInScreen(const Vector2& lightScreenPos) const {
   return lightScreenPos.x >= 0.0f && lightScreenPos.x <= screenSize_.x &&
	  lightScreenPos.y >= 0.0f && lightScreenPos.y <= screenSize_.y;
}


float LensFlare::GetVisibilityFactor() const {
   return std::clamp(
	  static_cast<float>(visiblePixels_) / static_cast<float>(maxVisiblePixels_),
	  0.0f, 1.0f
   );
}

Texture* LensFlare::GetElementTexture(const FlareElement& element) {
   if (element.textureIndex < 0 || element.textureIndex >= static_cast<int>(loadedTextures_.size())) {
	  return nullptr;
   }
   return &loadedTextures_[element.textureIndex].texture;
}

void LensFlare::SaveToJson(const std::string& filename) {
   nlohmann::json j;
   for (const auto& element : flareElements_) {
	  nlohmann::json e;
	  e["name"] = element.name;
	  e["textureIndex"] = element.textureIndex;
	  if (element.textureIndex >= 0 && element.textureIndex < static_cast<int>(loadedTextures_.size())) {
		 e["textureName"] = loadedTextures_[element.textureIndex].name;
	  }
	  e["distance"] = element.distance;
	  e["scale"] = element.scale;
	  e["intensity"] = element.intensity;
	  e["visible"] = element.visible;
	  auto& color = element.material->GetMaterialData()->color;
	  e["color"] = { color.x, color.y, color.z, color.w };
	  j.push_back(e);
   }

   std::ofstream ofs(filename);
   if (ofs) {
	  ofs << j.dump(4); // 整形出力
	  log_.Log("Saved lens flare configuration to: " + filename, Logger::LogLevel::Info);
   } else {
	  log_.Log("Failed to save lens flare configuration to: " + filename, Logger::LogLevel::Error);
   }
}

void LensFlare::LoadFromJson(const std::string& filename) {
   std::ifstream ifs(filename);
   if (!ifs) {
	  log_.Log("LensFlare config file not found: " + filename, Logger::LogLevel::Info);
	  return;
   }

   nlohmann::json j;
   try {
	  ifs >> j;
   }
   catch (const std::exception& ex) {
	  log_.Log("Failed to parse LensFlare JSON: " + std::string(ex.what()), Logger::LogLevel::Warning);
	  return;
   }

   flareElements_.clear();
   for (const auto& e : j) {
	  std::string name = e.value("name", "Unnamed");
	  int textureIndex = e.value("textureIndex", 0);
	  float distance = e.value("distance", 0.0f);
	  float scale = e.value("scale", 1.0f);
	  float intensity = e.value("intensity", 1.0f);
	  bool visible = e.value("visible", true);
	  std::vector<float> colorArr = e.value("color", std::vector<float>{1.0f, 1.0f, 1.0f, 1.0f});

	  // カラーをuint32_tに変換
	  uint32_t colorRGBA = 0xFFFFFFFF;
	  if (colorArr.size() == 4) {
		 uint8_t r = static_cast<uint8_t>(colorArr[0] * 255.0f);
		 uint8_t g = static_cast<uint8_t>(colorArr[1] * 255.0f);
		 uint8_t b = static_cast<uint8_t>(colorArr[2] * 255.0f);
		 uint8_t a = static_cast<uint8_t>(colorArr[3] * 255.0f);
		 colorRGBA = (r << 24) | (g << 16) | (b << 8) | a;
	  }

	  // テクスチャインデックスが有効範囲内かチェック
	  if (textureIndex >= 0 && textureIndex < static_cast<int>(loadedTextures_.size())) {
		 CreateFlareElement(name, textureIndex, colorRGBA, distance, scale, intensity);
		 auto& element = flareElements_.back();
		 element.visible = visible;
		 if (colorArr.size() == 4) {
			element.material->GetMaterialData()->color = Vector4(
			   colorArr[0], colorArr[1], colorArr[2], colorArr[3]
			);
		 }
	  } else {
		 log_.Log("Invalid texture index " + std::to_string(textureIndex) + " for flare element: " + name, Logger::LogLevel::Warning);
	  }
   }

   log_.Log("Loaded " + std::to_string(flareElements_.size()) + " lens flare elements from: " + filename, Logger::LogLevel::Info);
}

}