#include "pch.h"
#include "PSOManager.h"
#include "ShaderManager.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/OffscreenRenderTarget.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>

using json = nlohmann::json;

namespace {
// 文字列からD3D12列挙型への変換ヘルパー
D3D12_CULL_MODE StringToCullMode(const std::string& str) {
   if (str == "None") return D3D12_CULL_MODE_NONE;
   if (str == "Front") return D3D12_CULL_MODE_FRONT;
   if (str == "Back") return D3D12_CULL_MODE_BACK;
   return D3D12_CULL_MODE_BACK;
}

std::string ToLowerString(std::string value) {
   std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
	  return static_cast<char>(std::tolower(c));
   });
   return value;
}

D3D12_FILL_MODE StringToFillMode(const std::string& str) {
   if (str == "Wireframe") return D3D12_FILL_MODE_WIREFRAME;
   if (str == "Solid") return D3D12_FILL_MODE_SOLID;
   return D3D12_FILL_MODE_SOLID;
}

D3D12_DEPTH_WRITE_MASK StringToDepthWriteMask(const std::string& str) {
   if (str == "Zero") return D3D12_DEPTH_WRITE_MASK_ZERO;
   if (str == "All") return D3D12_DEPTH_WRITE_MASK_ALL;
   return D3D12_DEPTH_WRITE_MASK_ALL;
}

D3D12_COMPARISON_FUNC StringToComparisonFunc(const std::string& str) {
   if (str == "Never") return D3D12_COMPARISON_FUNC_NEVER;
   if (str == "Less") return D3D12_COMPARISON_FUNC_LESS;
   if (str == "Equal") return D3D12_COMPARISON_FUNC_EQUAL;
   if (str == "LessEqual") return D3D12_COMPARISON_FUNC_LESS_EQUAL;
   if (str == "Greater") return D3D12_COMPARISON_FUNC_GREATER;
   if (str == "NotEqual") return D3D12_COMPARISON_FUNC_NOT_EQUAL;
   if (str == "GreaterEqual") return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
   if (str == "Always") return D3D12_COMPARISON_FUNC_ALWAYS;
   return D3D12_COMPARISON_FUNC_LESS_EQUAL;
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE StringToTopologyType(const std::string& str) {
   if (str == "Point") return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
   if (str == "Line") return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
   if (str == "Triangle") return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
   if (str == "Patch") return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
   return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
}

BlendMode StringToBlendMode(const std::string& str) {
   if (str == "None") return BlendMode::kBlendModeNone;
   if (str == "Normal") return BlendMode::kBlendModeNormal;
   if (str == "Add") return BlendMode::kBlendModeAdd;
   if (str == "Subtract") return BlendMode::kBlendModeSubtract;
   if (str == "Multiply") return BlendMode::kBlendModeMultiply;
   if (str == "Screen") return BlendMode::kBlendModeScreen;
   return BlendMode::kBlendModeNone;
}

DXGI_FORMAT StringToFormat(const std::string& str) {
   if (str == "R32G32B32A32_FLOAT") return DXGI_FORMAT_R32G32B32A32_FLOAT;
   if (str == "R32G32B32_FLOAT") return DXGI_FORMAT_R32G32B32_FLOAT;
   if (str == "R32G32_FLOAT") return DXGI_FORMAT_R32G32_FLOAT;
   if (str == "R32_FLOAT") return DXGI_FORMAT_R32_FLOAT;
   if (str == "R32G32B32A32_SINT") return DXGI_FORMAT_R32G32B32A32_SINT;
   if (str == "R32G32B32_SINT") return DXGI_FORMAT_R32G32B32_SINT;
   if (str == "R32G32_SINT") return DXGI_FORMAT_R32G32_SINT;
   if (str == "R32_SINT") return DXGI_FORMAT_R32_SINT;
   if (str == "R32G32B32A32_UINT") return DXGI_FORMAT_R32G32B32A32_UINT;
   if (str == "R32G32B32_UINT") return DXGI_FORMAT_R32G32B32_UINT;
   if (str == "R32G32_UINT") return DXGI_FORMAT_R32G32_UINT;
   if (str == "R32_UINT") return DXGI_FORMAT_R32_UINT;
   return DXGI_FORMAT_R32G32B32A32_FLOAT;
}

D3D12_INPUT_CLASSIFICATION StringToInputClassification(const std::string& str) {
   if (str == "PerVertexData") return D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
   if (str == "PerInstanceData") return D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
   return D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
}

D3D12_ROOT_PARAMETER_TYPE StringToRootParameterType(const std::string& str) {
   if (str == "DescriptorTable") return D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
   if (str == "32BitConstants") return D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
   if (str == "CBV") return D3D12_ROOT_PARAMETER_TYPE_CBV;
   if (str == "SRV") return D3D12_ROOT_PARAMETER_TYPE_SRV;
   if (str == "UAV") return D3D12_ROOT_PARAMETER_TYPE_UAV;
   return D3D12_ROOT_PARAMETER_TYPE_CBV;
}

D3D12_SHADER_VISIBILITY StringToShaderVisibility(const std::string& str) {
   if (str == "All") return D3D12_SHADER_VISIBILITY_ALL;
   if (str == "Vertex") return D3D12_SHADER_VISIBILITY_VERTEX;
   if (str == "Hull") return D3D12_SHADER_VISIBILITY_HULL;
   if (str == "Domain") return D3D12_SHADER_VISIBILITY_DOMAIN;
   if (str == "Geometry") return D3D12_SHADER_VISIBILITY_GEOMETRY;
   if (str == "Pixel") return D3D12_SHADER_VISIBILITY_PIXEL;
   return D3D12_SHADER_VISIBILITY_ALL;
}

D3D12_FILTER StringToFilter(const std::string& str) {
   if (str == "MinMagMipPoint") return D3D12_FILTER_MIN_MAG_MIP_POINT;
   if (str == "MinMagMipLinear") return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
   if (str == "Anisotropic") return D3D12_FILTER_ANISOTROPIC;
   return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
}

D3D12_TEXTURE_ADDRESS_MODE StringToAddressMode(const std::string& str) {
   if (str == "Wrap") return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
   if (str == "Mirror") return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
   if (str == "Clamp") return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
   if (str == "Border") return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
   if (str == "MirrorOnce") return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
   return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
}

D3D12_DESCRIPTOR_RANGE_TYPE StringToRangeType(const std::string& str) {
   if (str == "SRV") return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
   if (str == "UAV") return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
   if (str == "CBV") return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
   if (str == "Sampler") return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
   return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
}

// wstringからstringへの変換
std::string WStringToString(const std::wstring& wstr) {
   if (wstr.empty()) return std::string();
   int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
   std::string strTo(size_needed, 0);
   WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
   return strTo;
}
}

namespace GameEngine {
void PSOManager::Initialize(GraphicsDevice* device, ShaderManager* shaderManager) {
   device_ = device;
   shaderManager_ = shaderManager;
   emittedValidationWarnings_.clear();
}

bool PSOManager::LoadPipelineDefinitions(const std::wstring& definitionFilePath, DXGI_FORMAT rtvFormat) {
   std::vector<std::string> rootSignaturePaths;
   std::vector<std::string> pipelinePaths;
   if (!definitionLoader_.LoadRegistryFile(definitionFilePath, rootSignaturePaths, pipelinePaths)) {
	  return false;
   }

   for (const auto& rootSignaturePath : rootSignaturePaths) {
	  if (!LoadRootSignatureFromFile(rootSignaturePath)) {
	  }
   }

   for (const auto& pipelinePath : pipelinePaths) {
	  if (!LoadPipelineFromFile(pipelinePath, rtvFormat)) {
	  }
   }

   return true;
}

void PSOManager::LogValidationMessage(const std::string& dedupeKey, const std::string& message, Logger::LogLevel level) {
   if (level == Logger::LogLevel::Warning) {
	  if (!emittedValidationWarnings_.insert(dedupeKey).second) {
		 return;
	  }
   }

   switch (level) {
	  case Logger::LogLevel::Trace:
		 Logger::Trace(message);
		 break;
	  case Logger::LogLevel::Debug:
		 Logger::Debug(message);
		 break;
	  case Logger::LogLevel::Warning:
		 Logger::Warning(message);
		 break;
	  case Logger::LogLevel::Error:
		 Logger::Error(message);
		 break;
	  case Logger::LogLevel::Critical:
		 Logger::Critical(message);
		 break;
	  case Logger::LogLevel::Info:
	  default:
		 Logger::Info(message);
		 break;
   }
}

bool PSOManager::LoadRootSignatureFromFile(const std::string& filePath) {
   std::ifstream file(filePath);
   if (!file.is_open()) {
      Logger::Error("Failed to open root signature file: " + filePath);
      return false;
   }

   try {
      json rootSigJson;
      file >> rootSigJson;

      RootSignatureDefinition definition;
      definition.name = rootSigJson["name"].get<std::string>();

	  const auto reflectionSources = rootSigJson.value("reflectionSources", json::object());
	  const std::string reflectionVS = reflectionSources.value("vertexShader", "");
	  const std::string reflectionPS = reflectionSources.value("pixelShader", "");
	  const std::string reflectionCS = reflectionSources.value("computeShader", "");
	  std::unordered_map<std::string, UINT> fallbackRegisterByGroup;

      // パラメータをロード
      if (rootSigJson.contains("parameters")) {
         for (const auto& param : rootSigJson["parameters"]) {
            RootParameterDefinition paramDef;
            paramDef.type = StringToRootParameterType(param["type"].get<std::string>());
            paramDef.visibility = StringToShaderVisibility(param["visibility"].get<std::string>());
            paramDef.shaderRegister = param.value("shaderRegister", UINT_MAX);
            paramDef.registerSpace = param.value("registerSpace", 0);
            paramDef.descriptorCount = param.value("descriptorCount", 1);
			paramDef.semantic = param.value("semantic", "");
            
            // ディスクリプタテーブルの場合、rangeTypeを読み込む
            if (paramDef.type == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
               if (param.contains("rangeType")) {
                  paramDef.rangeType = StringToRangeType(param["rangeType"].get<std::string>());
                  Logger::Info("  Parameter " + std::to_string(definition.parameters.size()) + 
                     ": DescriptorTable with rangeType=" + param["rangeType"].get<std::string>() + 
                     " (value=" + std::to_string(static_cast<int>(paramDef.rangeType)) + ")");
               } else {
                  Logger::Warning("  Parameter " + std::to_string(definition.parameters.size()) + 
                     ": DescriptorTable without rangeType - using default SRV");
               }
			}

            if (paramDef.shaderRegister == UINT_MAX && !paramDef.semantic.empty()) {
			   const auto resolvedRegister = ResolveShaderRegisterBySemantic(
				  paramDef.semantic,
				  paramDef.type,
				  paramDef.visibility,
				  paramDef.rangeType,
				  reflectionVS,
				  reflectionPS,
				  reflectionCS);
			   if (resolvedRegister.has_value()) {
				  paramDef.shaderRegister = resolvedRegister.value();
			   }
			}

			std::string fallbackGroupKey = std::to_string(static_cast<int>(paramDef.type)) + ":" +
			   std::to_string(static_cast<int>(paramDef.visibility)) + ":" +
			   std::to_string(static_cast<int>(paramDef.rangeType));

			if (paramDef.shaderRegister == UINT_MAX) {
			   UINT& nextRegister = fallbackRegisterByGroup[fallbackGroupKey];
			   Logger::Warning(
				  "Failed to resolve shaderRegister by reflection for root signature parameter in " + definition.name +
				  ", fallback to grouped register " + std::to_string(nextRegister));
			   paramDef.shaderRegister = nextRegister;
			   ++nextRegister;
			} else {
			   UINT& nextRegister = fallbackRegisterByGroup[fallbackGroupKey];
			   nextRegister = std::max(nextRegister, paramDef.shaderRegister + 1);
			}
            
            definition.parameters.push_back(paramDef);
         }
      }

      // サンプラーをロード
      if (rootSigJson.contains("samplers")) {
         for (const auto& samp : rootSigJson["samplers"]) {
            SamplerDefinition sampDef;
            sampDef.filter = StringToFilter(samp["filter"].get<std::string>());
            sampDef.addressU = StringToAddressMode(samp["addressU"].get<std::string>());
            sampDef.addressV = StringToAddressMode(samp["addressV"].get<std::string>());
            sampDef.addressW = StringToAddressMode(samp["addressW"].get<std::string>());
            sampDef.comparisonFunc = StringToComparisonFunc(samp["comparisonFunc"].get<std::string>());
            sampDef.maxLOD = samp["maxLOD"].get<float>();
            sampDef.shaderRegister = samp["shaderRegister"].get<UINT>();
            sampDef.visibility = StringToShaderVisibility(samp["visibility"].get<std::string>());
            definition.samplers.push_back(sampDef);
         }
      }

      Logger::Info("Loaded root signature definition: " + definition.name + " with " + std::to_string(definition.parameters.size()) + " parameters");
      return CreateRootSignatureFromDefinition(definition);
   }
   catch (const std::exception& e) {
      Logger::Error("Exception loading root signature from " + filePath + ": " + std::string(e.what()));
      return false;
   }
}

bool PSOManager::LoadPipelineFromFile(const std::string& filePath, DXGI_FORMAT rtvFormat) {
   std::ifstream file(filePath);
   if (!file.is_open()) {
	  return false;
   }

   try {
	  json pipelineJson;
	  file >> pipelineJson;

	  PipelineDefinition definition;
	  definition.name = pipelineJson["name"].get<std::string>();
	  definition.vertexShader = pipelineJson.value("vertexShader", "");
	  definition.pixelShader = pipelineJson.value("pixelShader", "");
	  definition.rootSignature = pipelineJson.value("rootSignature", "");
	  definition.computeShader = pipelineJson.value("computeShader", "");
	  definition.supportBlendModes = pipelineJson.value("supportBlendModes", false);
	  definition.defaultBlendMode = StringToBlendMode(pipelineJson.value("defaultBlendMode", "None"));
	  definition.cullMode = StringToCullMode(pipelineJson.value("cullMode", "Back"));
	  definition.fillMode = StringToFillMode(pipelineJson.value("fillMode", "Solid"));
	  definition.depthEnable = pipelineJson.value("depthEnable", true) ? TRUE : FALSE;
	  definition.depthWriteMask = StringToDepthWriteMask(pipelineJson.value("depthWriteMask", "All"));
	  definition.depthFunc = StringToComparisonFunc(pipelineJson.value("depthFunc", "LessEqual"));
	  definition.topologyType = StringToTopologyType(pipelineJson.value("topologyType", "Triangle"));

	  if (pipelineJson.contains("inputLayout") && pipelineJson["inputLayout"].is_array()) {
		 for (const auto& inputJson : pipelineJson["inputLayout"]) {
			InputElementDefinition element{};
			element.semanticName = inputJson.value("semanticName", "");
			element.semanticIndex = inputJson.value("semanticIndex", 0u);
			element.format = StringToFormat(inputJson.value("format", "R32G32B32A32_FLOAT"));
			element.alignedByteOffset = inputJson.value("alignedByteOffset", static_cast<UINT>(D3D12_APPEND_ALIGNED_ELEMENT));
			element.inputSlot = inputJson.value("inputSlot", 0u);
			element.inputSlotClass = StringToInputClassification(inputJson.value("inputSlotClass", "PerVertexData"));
			element.instanceDataStepRate = inputJson.value("instanceDataStepRate", 0u);
			definition.inputLayout.push_back(element);
		 }
	  }

	  if (!definition.computeShader.empty()) {
		 return CreateComputePipeline(definition.name, definition.computeShader, definition.rootSignature);
	  }

	  return CreatePipelineFromDefinition(definition, rtvFormat);
   } catch (const std::exception& e) {
	  Logger::Error("Exception loading pipeline from " + filePath + ": " + std::string(e.what()));
	  return false;
   }
}

void PSOManager::CreatePredefinedPipelines(OffscreenRenderTarget* offscreenRenderTarget) {
   if (!offscreenRenderTarget) {
	  return;
   }

   const DXGI_FORMAT rtvFormat = offscreenRenderTarget->GetFormat();
   std::vector<std::filesystem::path> rootSignaturePaths;
   std::vector<std::filesystem::path> pipelinePaths;

   const std::filesystem::path pipelineRoot = "resources/pipelines";
   if (!std::filesystem::exists(pipelineRoot)) {
	  return;
   }

   for (const auto& entry : std::filesystem::recursive_directory_iterator(pipelineRoot)) {
	  if (!entry.is_regular_file() || entry.path().extension() != ".json") {
		 continue;
	  }

	  const std::string path = entry.path().generic_string();
	  if (path.find("/rootsig/") != std::string::npos) {
		 rootSignaturePaths.push_back(entry.path());
	  } else if (entry.path().filename() != "pipeline_registry.json") {
		 pipelinePaths.push_back(entry.path());
	  }
   }

   std::sort(rootSignaturePaths.begin(), rootSignaturePaths.end());
   std::sort(pipelinePaths.begin(), pipelinePaths.end());

   for (const auto& path : rootSignaturePaths) {
	  LoadRootSignatureFromFile(path.generic_string());
   }

   for (const auto& path : pipelinePaths) {
	  LoadPipelineFromFile(path.generic_string(), rtvFormat);
   }
}

bool PSOManager::CreateRootSignatureFromDefinition(const RootSignatureDefinition& definition) {
   auto rootSignature = std::make_unique<RootSignature>();
   std::unordered_map<std::string, UINT> semanticSlots;

   for (UINT i = 0; i < static_cast<UINT>(definition.parameters.size()); ++i) {
	  const auto& parameter = definition.parameters[i];
	  const D3D12_DESCRIPTOR_RANGE* descriptorRange = nullptr;
	  UINT descriptorRangeCount = 0;

	  if (parameter.type == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
		 descriptorRange = rootSignature->CreateDescriptorRange(
			parameter.rangeType,
			parameter.descriptorCount,
			parameter.shaderRegister,
			parameter.registerSpace);
		 descriptorRangeCount = 1;
	  }

	  rootSignature->SetRootParameter(
		 parameter.type,
		 parameter.visibility,
		 parameter.shaderRegister,
		 descriptorRange,
		 descriptorRangeCount,
		 parameter.descriptorCount);

	  if (!parameter.semantic.empty()) {
		 semanticSlots[ToLowerString(parameter.semantic)] = i;
	  }
   }

   for (const auto& sampler : definition.samplers) {
	  rootSignature->SetSampler(
		 sampler.filter,
		 sampler.addressU,
		 sampler.addressV,
		 sampler.addressW,
		 sampler.comparisonFunc,
		 sampler.maxLOD,
		 sampler.shaderRegister,
		 sampler.visibility
	  );
   }

   rootSignature->CreateRootSignature(device_->GetDevice());
   rootSignatures_[definition.name] = std::move(rootSignature);
   rootSignatureParameterCounts_[definition.name] = static_cast<uint32_t>(definition.parameters.size());
   rootSignatureSemanticSlots_[definition.name] = std::move(semanticSlots);

   return true;
}

bool PSOManager::CreatePipelineFromDefinition(const PipelineDefinition& definition, DXGI_FORMAT rtvFormat) {
   // ルートシグネチャが存在しない場合は失敗
   if (rootSignatures_.find(definition.rootSignature) == rootSignatures_.end()) {
      Logger::Error("Root signature not found for pipeline: " + definition.name + " (requires: " + definition.rootSignature + ")");
      return false;
   }

   if (definition.supportBlendModes) {
      // ブレンドモード別にパイプラインを作成
      for (int32_t i = 0; i < static_cast<int32_t>(BlendMode::kCount); ++i) {
         BlendMode blendMode = static_cast<BlendMode>(i);

         PipelineConfig config;
         config.vertexShaderName = definition.vertexShader;
         config.pixelShaderName = definition.pixelShader;
         config.rootSignatureName = definition.rootSignature;
         config.blendMode = blendMode;
         config.cullMode = definition.cullMode;
         config.fillMode = definition.fillMode;
         config.depthEnable = definition.depthEnable;
         config.depthFunc = definition.depthFunc;
         config.topologyType = definition.topologyType;
         config.rtvFormat = rtvFormat;
         config.inputElements = definition.inputLayout.empty()
			? BuildInputLayoutFromVertexShaderReflection(definition.vertexShader)
			: definition.inputLayout;

         // 深度書き込みマスクの設定
         if (blendMode == BlendMode::kBlendModeNone) {
            config.depthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
         } else {
            config.depthWriteMask = definition.depthWriteMask;
         }

         std::string pipelineName = CreatePipelineKey(definition.name, blendMode);
         if (!CreateCustomPipeline(pipelineName, config)) {
            Logger::Error("Failed to create pipeline: " + pipelineName);
            return false;
         } else {
            Logger::Info("Successfully created pipeline: " + pipelineName);
            RegisterPipelineSemanticSlots(pipelineName, definition.rootSignature);
         }
      }

	  RegisterPipelineSemanticSlots(definition.name, definition.rootSignature);
   } else {
      // 単一パイプラインを作成
      PipelineConfig config;
      config.vertexShaderName = definition.vertexShader;
      config.pixelShaderName = definition.pixelShader;
      config.rootSignatureName = definition.rootSignature;
      config.blendMode = definition.defaultBlendMode;
      config.cullMode = definition.cullMode;
      config.fillMode = definition.fillMode;
      config.depthEnable = definition.depthEnable;
      config.depthWriteMask = definition.depthWriteMask;
      config.depthFunc = definition.depthFunc;
      config.topologyType = definition.topologyType;
      config.rtvFormat = rtvFormat;
      config.inputElements = definition.inputLayout.empty()
		 ? BuildInputLayoutFromVertexShaderReflection(definition.vertexShader)
		 : definition.inputLayout;

      if (!CreateCustomPipeline(definition.name, config)) {
         Logger::Error("Failed to create pipeline: " + definition.name);
         return false;
      } else {
         Logger::Info("Successfully created pipeline: " + definition.name);
         RegisterPipelineSemanticSlots(definition.name, definition.rootSignature);
      }
   }

   return true;
}

bool PSOManager::CreateCustomPipeline(const std::string& name, const PipelineConfig& config) {
   auto* vertexShader = shaderManager_->GetVertexShader(config.vertexShaderName);
   auto* pixelShader = shaderManager_->GetPixelShader(config.pixelShaderName);

   if (!vertexShader || !pixelShader) {
      Logger::Error("Shaders not found for pipeline: " + name + " (VS: " + config.vertexShaderName + ", PS: " + config.pixelShaderName + ")");
      return false;
   }

   auto* rootSignature = GetRootSignature(config.rootSignatureName);
   if (!rootSignature) {
      Logger::Error("Root signature not found for pipeline: " + name + " (requires: " + config.rootSignatureName + ")");
      return false;
   }

   auto pipeline = std::make_unique<PipelineState>();
   pipeline->SetRootSignature(rootSignature);

   PipelineReflectionMetadata reflectionMetadata{};
   if (const auto* vsReflection = shaderManager_->GetShaderReflection(config.vertexShaderName, ShaderType::Vertex);
	  vsReflection && vsReflection->isValid) {
	  reflectionMetadata.hasVertexReflection = true;
	  reflectionMetadata.vertexResourceCount = static_cast<uint32_t>(vsReflection->boundResources.size());
   }
   if (const auto* psReflection = shaderManager_->GetShaderReflection(config.pixelShaderName, ShaderType::Pixel);
	  psReflection && psReflection->isValid) {
	  reflectionMetadata.hasPixelReflection = true;
	  reflectionMetadata.pixelResourceCount = static_cast<uint32_t>(psReflection->boundResources.size());
   }

  const std::vector<std::string> expectedSemantics = bindingLayoutResolver_.GetExpectedSemanticsForRootSignature(config.rootSignatureName);

   std::string pipelineLookupName = name;
   const size_t suffixPos = pipelineLookupName.rfind('_');
   if (suffixPos != std::string::npos && suffixPos + 1 < pipelineLookupName.size()) {
	  const bool hasNumericSuffix = std::all_of(
		 pipelineLookupName.begin() + static_cast<std::ptrdiff_t>(suffixPos + 1),
		 pipelineLookupName.end(),
		 [](unsigned char c) { return std::isdigit(c) != 0; });
	  if (hasNumericSuffix) {
		 pipelineLookupName = pipelineLookupName.substr(0, suffixPos);
	  }
   }

   uint32_t validationWarnings = 0;
   const auto* rootTable = shaderManager_->GetPipelineRootParameterTable(pipelineLookupName);
   for (const auto& semantic : expectedSemantics) {
    const bool resolved = rootTable && rootTable->slotBySemanticName.contains(semantic);
	  if (!resolved) {
		 ++validationWarnings;
      reflectionMetadata.missingSemantics.push_back(semantic);
		 LogValidationMessage(
			"missing_semantic:" + name + ":" + semantic,
			"[PSOManager] Binding validation warning in pipeline: " + name +
			" missing semantic='" + semantic + "' (rootSignature='" + config.rootSignatureName + "')",
			Logger::LogLevel::Warning);
	  }
   }

   reflectionMetadata.estimatedRequiredBindingCount = reflectionMetadata.vertexResourceCount + reflectionMetadata.pixelResourceCount;

   auto rootParamCountIt = rootSignatureParameterCounts_.find(config.rootSignatureName);
   if (rootParamCountIt != rootSignatureParameterCounts_.end()) {
	  reflectionMetadata.rootParameterCount = rootParamCountIt->second;
	  reflectionMetadata.hasPotentialBindingMismatch =
		 reflectionMetadata.estimatedRequiredBindingCount > reflectionMetadata.rootParameterCount;

	  if (reflectionMetadata.hasPotentialBindingMismatch) {
      ++validationWarnings;
      LogValidationMessage(
			"binding_mismatch:" + name,
			"[PSOManager] Potential binding mismatch in pipeline: " + name +
			" (required=" + std::to_string(reflectionMetadata.estimatedRequiredBindingCount) +
			", rootParameters=" + std::to_string(reflectionMetadata.rootParameterCount) + ")",
			Logger::LogLevel::Warning);
	  }
   }

   reflectionMetadata.validationWarningCount = validationWarnings;

   if (validationWarnings == 0) {
   LogValidationMessage("binding_pass:" + name, "[PSOManager] Binding validation passed: " + name, Logger::LogLevel::Info);
   } else {
      LogValidationMessage(
		 "binding_report:" + name,
		 "[PSOManager] Binding validation report: pipeline=" + name +
		 ", warnings=" + std::to_string(validationWarnings) +
		 ", expectedSemantics=" + std::to_string(expectedSemantics.size()) +
		 ", reflectedResources=" + std::to_string(reflectionMetadata.estimatedRequiredBindingCount) +
		 ", rootParameters=" + std::to_string(reflectionMetadata.rootParameterCount),
         validationWarnings >= 3 ? Logger::LogLevel::Error : Logger::LogLevel::Warning);
   }

   // 入力レイアウトの設定
   for (const auto& element : config.inputElements) {
      pipeline->SetInputLayOut(
         element.semanticName.c_str(),
         element.semanticIndex,
         element.format,
         element.alignedByteOffset,
         element.inputSlot,
         element.inputSlotClass,
         element.instanceDataStepRate
      );
   }

   // パイプラインステートの設定
   pipeline->SetBlendState(config.blendMode);
   pipeline->SetRasterizer(config.cullMode, config.fillMode);
   pipeline->SetDepthStencil(config.depthEnable, config.depthWriteMask, config.depthFunc);
   pipeline->SetShaders(vertexShader, pixelShader);
   pipeline->SetRTVFormat(config.rtvFormat);
   pipeline->SetPrimitiveTopologyType(config.topologyType);

   // PIXなどのデバッグ用にPSOへ名前を設定
   pipeline->SetName(std::wstring(name.begin(), name.end()));

   // パイプラインの作成
   pipeline->CreatePipelineState(device_->GetDevice());

   pipelineLibrary_.StoreGraphicsPipeline(name, std::move(pipeline));
   pipelineReflectionMetadata_[name] = reflectionMetadata;
   Logger::Info("Pipeline created and stored: " + name);
   return true;
}

bool PSOManager::CreateComputePipeline(const std::string& name, const std::string& computeShaderName, const std::string& rootSignatureName) {
   if (!shaderManager_ || !shaderManager_->GetComputeShader(computeShaderName)) {
	  Logger::Error("Compute shader not found for pipeline: " + name + " (CS: " + computeShaderName + ")");
	  return false;
   }

   if (!GetRootSignature(rootSignatureName)) {
	  Logger::Error("Root signature not found for compute pipeline: " + name + " (requires: " + rootSignatureName + ")");
	  return false;
   }

   pipelineLibrary_.StoreComputePipeline({ name, rootSignatureName, computeShaderName });
   Logger::Info("Compute pipeline definition registered: " + name);
   return true;
}

PipelineState* PSOManager::GetPipeline(const std::string& name, BlendMode blendMode) {
   std::string key = CreatePipelineKey(name, blendMode);
   if (auto* pipeline = pipelineLibrary_.GetGraphicsPipeline(key)) {
	  return pipeline;
   }

   // ブレンドモードが指定されていても見つからない場合は、名前のみで検索
   return pipelineLibrary_.GetGraphicsPipeline(name);
}

const ComputePipelineDefinition* PSOManager::GetComputePipeline(const std::string& name) const {
   return pipelineLibrary_.GetComputePipeline(name);
}

std::optional<UINT> PSOManager::ResolvePipelineRootParameter(const std::string& pipelineName, const std::string& semantic) const {
   const std::string normalizedSemantic = ToLowerString(semantic);

   auto it = pipelineSemanticSlots_.find(pipelineName);
   if (it != pipelineSemanticSlots_.end()) {
	  auto slotIt = it->second.find(normalizedSemantic);
	  if (slotIt != it->second.end()) {
		 return slotIt->second;
	  }
   }

   const size_t suffixPos = pipelineName.rfind('_');
   if (suffixPos != std::string::npos && suffixPos + 1 < pipelineName.size()) {
	  const bool hasNumericSuffix = std::all_of(
		 pipelineName.begin() + static_cast<std::ptrdiff_t>(suffixPos + 1),
		 pipelineName.end(),
		 [](unsigned char c) { return std::isdigit(c) != 0; });
	  if (hasNumericSuffix) {
		 const std::string baseName = pipelineName.substr(0, suffixPos);
		 auto baseIt = pipelineSemanticSlots_.find(baseName);
		 if (baseIt != pipelineSemanticSlots_.end()) {
			auto slotIt = baseIt->second.find(normalizedSemantic);
			if (slotIt != baseIt->second.end()) {
			   return slotIt->second;
			}
		 }
	  }
   }

   return std::nullopt;
}

RootSignature* PSOManager::GetRootSignature(const std::string& name) {
   auto it = rootSignatures_.find(name);
   return (it != rootSignatures_.end()) ? it->second.get() : nullptr;
}

const PipelineReflectionMetadata* PSOManager::GetPipelineReflectionMetadata(const std::string& name) const {
   auto it = pipelineReflectionMetadata_.find(name);
   if (it != pipelineReflectionMetadata_.end()) {
	  return &it->second;
   }

   auto itNoBlend = pipelineReflectionMetadata_.find(CreatePipelineKey(name, BlendMode::kBlendModeNone));
   return (itNoBlend != pipelineReflectionMetadata_.end()) ? &itNoBlend->second : nullptr;
}

ValidationSummary PSOManager::GetValidationSummary() const {
   ValidationSummary summary{};
   summary.pipelineCount = static_cast<uint32_t>(pipelineReflectionMetadata_.size());

   for (const auto& [_, metadata] : pipelineReflectionMetadata_) {
	  summary.totalWarnings += metadata.validationWarningCount;
	  if (metadata.validationWarningCount > 0) {
		 ++summary.warningPipelines;
	  }
   }

   return summary;
}

nlohmann::json PSOManager::BuildValidationReportJson() const {
   nlohmann::json report = nlohmann::json::object();
   const ValidationSummary summary = GetValidationSummary();

   report["summary"] = {
	  {"pipelineCount", summary.pipelineCount},
	  {"warningPipelines", summary.warningPipelines},
	  {"totalWarnings", summary.totalWarnings}
   };

   report["pipelines"] = nlohmann::json::array();
   for (const auto& [pipelineName, metadata] : pipelineReflectionMetadata_) {
	  nlohmann::json entry = {
		 {"name", pipelineName},
		 {"hasVertexReflection", metadata.hasVertexReflection},
		 {"hasPixelReflection", metadata.hasPixelReflection},
		 {"vertexResourceCount", metadata.vertexResourceCount},
		 {"pixelResourceCount", metadata.pixelResourceCount},
		 {"estimatedRequiredBindingCount", metadata.estimatedRequiredBindingCount},
		 {"rootParameterCount", metadata.rootParameterCount},
		 {"hasPotentialBindingMismatch", metadata.hasPotentialBindingMismatch},
		 {"validationWarningCount", metadata.validationWarningCount},
		 {"missingSemantics", metadata.missingSemantics}
	  };
	  report["pipelines"].push_back(std::move(entry));
   }

   return report;
}

bool PSOManager::SaveValidationReportJson(const std::string& filePath) const {
   try {
	  std::filesystem::path path(filePath);
	  if (path.has_parent_path()) {
		 std::filesystem::create_directories(path.parent_path());
	  }

	  std::ofstream ofs(path);
	  if (!ofs.is_open()) {
		 return false;
	  }

	  ofs << BuildValidationReportJson().dump(2);
	  return true;
   } catch (...) {
	  return false;
   }
}

void PSOManager::Clear() {
   pipelineLibrary_.Clear();
   rootSignatures_.clear();
   pipelineReflectionMetadata_.clear();
   rootSignatureParameterCounts_.clear();
   rootSignatureSemanticSlots_.clear();
   pipelineSemanticSlots_.clear();
}

std::string PSOManager::CreatePipelineKey(const std::string& name, BlendMode blendMode) const {
   return name + "_" + std::to_string(static_cast<int>(blendMode));
}

std::vector<InputElementDefinition> PSOManager::BuildInputLayoutFromVertexShaderReflection(const std::string& vertexShaderName) const {
   std::vector<InputElementDefinition> layout;
   if (!shaderManager_) {
	  return layout;
   }

   const auto* reflection = shaderManager_->GetShaderReflection(vertexShaderName, ShaderType::Vertex);
   if (!reflection || !reflection->isValid) {
	  return layout;
   }

   layout.reserve(reflection->inputParameters.size());
   for (const auto& input : reflection->inputParameters) {
	  std::string semantic = input.semanticName;
	  std::transform(semantic.begin(), semantic.end(), semantic.begin(), [](unsigned char c) {
		 return static_cast<char>(std::toupper(c));
	  });

	  if (semantic.rfind("SV_", 0) == 0) {
		 continue;
	  }

	  InputElementDefinition element{};
	  element.semanticName = input.semanticName;
	  element.semanticIndex = input.semanticIndex;
	  element.format = ConvertReflectionInputToFormat(input.mask, input.componentType);
	  element.alignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	  element.inputSlot = 0;
	  element.inputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	  element.instanceDataStepRate = 0;
	  layout.push_back(std::move(element));
   }

   return layout;
}

DXGI_FORMAT PSOManager::ConvertReflectionInputToFormat(BYTE mask, D3D_REGISTER_COMPONENT_TYPE componentType) const {
   const UINT componentCount =
	  (mask == 1) ? 1 :
	  (mask <= 3) ? 2 :
	  (mask <= 7) ? 3 : 4;

   switch (componentType) {
   case D3D_REGISTER_COMPONENT_UINT32:
	  switch (componentCount) {
	  case 1: return DXGI_FORMAT_R32_UINT;
	  case 2: return DXGI_FORMAT_R32G32_UINT;
	  case 3: return DXGI_FORMAT_R32G32B32_UINT;
	  default: return DXGI_FORMAT_R32G32B32A32_UINT;
	  }
   case D3D_REGISTER_COMPONENT_SINT32:
	  switch (componentCount) {
	  case 1: return DXGI_FORMAT_R32_SINT;
	  case 2: return DXGI_FORMAT_R32G32_SINT;
	  case 3: return DXGI_FORMAT_R32G32B32_SINT;
	  default: return DXGI_FORMAT_R32G32B32A32_SINT;
	  }
   case D3D_REGISTER_COMPONENT_FLOAT32:
   default:
	  switch (componentCount) {
	  case 1: return DXGI_FORMAT_R32_FLOAT;
	  case 2: return DXGI_FORMAT_R32G32_FLOAT;
	  case 3: return DXGI_FORMAT_R32G32B32_FLOAT;
	  default: return DXGI_FORMAT_R32G32B32A32_FLOAT;
	  }
   }
}

std::optional<UINT> PSOManager::ResolveShaderRegisterBySemantic(
   const std::string& semantic,
   D3D12_ROOT_PARAMETER_TYPE parameterType,
   D3D12_SHADER_VISIBILITY visibility,
   D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
   const std::string& vertexShader,
   const std::string& pixelShader,
   const std::string& computeShader) const {
   if (!shaderManager_ || semantic.empty()) {
	  return std::nullopt;
   }

   const auto normalizeIdentifier = [](std::string value) {
	  value = ToLowerString(std::move(value));
	  value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
		 return !std::isalnum(c);
	  }), value.end());
	  if (!value.empty() && value.front() == 'g') {
		 value.erase(value.begin());
	  }
	  return value;
   };

   const std::string target = normalizeIdentifier(semantic);

   const auto matchResourceType = [parameterType, rangeType](D3D_SHADER_INPUT_TYPE type) {
	  if (parameterType == D3D12_ROOT_PARAMETER_TYPE_CBV) {
		 return type == D3D_SIT_CBUFFER;
	  }
	  if (parameterType == D3D12_ROOT_PARAMETER_TYPE_SRV ||
		 (parameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)) {
		 return type == D3D_SIT_TEXTURE || type == D3D_SIT_STRUCTURED || type == D3D_SIT_TBUFFER || type == D3D_SIT_BYTEADDRESS;
	  }
	  if (parameterType == D3D12_ROOT_PARAMETER_TYPE_UAV ||
		 (parameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)) {
		 return type == D3D_SIT_UAV_RWTYPED || type == D3D_SIT_UAV_RWSTRUCTURED || type == D3D_SIT_UAV_RWBYTEADDRESS;
	  }
	  return true;
   };

   const auto searchShader = [&](const std::string& shaderName, ShaderType stage) -> std::optional<UINT> {
	  if (shaderName.empty()) {
		 return std::nullopt;
	  }
	  const auto* reflection = shaderManager_->GetShaderReflection(shaderName, stage);
	  if (!reflection || !reflection->isValid) {
		 return std::nullopt;
	  }

	  for (const auto& resource : reflection->boundResources) {
         const std::string candidate = normalizeIdentifier(resource.name);
		 if (candidate != target && candidate.find(target) == std::string::npos) {
			continue;
		 }
		 if (!matchResourceType(resource.type)) {
			continue;
		 }
		 return resource.bindPoint;
	  }
	  return std::nullopt;
   };

   if (visibility == D3D12_SHADER_VISIBILITY_VERTEX || visibility == D3D12_SHADER_VISIBILITY_ALL) {
	  if (auto slot = searchShader(vertexShader, ShaderType::Vertex); slot.has_value()) {
		 return slot;
	  }
   }
   if (visibility == D3D12_SHADER_VISIBILITY_PIXEL || visibility == D3D12_SHADER_VISIBILITY_ALL) {
	  if (auto slot = searchShader(pixelShader, ShaderType::Pixel); slot.has_value()) {
		 return slot;
	  }
   }
   if (visibility == D3D12_SHADER_VISIBILITY_ALL) {
	  if (auto slot = searchShader(computeShader, ShaderType::Compute); slot.has_value()) {
		 return slot;
	  }
   }

   if (visibility == D3D12_SHADER_VISIBILITY_ALL && !computeShader.empty()) {
	  if (auto slot = searchShader(computeShader, ShaderType::Compute); slot.has_value()) {
		 return slot;
	  }
   }

   return std::nullopt;
}

void PSOManager::RegisterPipelineSemanticSlots(const std::string& pipelineName, const std::string& rootSignatureName) {
   auto it = rootSignatureSemanticSlots_.find(rootSignatureName);
   if (it == rootSignatureSemanticSlots_.end()) {
	  return;
   }

   pipelineSemanticSlots_[pipelineName] = it->second;
}

}
