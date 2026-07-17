#pragma once

#include "Component/IObjectComponent.h"
#include "Utility/VectorMath.h"
#include <functional>
#include <string>
#include <vector>

namespace GameEngine {
class Texture;

/// @brief Skybox固有のキューブマップと色を保持するコンポーネント
class SkyboxComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "SkyboxComponent";
   static constexpr ComponentDisplayName kDisplayName{ "スカイボックス", "Skybox" };

   using TextureResolver = std::function<Texture*(const std::string&)>;
   using TextureNamesProvider = std::function<std::vector<std::string>()>;

   /// @brief キューブマップ名からテクスチャを解決する関数を設定する
   static void SetTextureResolver(TextureResolver resolver);

   /// @brief 選択可能なキューブマップ名を提供する関数を設定する
   static void SetTextureNamesProvider(TextureNamesProvider provider);

   /// @brief コンポーネントの型名を取得する
   /// @return SkyboxComponent
   const char* GetTypeName() const override;

   /// @brief 使用するキューブマップテクスチャを設定する
   /// @param texture キューブマップテクスチャ。2Dテクスチャは受け付けない
   void SetTexture(Texture* texture);

   /// @brief 使用するキューブマップをアセット名で設定する
   /// @param textureName キューブマップのアセット名
   void SetTextureName(const std::string& textureName);

   /// @brief 使用中のキューブマップテクスチャを取得する
   /// @return 解決済みテクスチャ。未設定または不正ならnullptr
   Texture* GetTexture() const;

   /// @brief 使用中のキューブマップ名を取得する
   /// @return キューブマップのアセット名
   const std::string& GetTextureName() const { return textureName_; }

   /// @brief スカイボックスの乗算色を設定する
   /// @param color RGBA色
   void SetColor(const Vector4& color) { color_ = color; }

   /// @brief スカイボックスの乗算色を取得する
   /// @return RGBA色
   const Vector4& GetColor() const { return color_; }

   /// @brief 設定をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONから設定を復元する
   /// @param data 保存済みJSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief スカイボックス設定のインスペクターを描画する
   void DrawInspector() override;
#endif

private:
   static TextureResolver textureResolver_;
   static TextureNamesProvider textureNamesProvider_;

   mutable Texture* texture_ = nullptr;
   std::string textureName_;
   Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
};

} // namespace GameEngine
