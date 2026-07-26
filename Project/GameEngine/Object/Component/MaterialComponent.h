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

/// @brief Objectのマテリアルスロットとテクスチャ参照を名前付きで永続化する
class MaterialComponent final : public IObjectComponent {
public:
   /// @brief シリアライズ時に使用するコンポーネント型名
   static constexpr const char* kTypeName = "MaterialComponent";
   /// @brief エディタへ表示するローカライズ済み名称
   static constexpr ComponentDisplayName kDisplayName{ "マテリアル", "Material" };
   /// @brief 名前から既存マテリアルを解決する関数型
   using MaterialResolver = std::function<Material*(const std::string&)>;
   /// @brief 名前と初期値からマテリアルを作成する関数型
   using MaterialCreator = std::function<Material*(const std::string&, uint32_t, int32_t, const Matrix4x4&)>;
   /// @brief 選択可能なマテリアル名一覧を返す関数型
   using MaterialNamesProvider = std::function<std::vector<std::string>()>;
   /// @brief 名前からテクスチャを解決する関数型
   using TextureResolver = std::function<Texture*(const std::string&)>;
   /// @brief 選択可能なテクスチャ名一覧を返す関数型
   using TextureNamesProvider = std::function<std::vector<std::string>()>;
   /// @brief 名前から環境テクスチャを解決する関数型
   using EnvironmentTextureResolver = std::function<Texture*(const std::string&)>;
   /// @brief 選択可能な環境テクスチャ名一覧を返す関数型
   using EnvironmentTextureNamesProvider = std::function<std::vector<std::string>()>;

   /// @brief 全MaterialComponentが使用するマテリアル解決関数を設定する
   /// @param resolver 名前から既存マテリアルを返す関数
   static void SetMaterialResolver(MaterialResolver resolver);
   /// @brief 全MaterialComponentが使用するマテリアル生成関数を設定する
   /// @param creator 名前と初期値からマテリアルを生成する関数
   static void SetMaterialCreator(MaterialCreator creator);
   /// @brief エディタのマテリアル候補供給関数を設定する
   /// @param provider 名前一覧を返す関数
   static void SetMaterialNamesProvider(MaterialNamesProvider provider);
   /// @brief 全MaterialComponentが使用するテクスチャ解決関数を設定する
   /// @param resolver 名前からテクスチャを返す関数
   static void SetTextureResolver(TextureResolver resolver);
   /// @brief エディタのテクスチャ候補供給関数を設定する
   /// @param provider 名前一覧を返す関数
   static void SetTextureNamesProvider(TextureNamesProvider provider);
   /// @brief 全MaterialComponentが使用する環境テクスチャ解決関数を設定する
   /// @param resolver 名前から環境テクスチャを返す関数
   static void SetEnvironmentTextureResolver(EnvironmentTextureResolver resolver);
   /// @brief エディタの環境テクスチャ候補供給関数を設定する
   /// @param provider 名前一覧を返す関数
   static void SetEnvironmentTextureNamesProvider(EnvironmentTextureNamesProvider provider);

   /// @brief 名前付きマテリアルを解決し、存在しなければ指定初期値で作成する
   /// @param name 共有マテリアル名
   /// @param color RGBA8形式の初期色
   /// @param lightingMode 初期ライティング方式
   /// @param uvTransform 初期UV変換
   /// @return 解決または作成したマテリアル。サービス未設定時はnullptr
   Material* EnsureMaterial(const std::string& name,
      uint32_t color = 0xffffffff,
      int32_t lightingMode = Material::LightingMode::HALFLAMBERT,
      const Matrix4x4& uvTransform = MakeIdentity4x4());

   /// @brief 全スロットを破棄して先頭へ1マテリアルを割り当てる
   /// @param material 割り当てるマテリアル
   /// @param materialName シーン保存に使う名前
   void AssignMaterial(Material* material, const std::string& materialName = {});
   /// @brief 末尾へマテリアルスロットを追加する
   /// @param material 追加するマテリアル
   /// @param materialName シーン保存に使う名前
   void AppendMaterial(Material* material, const std::string& materialName = {});
   /// @brief マテリアルスロットと保存名をまとめて置き換える
   /// @param materials 新しいマテリアル一覧
   /// @param materialNames 各スロットの保存名
   void AssignMaterials(const std::vector<Material*>& materials, const std::vector<std::string>& materialNames = {});

   /// @brief 各マテリアルスロットの保存名を取得する
   /// @return スロット順のマテリアル名
   const std::vector<std::string>& GetMaterialNames() const { return materialNames_; }
   /// @brief 各マテリアルスロットのテクスチャ名を取得する
   /// @return スロット順のテクスチャ名
   const std::vector<std::string>& GetTextureNames() const { return textureNames_; }
   /// @brief 指定スロットのテクスチャを名前から解決して取得する
   /// @param index スロット番号
   /// @return 解決したテクスチャ。範囲外または未解決の場合はnullptr
   Texture* GetTexture(size_t index = 0) const;
   /// @brief 指定スロットの保存用テクスチャ名を取得する
   /// @param index スロット番号
   /// @return テクスチャ名。範囲外の場合は空文字列
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

   /// @brief IBLへ使用する環境テクスチャ名を取得する
   /// @return 環境テクスチャ名
   const std::string& GetEnvironmentTextureName() const { return environmentTextureName_; }

   /// @copydoc IObjectComponent::GetTypeName
   const char* GetTypeName() const override;

   /// @copydoc IObjectComponent::Serialize
   nlohmann::json Serialize() const override;

   /// @copydoc IObjectComponent::Deserialize
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @copydoc IObjectComponent::DrawInspector
   void DrawInspector() override;
#endif

   /// 描画側がスロット順で参照するマテリアルポインター
   std::vector<Material*> materials;

   /// シーン保存と遅延解決に使う環境テクスチャ名
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
