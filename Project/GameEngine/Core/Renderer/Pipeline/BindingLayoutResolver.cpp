#include "BindingLayoutResolver.h"
#include "RootBindingSlots.h"
#include <algorithm>
#include <cctype>

namespace {
std::string ToLowerString(std::string value) {
   std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
   });
   return value;
}

void RegisterRootSlot(std::unordered_map<std::string, UINT>& table, const std::string& semantic, UINT slot) {
   table[ToLowerString(semantic)] = slot;
}

void RegisterSemantic(GameEngine::PipelineRootParameterTable& table, const std::string& semantic, UINT slot) {
   RegisterRootSlot(table.slotBySemanticName, semantic, slot);
}

bool IsTextureLike(D3D_SHADER_INPUT_TYPE type) {
   return type == D3D_SIT_TEXTURE || type == D3D_SIT_STRUCTURED || type == D3D_SIT_TBUFFER || type == D3D_SIT_BYTEADDRESS;
}
}

namespace GameEngine {

std::vector<std::string> BindingLayoutResolver::GetExpectedSemanticsForRootSignature(const std::string& rootSignatureName) const {
   if (rootSignatureName == "Object3D") {
      return { "material", "transform", "camera", "lightcount", "directionallights", "pointlights", "spotlights", "arealights", "texture" };
   }
   if (rootSignatureName == "Object3DSkinning") {
      return { "material", "transform", "camera", "lightcount", "directionallights", "pointlights", "spotlights", "arealights", "texture", "skinpalette" };
   }
   if (rootSignatureName == "Particle") {
      return { "material", "instancing", "texture" };
   }
   if (rootSignatureName == "Line3D") {
      return { "transform" };
   }
   if (rootSignatureName == "FullscreenTriangle") {
      return { "texture" };
   }
   if (rootSignatureName == "PostProcess") {
      return { "constantbuffer", "inputtexture" };
   }

   return {};
}

void BindingLayoutResolver::BuildPipelineRootParameterTables(
   std::unordered_map<std::string, PipelineRootParameterTable>& output,
   const std::function<const ShaderReflectionInfo*(const std::string&, ShaderType)>& getReflection) const {
   output.clear();

   const auto registerTable = [&output](const std::string& name, PipelineRootParameterTable table) {
      output[name] = std::move(table);
   };

   const auto registerByReflection = [&](PipelineRootParameterTable& table,
      const std::string& shaderName,
      ShaderType stage,
      const std::function<void(const ShaderResourceBindingInfo&, PipelineRootParameterTable&)>& mapFunc) {
      const auto* reflection = getReflection(shaderName, stage);
      if (!reflection || !reflection->isValid) {
         return;
      }

      table.hasReflectionData = true;
      for (const auto& resource : reflection->boundResources) {
         mapFunc(resource, table);
         if (!resource.name.empty()) {
            RegisterSemantic(table, resource.name, resource.bindPoint);
         }
      }
   };

   {
      PipelineRootParameterTable table{};
      registerByReflection(table, "Object3D", ShaderType::Vertex, [](const ShaderResourceBindingInfo& resource, PipelineRootParameterTable& t) {
         if (resource.type == D3D_SIT_CBUFFER && resource.bindPoint == 0) {
            RegisterSemantic(t, "transform", RootBindingSlots::Object3D::kTransform);
         }
      });
      registerByReflection(table, "Object3D", ShaderType::Pixel, [](const ShaderResourceBindingInfo& resource, PipelineRootParameterTable& t) {
         if (resource.type == D3D_SIT_CBUFFER) {
            if (resource.bindPoint == 0) RegisterSemantic(t, "material", RootBindingSlots::Object3D::kMaterial);
            if (resource.bindPoint == 1) RegisterSemantic(t, "camera", RootBindingSlots::Object3D::kCamera);
            if (resource.bindPoint == 2) RegisterSemantic(t, "lightcount", RootBindingSlots::Object3D::kLightCount);
         }
         if (IsTextureLike(resource.type)) {
            if (resource.bindPoint == 0) RegisterSemantic(t, "directionallights", RootBindingSlots::Object3D::kDirectionalLight);
            if (resource.bindPoint == 1) RegisterSemantic(t, "pointlights", RootBindingSlots::Object3D::kPointLight);
            if (resource.bindPoint == 2) RegisterSemantic(t, "spotlights", RootBindingSlots::Object3D::kSpotLight);
            if (resource.bindPoint == 3) RegisterSemantic(t, "arealights", RootBindingSlots::Object3D::kAreaLight);
            if (resource.bindPoint == 4) RegisterSemantic(t, "texture", RootBindingSlots::Object3D::kTexture);
            if (resource.bindPoint == 5) RegisterSemantic(t, "envmap", RootBindingSlots::Object3D::kEnvMap);
         }
      });
      if (!table.hasReflectionData) {
         RegisterSemantic(table, "material", RootBindingSlots::Object3D::kMaterial);
         RegisterSemantic(table, "transform", RootBindingSlots::Object3D::kTransform);
         RegisterSemantic(table, "camera", RootBindingSlots::Object3D::kCamera);
         RegisterSemantic(table, "lightcount", RootBindingSlots::Object3D::kLightCount);
         RegisterSemantic(table, "directionallights", RootBindingSlots::Object3D::kDirectionalLight);
         RegisterSemantic(table, "pointlights", RootBindingSlots::Object3D::kPointLight);
         RegisterSemantic(table, "spotlights", RootBindingSlots::Object3D::kSpotLight);
         RegisterSemantic(table, "arealights", RootBindingSlots::Object3D::kAreaLight);
         RegisterSemantic(table, "texture", RootBindingSlots::Object3D::kTexture);
         RegisterSemantic(table, "envmap", RootBindingSlots::Object3D::kEnvMap);
      }
      registerTable("Object3D", table);
      registerTable("Sprite", table);
   }

   {
      PipelineRootParameterTable table{};
      registerByReflection(table, "SkinningObject3D", ShaderType::Vertex, [](const ShaderResourceBindingInfo& resource, PipelineRootParameterTable& t) {
         if (resource.type == D3D_SIT_CBUFFER && resource.bindPoint == 0) {
            RegisterSemantic(t, "transform", RootBindingSlots::Object3D::kTransform);
         }
         if (IsTextureLike(resource.type) && resource.bindPoint == 5) {
            RegisterSemantic(t, "skinpalette", RootBindingSlots::Object3D::kSkinPalette);
         }
      });
      registerByReflection(table, "Object3D", ShaderType::Pixel, [](const ShaderResourceBindingInfo& resource, PipelineRootParameterTable& t) {
         if (resource.type == D3D_SIT_CBUFFER) {
            if (resource.bindPoint == 0) RegisterSemantic(t, "material", RootBindingSlots::Object3D::kMaterial);
            if (resource.bindPoint == 1) RegisterSemantic(t, "camera", RootBindingSlots::Object3D::kCamera);
            if (resource.bindPoint == 2) RegisterSemantic(t, "lightcount", RootBindingSlots::Object3D::kLightCount);
         }
         if (IsTextureLike(resource.type)) {
            if (resource.bindPoint == 0) RegisterSemantic(t, "directionallights", RootBindingSlots::Object3D::kDirectionalLight);
            if (resource.bindPoint == 1) RegisterSemantic(t, "pointlights", RootBindingSlots::Object3D::kPointLight);
            if (resource.bindPoint == 2) RegisterSemantic(t, "spotlights", RootBindingSlots::Object3D::kSpotLight);
            if (resource.bindPoint == 3) RegisterSemantic(t, "arealights", RootBindingSlots::Object3D::kAreaLight);
            if (resource.bindPoint == 4) RegisterSemantic(t, "texture", RootBindingSlots::Object3D::kTexture);
            if (resource.bindPoint == 5) RegisterSemantic(t, "envmap", RootBindingSlots::Object3D::kEnvMap);
         }
      });
      if (!table.hasReflectionData) {
         RegisterSemantic(table, "material", RootBindingSlots::Object3D::kMaterial);
         RegisterSemantic(table, "transform", RootBindingSlots::Object3D::kTransform);
         RegisterSemantic(table, "camera", RootBindingSlots::Object3D::kCamera);
         RegisterSemantic(table, "lightcount", RootBindingSlots::Object3D::kLightCount);
         RegisterSemantic(table, "directionallights", RootBindingSlots::Object3D::kDirectionalLight);
         RegisterSemantic(table, "pointlights", RootBindingSlots::Object3D::kPointLight);
         RegisterSemantic(table, "spotlights", RootBindingSlots::Object3D::kSpotLight);
         RegisterSemantic(table, "arealights", RootBindingSlots::Object3D::kAreaLight);
         RegisterSemantic(table, "texture", RootBindingSlots::Object3D::kTexture);
         RegisterSemantic(table, "envmap", RootBindingSlots::Object3D::kEnvMap);
         RegisterSemantic(table, "skinpalette", RootBindingSlots::Object3D::kSkinPalette);
      }
      registerTable("SkinningObject3D", table);
   }

   {
      PipelineRootParameterTable table{};
      registerByReflection(table, "Particle", ShaderType::Vertex, [](const ShaderResourceBindingInfo& resource, PipelineRootParameterTable& t) {
         if (IsTextureLike(resource.type) && resource.bindPoint == 0) {
            RegisterSemantic(t, "instancing", RootBindingSlots::Particle::kInstancing);
         }
      });
      registerByReflection(table, "Particle", ShaderType::Pixel, [](const ShaderResourceBindingInfo& resource, PipelineRootParameterTable& t) {
         if (resource.type == D3D_SIT_CBUFFER && resource.bindPoint == 0) {
            RegisterSemantic(t, "material", RootBindingSlots::Particle::kMaterial);
         }
         if (IsTextureLike(resource.type) && resource.bindPoint == 1) {
            RegisterSemantic(t, "texture", RootBindingSlots::Particle::kTexture);
         }
      });
      if (!table.hasReflectionData) {
         RegisterSemantic(table, "material", RootBindingSlots::Particle::kMaterial);
         RegisterSemantic(table, "instancing", RootBindingSlots::Particle::kInstancing);
         RegisterSemantic(table, "texture", RootBindingSlots::Particle::kTexture);
      }
      registerTable("Particle", table);
   }

   {
      PipelineRootParameterTable table{};
      registerByReflection(table, "Line3D", ShaderType::Vertex, [](const ShaderResourceBindingInfo& resource, PipelineRootParameterTable& t) {
         if (resource.type == D3D_SIT_CBUFFER && resource.bindPoint == 0) {
            RegisterSemantic(t, "transform", RootBindingSlots::Line3D::kTransform);
         }
      });
      if (!table.hasReflectionData) {
         RegisterSemantic(table, "transform", RootBindingSlots::Line3D::kTransform);
      }
      registerTable("Line3D", table);
   }

   {
      PipelineRootParameterTable table{};
      registerByReflection(table, "FullscreenTriangle", ShaderType::Pixel, [](const ShaderResourceBindingInfo& resource, PipelineRootParameterTable& t) {
         if (IsTextureLike(resource.type) && resource.bindPoint == 0) {
            RegisterSemantic(t, "texture", RootBindingSlots::FullscreenTriangle::kTexture);
            RegisterSemantic(t, "inputtexture", RootBindingSlots::FullscreenTriangle::kTexture);
         }
      });
      if (!table.hasReflectionData) {
         RegisterSemantic(table, "texture", RootBindingSlots::FullscreenTriangle::kTexture);
         RegisterSemantic(table, "inputtexture", RootBindingSlots::FullscreenTriangle::kTexture);
      }
      registerTable("FullscreenTriangle", table);
   }

   const std::vector<std::string> postProcessEffects = {
      "Grayscale", "RadialBlur", "GaussFilter", "Vignette",
      "ChromaticAberration", "ShockWave", "Pixelation", "Bloom", "BoxFilter"
   };

   for (const auto& effectName : postProcessEffects) {
      PipelineRootParameterTable table{};
      registerByReflection(table, effectName, ShaderType::Pixel, [](const ShaderResourceBindingInfo& resource, PipelineRootParameterTable& t) {
         if (resource.type == D3D_SIT_CBUFFER && resource.bindPoint == 0) {
            RegisterSemantic(t, "constantbuffer", RootBindingSlots::PostProcess::kConstantBuffer);
            RegisterSemantic(t, "material", RootBindingSlots::PostProcess::kConstantBuffer);
         }
         if (IsTextureLike(resource.type) && resource.bindPoint == 0) {
            RegisterSemantic(t, "texture", RootBindingSlots::PostProcess::kInputTexture);
            RegisterSemantic(t, "inputtexture", RootBindingSlots::PostProcess::kInputTexture);
         }
      });
      if (!table.hasReflectionData) {
         RegisterSemantic(table, "constantbuffer", RootBindingSlots::PostProcess::kConstantBuffer);
         RegisterSemantic(table, "material", RootBindingSlots::PostProcess::kConstantBuffer);
         RegisterSemantic(table, "texture", RootBindingSlots::PostProcess::kInputTexture);
         RegisterSemantic(table, "inputtexture", RootBindingSlots::PostProcess::kInputTexture);
      }
      registerTable(effectName, table);
      registerTable("PostProcess_" + effectName, table);
   }
}

}
