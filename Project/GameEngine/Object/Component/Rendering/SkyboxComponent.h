#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/VectorMath.h"
#include <functional>
#include <string>

namespace GameEngine {
class Texture;

/// @brief Skybox固有のキューブマップと色を保持するコンポーネント
class SkyboxComponent final : public IObjectComponent {
public:
   /// @brief シーン保存とファクトリ検索で使うコンポーネント型名。
   static constexpr const char* kTypeName = "SkyboxComponent";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr ComponentDisplayName kDisplayName{"スカイボックス", "Skybox"};

   /// @brief Asset名から非所有のTexture参照を取得するコールバック
   using TextureResolver = std::function<Texture*(const std::string&)>;

   /// @brief キューブマップ名からテクスチャを解決する関数を設定する
   /// @param resolver 全インスタンスで共有する名前解決関数。空関数で解決を無効化する
   /// @note resolverが参照するサービスはSkyboxComponentからの呼出し中、生存している必要がある
   static void SetTextureResolver(TextureResolver resolver);

   /// @brief コンポーネントの型名を取得する
   /// @return SkyboxComponent
   const char* GetTypeName() const override;

   /// @brief 使用するキューブマップテクスチャを設定する
   /// @param texture キューブマップテクスチャ。2Dテクスチャは受け付けない
   /// @note Textureの所有権は移動せず、名前と非所有ポインターを保持する
   void SetTexture(Texture* texture);

   /// @brief 使用するキューブマップをアセット名で設定する
   /// @param textureName キューブマップのアセット名
   /// @details 現在のキャッシュを破棄して解決を試み、未解決なら後続のGetTextureで再試行する
   void SetTextureName(const std::string& textureName);

   /// @brief 使用中のキューブマップテクスチャを取得する
   /// @return 解決済みテクスチャ。未設定または不正ならnullptr
   /// @details 未解決のtextureNameがあればTextureResolverを呼び、Cubemapだけをキャッシュする
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
   static TextureResolver textureResolver_; ///< テクスチャ名から読み込み済み実体を解決する外部提供の処理。

   mutable Texture* texture_ = nullptr;       ///< 描画に使用するテクスチャへの非所有参照。
   std::string textureName_;                  ///< 使用中のキューブマップ名。
   Vector4 color_ = {1.0f, 1.0f, 1.0f, 1.0f}; ///< スカイボックスの乗算色。
};

} // namespace GameEngine
