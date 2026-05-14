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
   using MaterialResolver = std::function<Material*(const std::string&)>;
   using MaterialCreator = std::function<Material*(const std::string&, uint32_t, int32_t, const Matrix4x4&)>;
   using MaterialNamesProvider = std::function<std::vector<std::string>()>;
   using EnvironmentTextureResolver = std::function<Texture*(const std::string&)>;
   using EnvironmentTextureNamesProvider = std::function<std::vector<std::string>()>;

   static void SetMaterialResolver(MaterialResolver resolver);
   static void SetMaterialCreator(MaterialCreator creator);
   static void SetMaterialNamesProvider(MaterialNamesProvider provider);
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
   static EnvironmentTextureResolver environmentTextureResolver_;
   static EnvironmentTextureNamesProvider environmentTextureNamesProvider_;
   std::vector<std::string> materialNames_;
};
}
