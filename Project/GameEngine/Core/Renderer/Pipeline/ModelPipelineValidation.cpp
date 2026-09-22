#include "pch.h"
#include "PSOManager.h"
#include "ShaderManager.h"
#include "Graphics/Material.h"
#include "Graphics/Mesh.h"
#include <cmath>
#include <cstddef>

namespace GameEngine {
namespace {
std::string Lower(std::string text) {
   std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
   return text;
}

bool MatchesField(const ShaderVariableInfo& field, UINT offset, UINT columns,
   D3D_SHADER_VARIABLE_TYPE type = D3D_SVT_FLOAT, UINT rows = 1) {
   const auto variableClass = rows == 4 ? D3D_SVC_MATRIX_ROWS : columns == 1 ? D3D_SVC_SCALAR : D3D_SVC_VECTOR;
   return field.offset == offset && field.type == type && field.rows == rows &&
      field.columns == columns && field.elements == 0 && field.variableClass == variableClass;
}

bool ValidateBuiltinBuffer(const std::string& semantic, const ShaderConstantBufferInfo& buffer) {
   // These offsets are the CPU/GPU ABI. Size alone cannot detect reordered fields or changed types.
   std::map<std::string, std::tuple<UINT, UINT, D3D_SHADER_VARIABLE_TYPE, UINT>> fields;
   UINT size = 0;
   if (semantic == "material") {
      size = sizeof(Material::MaterialData);
      fields = {
         { "color", { static_cast<UINT>(offsetof(Material::MaterialData, color)), 4, D3D_SVT_FLOAT, 1 } },
         { "lightingMode", { static_cast<UINT>(offsetof(Material::MaterialData, lightingMode)), 1, D3D_SVT_INT, 1 } },
         { "environmentCoefficient", { static_cast<UINT>(offsetof(Material::MaterialData, environmentCoefficient)), 1, D3D_SVT_FLOAT, 1 } },
         { "uvTransform", { static_cast<UINT>(offsetof(Material::MaterialData, uvTransform)), 4, D3D_SVT_FLOAT, 4 } },
         { "shininess", { static_cast<UINT>(offsetof(Material::MaterialData, shininess)), 1, D3D_SVT_FLOAT, 1 } },
         { "rimLightIntensity", { static_cast<UINT>(offsetof(Material::MaterialData, rimLightIntensity)), 1, D3D_SVT_FLOAT, 1 } },
         { "rimLightPower", { static_cast<UINT>(offsetof(Material::MaterialData, rimLightPower)), 1, D3D_SVT_FLOAT, 1 } },
         { "fillLightIntensity", { static_cast<UINT>(offsetof(Material::MaterialData, fillLightIntensity)), 1, D3D_SVT_FLOAT, 1 } },
         { "rimLightColor", { static_cast<UINT>(offsetof(Material::MaterialData, rimLightColor)), 4, D3D_SVT_FLOAT, 1 } },
         { "fillLightColor", { static_cast<UINT>(offsetof(Material::MaterialData, fillLightColor)), 4, D3D_SVT_FLOAT, 1 } }
      };
   } else if (semantic == "transform") {
      size = 192;
      fields = { { "wVP", { 0, 4, D3D_SVT_FLOAT, 4 } }, { "world", { 64, 4, D3D_SVT_FLOAT, 4 } },
         { "worldInverseTranspose", { 128, 4, D3D_SVT_FLOAT, 4 } } };
   } else if (semantic == "camera") {
      size = 16;
      fields = { { "worldPosition", { 0, 3, D3D_SVT_FLOAT, 1 } } };
   } else if (semantic == "lightcount") {
      size = 16;
      fields = { { "directionalLightCount", { 0, 1, D3D_SVT_UINT, 1 } }, { "pointLightCount", { 4, 1, D3D_SVT_UINT, 1 } },
         { "spotLightCount", { 8, 1, D3D_SVT_UINT, 1 } }, { "areaLightCount", { 12, 1, D3D_SVT_UINT, 1 } } };
   } else {
      return false;
   }
   if (buffer.size != size || buffer.variables.size() != fields.size()) return false;
   for (const auto& field : buffer.variables) {
      const auto it = fields.find(field.name);
      if (it == fields.end()) return false;
      const auto& [offset, columns, type, rows] = it->second;
      if (!MatchesField(field, offset, columns, type, rows)) return false;
   }
   return true;
}
}

const ModelPipelineDefinition* PSOManager::GetModelPipeline(const std::string& name) const {
   const auto it = modelPipelines_.find(name.empty() ? "Object3D" : name);
   return it == modelPipelines_.end() ? nullptr : &it->second;
}

std::vector<std::string> PSOManager::GetModelPipelineNames() const {
   std::vector<std::string> names;
   for (const auto& [name, contract] : modelPipelines_) {
      (void)contract;
      names.push_back(name);
   }
   std::sort(names.begin(), names.end());
   return names;
}

BlendMode PSOManager::ResolveModelBlendMode(const std::string& name, BlendMode requested) const {
   const auto it = fixedModelBlendModes_.find(name.empty() ? "Object3D" : name);
   if (it != fixedModelBlendModes_.end()) return it->second;
   const int value = static_cast<int>(requested);
   return value >= 0 && value < static_cast<int>(BlendMode::kCount) ? requested : BlendMode::kBlendModeNone;
}

bool PSOManager::ValidateModelPipeline(const PipelineDefinition& definition, ModelPipelineDefinition& model) const {
   const auto fail = [&](const std::string& reason) {
      Logger::Error("[ModelPipeline] Rejected pipeline=" + definition.name + ": " + reason);
      return false;
   };
   const auto* vertex = shaderManager_->GetShaderReflection(definition.vertexShader, ShaderType::Vertex);
   const auto* pixel = shaderManager_->GetShaderReflection(definition.pixelShader, ShaderType::Pixel);
   const auto root = rootDefinitions_.find(definition.rootSignature);
   if (!vertex || !pixel || !vertex->isValid || !pixel->isValid || root == rootDefinitions_.end()) return fail("missing shader reflection/root signature");
   if (definition.topologyType != D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE || definition.rtvFormatOverride != DXGI_FORMAT_UNKNOWN) {
      return fail("model draws require triangle topology and the scene render target format");
   }

   const auto layout = definition.inputLayout.empty() ? BuildInputLayoutFromVertexShaderReflection(definition.vertexShader) : definition.inputLayout;
   UINT nextOffset = 0;
   std::unordered_set<std::string> inputs;
   for (const auto& input : layout) {
      const auto semantic = Lower(input.semanticName);
      const UINT offset = semantic == "position" ? static_cast<UINT>(offsetof(Mesh::VertexData, position)) :
         semantic == "texcoord" ? static_cast<UINT>(offsetof(Mesh::VertexData, texCoord)) : static_cast<UINT>(offsetof(Mesh::VertexData, normal));
      const auto format = semantic == "position" ? DXGI_FORMAT_R32G32B32A32_FLOAT :
         semantic == "texcoord" ? DXGI_FORMAT_R32G32_FLOAT : DXGI_FORMAT_R32G32B32_FLOAT;
      const UINT actualOffset = input.alignedByteOffset == D3D12_APPEND_ALIGNED_ELEMENT ? nextOffset : input.alignedByteOffset;
      if ((semantic != "position" && semantic != "texcoord" && semantic != "normal") || input.semanticIndex != 0 ||
         input.format != format || actualOffset != offset || input.inputSlot != 0 ||
         input.inputSlotClass != D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA || input.instanceDataStepRate != 0 ||
         !inputs.insert(semantic).second) return fail("input layout does not match Mesh::VertexData: " + semantic);
      nextOffset = actualOffset + (semantic == "position" ? 16 : semantic == "texcoord" ? 8 : 12);
   }
   for (const auto& input : vertex->inputParameters) {
      if (Lower(input.semanticName).rfind("sv_", 0) == 0) continue;
      const auto found = std::find_if(layout.begin(), layout.end(), [&](const auto& entry) {
         return Lower(entry.semanticName) == Lower(input.semanticName) && entry.semanticIndex == input.semanticIndex &&
            entry.format == ConvertReflectionInputToFormat(input.mask, input.componentType);
      });
      if (found == layout.end()) return fail("vertex shader input is not supplied: " + input.semanticName);
   }

   model = definition.model;
   model.bindings = root->second.parameters;
   std::unordered_set<std::string> semantics;
   for (auto& binding : model.bindings) {
      binding.semantic = Lower(binding.semantic);
      binding.required = false;
      const auto& semantic = binding.semantic;
      const bool cbv = semantic == "material" || semantic == "transform" || semantic == "camera" || semantic == "lightcount" || semantic == "parameters";
      const bool srv = semantic == "texture" || semantic == "envmap" || semantic == "directionallights" ||
         semantic == "pointlights" || semantic == "spotlights" || semantic == "arealights";
      if ((!cbv && !srv) || !semantics.insert(semantic).second || binding.registerSpace != 0 ||
         (cbv && binding.type != D3D12_ROOT_PARAMETER_TYPE_CBV) ||
         (srv && (binding.type != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE || binding.rangeType != D3D12_DESCRIPTOR_RANGE_TYPE_SRV || binding.descriptorCount != 1))) {
         return fail("unsupported/duplicate model binding: " + semantic);
      }
   }

   bool parametersUsed = false;
   for (const auto& [reflection, visibility] : { std::pair{vertex, D3D12_SHADER_VISIBILITY_VERTEX}, std::pair{pixel, D3D12_SHADER_VISIBILITY_PIXEL} }) {
      for (const auto& resource : reflection->boundResources) {
         if (resource.type == D3D_SIT_SAMPLER) {
            const auto sampler = std::find_if(root->second.samplers.begin(), root->second.samplers.end(), [&](const auto& s) {
               return s.shaderRegister == resource.bindPoint && resource.space == 0 && resource.bindCount == 1 &&
                  (s.visibility == visibility || s.visibility == D3D12_SHADER_VISIBILITY_ALL);
            });
            if (sampler == root->second.samplers.end()) return fail("missing sampler: " + resource.name);
            continue;
         }
         auto binding = std::find_if(model.bindings.begin(), model.bindings.end(), [&](const auto& b) {
            const bool cbv = resource.type == D3D_SIT_CBUFFER;
            return b.shaderRegister == resource.bindPoint && b.registerSpace == resource.space && resource.bindCount == 1 &&
               (b.visibility == visibility || b.visibility == D3D12_SHADER_VISIBILITY_ALL) &&
               (cbv ? b.type == D3D12_ROOT_PARAMETER_TYPE_CBV : b.type == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE);
         });
         if (binding == model.bindings.end()) return fail("unbound shader resource: " + resource.name);
         binding->required = true;
         const auto& semantic = binding->semantic;
         if (resource.type != D3D_SIT_CBUFFER) {
            const bool texture = semantic == "texture" || semantic == "envmap";
            const auto dimension = semantic == "envmap" ? D3D_SRV_DIMENSION_TEXTURECUBE : D3D_SRV_DIMENSION_TEXTURE2D;
            if ((texture && (resource.type != D3D_SIT_TEXTURE || resource.dimension != dimension)) ||
               (!texture && resource.type != D3D_SIT_STRUCTURED)) return fail("resource type mismatch: " + semantic);
            continue;
         }
         const auto buffer = std::find_if(reflection->constantBuffers.begin(), reflection->constantBuffers.end(),
            [&](const auto& b) { return b.name == resource.name; });
         if (buffer == reflection->constantBuffers.end()) return fail("missing buffer reflection: " + semantic);
         if (semantic != "parameters") {
            if (!ValidateBuiltinBuffer(semantic, *buffer)) return fail("CPU/HLSL constant-buffer layout mismatch: " + semantic);
            continue;
         }
         parametersUsed = true;
         if (buffer->size != model.parameterBufferSize || buffer->size == 0 || buffer->size > 4096 || buffer->size % 16 != 0 ||
            buffer->variables.size() != model.parameters.size()) return fail("materialParameters size/field count mismatch");
         std::unordered_set<UINT> occupied;
         for (const auto& field : model.parameters) {
            const auto variable = std::find_if(buffer->variables.begin(), buffer->variables.end(), [&](const auto& v) { return v.name == field.name; });
            const UINT count = static_cast<UINT>(field.defaultValue.size());
            if (count == 0 || count > 4 || variable == buffer->variables.end() ||
               !MatchesField(*variable, field.offset, count) || field.offset > buffer->size || count * 4 > buffer->size - field.offset ||
               !std::all_of(field.defaultValue.begin(), field.defaultValue.end(), [](float v) { return std::isfinite(v); })) return fail("parameter layout/type mismatch: " + field.name);
            for (UINT offset = field.offset; offset < field.offset + count * 4; ++offset) {
               if (!occupied.insert(offset).second) return fail("overlapping parameter: " + field.name);
            }
         }
      }
   }
   if (!parametersUsed && (model.parameterBufferSize != 0 || !model.parameters.empty())) return fail("unused materialParameters definition");
   return true;
}
}
