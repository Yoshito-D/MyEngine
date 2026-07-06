#pragma once
#include "IObjectComponent.h"
#include "Material.h"
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace GameEngine {
class Material;
class Texture;

class MaterialComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "MaterialComponent";
   static constexpr ComponentDisplayName kDisplayName{ "マテリアル", "Material" };
   using MaterialResolver = std::function<Material*(const std::string&)>;
   using MaterialCreator = std::function<Material*(const std::string&, uint32_t, int32_t, const Matrix4x4&)>;
   using MaterialNamesProvider = std::function<std::vector<std::string>()>;
   using TextureResolver = std::function<Texture*(const std::string&)>;
   using TextureNamesProvider = std::function<std::vector<std::string>()>;
   using EnvironmentTextureResolver = std::function<Texture*(const std::string&)>;
   using EnvironmentTextureNamesProvider = std::function<std::vector<std::string>()>;

   static void SetMaterialResolver(MaterialResolver resolver);
   static void SetMaterialCreator(MaterialCreator creator);
   static void SetMaterialNamesProvider(MaterialNamesProvider provider);
   static void SetTextureResolver(TextureResolver resolver);
   static void SetTextureNamesProvider(TextureNamesProvider provider);
   static void SetEnvironmentTextureResolver(EnvironmentTextureResolver resolver);
   static void SetEnvironmentTextureNamesProvider(EnvironmentTextureNamesProvider provider);

   Material* EnsureMaterial(const std::string& name,
      uint32_t color = 0xffffffff,
      int32_t lightingMode = Material::LightingMode::HALFLAMBERT,
      const Matrix4x4& uvTransform = MakeIdentity4x4());

   void AssignMaterial(Material* material, const std::string& materialName = {});
   void AppendMaterial(Material* material, const std::string& materialName = {});
   void AssignMaterials(const std::vector<Material*>& materials, const std::vector<std::string>& materialNames = {});

   const std::vector<std::string>& GetMaterialNames() const { return materialNames_; }
   const std::vector<std::string>& GetTextureNames() const { return textureNames_; }
   Texture* GetTexture(size_t index = 0) const;
   const std::string& GetTextureName(size_t index = 0) const;

   /// @brief 指定スロットのテクスチャ名をコードから設定する
   void SetTextureName(size_t slot, const std::string& name);
   /// @brief スロット 0 のテクスチャ名を設定する（ショートハンド）
   void SetTextureName(const std::string& name) { SetTextureName(0, name); }

   /// @brief Spriteなどの矩形描画で使用するテクスチャ範囲を設定する
   /// @param leftTop テクスチャ左上座標
   /// @param size テクスチャ矩形サイズ。0以下なら描画時に全体を使用する
   void SetTextureUV(const Vector2& leftTop, const Vector2& size);

   /// @brief Spriteなどの矩形描画で使用するテクスチャ左上座標を設定する
   /// @param leftTop テクスチャ左上座標
   void SetTextureLeftTop(const Vector2& leftTop) { textureLeftTop_ = leftTop; }

   /// @brief Spriteなどの矩形描画で使用するテクスチャ矩形サイズを設定する
   /// @param size テクスチャ矩形サイズ。0以下なら描画時に全体を使用する
   void SetTextureSize(const Vector2& size) { textureSize_ = size; }

   /// @brief Spriteなどの矩形描画で使用するテクスチャ左上座標を取得する
   /// @return テクスチャ左上座標
   Vector2 GetTextureLeftTop() const { return textureLeftTop_; }

   /// @brief Spriteなどの矩形描画で使用するテクスチャ矩形サイズを取得する
   /// @return テクスチャ矩形サイズ
   Vector2 GetTextureSize() const { return textureSize_; }

   const std::string& GetEnvironmentTextureName() const { return environmentTextureName_; }

   const char* GetTypeName() const override;

   nlohmann::json Serialize() const override;

   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   std::vector<Material*> materials;

   std::string environmentTextureName_;

private:
   void SyncMaterialNamesSize();

   static MaterialResolver resolver_;
   static MaterialCreator creator_;
   static MaterialNamesProvider namesProvider_;
   static TextureResolver textureResolver_;
   static TextureNamesProvider textureNamesProvider_;
   static EnvironmentTextureResolver environmentTextureResolver_;
   static EnvironmentTextureNamesProvider environmentTextureNamesProvider_;
   std::vector<std::string> materialNames_;
   std::vector<std::string> textureNames_;
   Vector2 textureLeftTop_ = { 0.0f, 0.0f };
   Vector2 textureSize_ = { 0.0f, 0.0f };
};
}
