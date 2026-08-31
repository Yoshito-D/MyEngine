#include "pch.h"
#include "SkyboxComponent.h"
#include "Component/ComponentRegistry.h"
#include "Graphics/Texture.h"
#include "Object.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "imgui.h"
#include "Utility/ImGuiHelper.h"
#endif

namespace {
// Skybox専用Objectにだけ追加可能なFactoryを静的登録し、JSON復元側が具象コンポーネントへ依存しないようにする。
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   GameEngine::SkyboxComponent::kTypeName,
   [](GameEngine::Object& object) -> GameEngine::IObjectComponent* {
      return object.AddComponent<GameEngine::SkyboxComponent>();
   },
   GameEngine::SkyboxComponent::kDisplayName,
   GameEngine::ToObjectTypeMask(GameEngine::ObjectType::Skybox));
}

namespace GameEngine {

SkyboxComponent::TextureResolver SkyboxComponent::textureResolver_ = nullptr;
SkyboxComponent::TextureNamesProvider SkyboxComponent::textureNamesProvider_ = nullptr;

void SkyboxComponent::SetTextureResolver(TextureResolver resolver) {
   // AssetManagerへの直接依存を避けるため、全SkyboxComponentが共有する名前解決窓口だけを注入する。
   textureResolver_ = std::move(resolver);
}

void SkyboxComponent::SetTextureNamesProvider(TextureNamesProvider provider) {
   // Inspectorを開いた時点のAsset一覧を取得できるよう、固定リストではなくProvider自体を保持する。
   textureNamesProvider_ = std::move(provider);
}

const char* SkyboxComponent::GetTypeName() const {
   return kTypeName;
}

void SkyboxComponent::SetTexture(Texture* texture) {
   // 通常の2Dテクスチャをキューブとして参照するとSRV次元が一致しないため拒否する。
   if (texture && !texture->GetMetadata().IsCubemap()) {
      texture = nullptr;
   }
   // Textureの所有権はTextureManagerに残し、Componentは描画用の非所有参照と永続化用の名前だけを同期する。
   texture_ = texture;
   textureName_ = texture ? texture->GetName() : std::string();
}

void SkyboxComponent::SetTextureName(const std::string& textureName) {
   textureName_ = textureName;
   // 名前変更後に旧Textureを返さないようキャッシュを必ず破棄する。Resolver未設定でも名前は残り、後から遅延解決できる。
   texture_ = nullptr;
   GetTexture();
}

Texture* SkyboxComponent::GetTexture() const {
   // シーン読込時に未ロードでも、描画時点で名前から遅延解決できるようにする。
   if (!texture_ && !textureName_.empty() && textureResolver_) {
      Texture* resolvedTexture = textureResolver_(textureName_);
      // 名前が2D Textureを指していてもキャッシュせず、Skybox用SRVの次元不一致を描画まで持ち込まない。
      if (resolvedTexture && resolvedTexture->GetMetadata().IsCubemap()) {
         texture_ = resolvedTexture;
      }
   }
   return texture_;
}

nlohmann::json SkyboxComponent::Serialize() const {
   // 生ポインターではなくAsset名だけを保存し、次回ロード時は現在のTextureManagerから解決し直す。
   return nlohmann::json{
      { "textureName", textureName_ },
      { "color", { color_.x, color_.y, color_.z, color_.w } }
   };
}

void SkyboxComponent::Deserialize(const nlohmann::json& data) {
   // 各設定を独立して復元し、欠落フィールドでは既定値または現在値を維持する。
   if (data.contains("textureName") && data.at("textureName").is_string()) {
      SetTextureName(data.at("textureName").get<std::string>());
   }
   if (data.contains("color") && data.at("color").is_array() && data.at("color").size() >= 4) {
      const auto& color = data.at("color");
      if (color[0].is_number() && color[1].is_number() && color[2].is_number() && color[3].is_number()) {
         color_ = Vector4(color[0].get<float>(), color[1].get<float>(), color[2].get<float>(), color[3].get<float>());
      }
   }
}

#ifdef USE_IMGUI
void SkyboxComponent::DrawInspector() {
   auto Tr = [](const char* japanese, const char* english) {
      return ImGuiHelper::Localize({ japanese, english });
   };

   const std::string header = MakeObjectComponentHeaderLabel(GetTypeName());
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }

   float color[4] = { color_.x, color_.y, color_.z, color_.w };
   if (ImGui::ColorEdit4(Tr("カラー", "Color"), color)) {
      color_ = Vector4(color[0], color[1], color[2], color[3]);
   }

   const char* preview = textureName_.empty() ? Tr("<なし>", "<none>") : textureName_.c_str();
   if (ImGui::BeginCombo(Tr("キューブマップ", "Cubemap"), preview)) {
      if (ImGui::Selectable(Tr("<なし>", "<none>"), textureName_.empty())) {
         SetTextureName({});
      }
      if (textureNamesProvider_) {
         // TextureManagerの再読込や追加を即時反映するため、Comboを開くたびに候補を取り直す。
         const auto textureNames = textureNamesProvider_();
         for (const auto& textureName : textureNames) {
            const bool selected = textureName == textureName_;
            if (ImGui::Selectable(textureName.c_str(), selected)) {
               SetTextureName(textureName);
            }
            if (selected) {
               ImGui::SetItemDefaultFocus();
            }
         }
      }
      ImGui::EndCombo();
   }

   const std::string dropLabel = textureName_.empty()
      ? Tr("DDSキューブマップをここへドロップ", "Drop DDS Cubemap Here")
      : textureName_;
   ImGui::Button((dropLabel + "##SkyboxCubemapDrop").c_str(), ImVec2(-1.0f, 0.0f));
   if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_ASSET_TEXTURE")) {
         const char* textureAssetId = static_cast<const char*>(payload->Data);
         if (textureAssetId && payload->DataSize > 1 && textureResolver_) {
            Texture* candidate = textureResolver_(textureAssetId);
            // 無効なDropでは現在選択を維持し、Cubemap確認後に名前とキャッシュを同時に確定する。
            if (candidate && candidate->GetMetadata().IsCubemap()) {
               textureName_ = textureAssetId;
               texture_ = candidate;
            }
         }
      }
      ImGui::EndDragDropTarget();
   }
}
#endif

} // namespace GameEngine
