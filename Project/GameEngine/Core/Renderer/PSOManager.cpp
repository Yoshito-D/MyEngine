#include "pch.h"
#include "PSOManager.h"
#include "ShaderManager.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/OffscreenRenderTarget.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

namespace {
// 文字列からD3D12列挙型への変換ヘルパー
D3D12_CULL_MODE StringToCullMode(const std::string& str) {
   if (str == "None") return D3D12_CULL_MODE_NONE;
   if (str == "Front") return D3D12_CULL_MODE_FRONT;
   if (str == "Back") return D3D12_CULL_MODE_BACK;
   return D3D12_CULL_MODE_BACK;
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
}

bool PSOManager::LoadPipelineDefinitions(const std::wstring& definitionFilePath, DXGI_FORMAT rtvFormat) {
   std::string path = WStringToString(definitionFilePath);
   std::ifstream file(path);
   if (!file.is_open()) {
	  // ファイルが開けない場合は事前定義を使用
	  return false;
   }

   try {
	  json registryJson;
	  file >> registryJson;

	  // ルートシグネチャをロード
	  if (registryJson.contains("rootSignatures")) {
		 for (const auto& rootSigPath : registryJson["rootSignatures"]) {
			std::string rsPath = rootSigPath.get<std::string>();
			if (!LoadRootSignatureFromFile(rsPath)) {
			   // ロード失敗時はログ出力など
			}
		 }
	  }

	  // パイプラインをロード
	  if (registryJson.contains("pipelines")) {
		 for (const auto& pipelinePath : registryJson["pipelines"]) {
			std::string plPath = pipelinePath.get<std::string>();
			if (!LoadPipelineFromFile(plPath, rtvFormat)) {
			   // ロード失敗時はログ出力など
			}
		 }
	  }

	  return true;
   }
   catch (const std::exception& e) {
	  // JSON解析エラー
	  (void)e; // 未使用警告を抑制
	  return false;
   }
}

bool PSOManager::LoadRootSignatureFromFile(const std::string& filePath) {
   std::ifstream file(filePath);
   if (!file.is_open()) {
      Logger::GetInstance().Log("Failed to open root signature file: " + filePath, Logger::LogLevel::Error);
      return false;
   }

   try {
      json rootSigJson;
      file >> rootSigJson;

      RootSignatureDefinition definition;
      definition.name = rootSigJson["name"].get<std::string>();

      // パラメータをロード
      if (rootSigJson.contains("parameters")) {
         for (const auto& param : rootSigJson["parameters"]) {
            RootParameterDefinition paramDef;
            paramDef.type = StringToRootParameterType(param["type"].get<std::string>());
            paramDef.visibility = StringToShaderVisibility(param["visibility"].get<std::string>());
            paramDef.shaderRegister = param["shaderRegister"].get<UINT>();
            paramDef.registerSpace = param.value("registerSpace", 0);
            paramDef.descriptorCount = param.value("descriptorCount", 1);
            
            // ディスクリプタテーブルの場合、rangeTypeを読み込む
            if (paramDef.type == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
               if (param.contains("rangeType")) {
                  paramDef.rangeType = StringToRangeType(param["rangeType"].get<std::string>());
                  Logger::GetInstance().Log("  Parameter " + std::to_string(definition.parameters.size()) + 
                     ": DescriptorTable with rangeType=" + param["rangeType"].get<std::string>() + 
                     " (value=" + std::to_string(static_cast<int>(paramDef.rangeType)) + ")");
               } else {
                  Logger::GetInstance().Log("  Parameter " + std::to_string(definition.parameters.size()) + 
                     ": DescriptorTable without rangeType - using default SRV", Logger::LogLevel::Warning);
               }
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

      Logger::GetInstance().Log("Loaded root signature definition: " + definition.name + " with " + std::to_string(definition.parameters.size()) + " parameters");
      return CreateRootSignatureFromDefinition(definition);
   }
   catch (const std::exception& e) {
      Logger::GetInstance().Log("Exception loading root signature from " + filePath + ": " + std::string(e.what()), Logger::LogLevel::Error);
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
	  definition.vertexShader = pipelineJson["vertexShader"].get<std::string>();
	  definition.pixelShader = pipelineJson["pixelShader"].get<std::string>();
	  definition.rootSignature = pipelineJson["rootSignature"].get<std::string>();
	  definition.supportBlendModes = pipelineJson.value("supportBlendModes", false);
	  definition.defaultBlendMode = StringToBlendMode(pipelineJson.value("defaultBlendMode", "None"));
	  definition.cullMode = StringToCullMode(pipelineJson.value("cullMode", "Back"));
	  definition.fillMode = StringToFillMode(pipelineJson.value("fillMode", "Solid"));
	  definition.depthEnable = pipelineJson.value("depthEnable", true);
	  definition.depthWriteMask = StringToDepthWriteMask(pipelineJson.value("depthWriteMask", "All"));
	  definition.depthFunc = StringToComparisonFunc(pipelineJson.value("depthFunc", "LessEqual"));
	  definition.topologyType = StringToTopologyType(pipelineJson.value("topologyType", "Triangle"));

	  // 入力レイアウトをロード
	  if (pipelineJson.contains("inputLayout")) {
		 for (const auto& elem : pipelineJson["inputLayout"]) {
			InputElementDefinition elemDef;
			elemDef.semanticName = elem["semanticName"].get<std::string>();
			elemDef.semanticIndex = elem["semanticIndex"].get<UINT>();
			elemDef.format = StringToFormat(elem["format"].get<std::string>());
			elemDef.alignedByteOffset = elem.value("alignedByteOffset", D3D12_APPEND_ALIGNED_ELEMENT);
			elemDef.inputSlot = elem.value("inputSlot", 0);
			elemDef.inputSlotClass = StringToInputClassification(elem.value("inputSlotClass", "PerVertexData"));
			elemDef.instanceDataStepRate = elem.value("instanceDataStepRate", 0);
			definition.inputLayout.push_back(elemDef);
		 }
	  }

	  return CreatePipelineFromDefinition(definition, rtvFormat);
   }
   catch (const std::exception& e) {
	  (void)e;
	  return false;
   }
}

void PSOManager::CreatePredefinedPipelines(OffscreenRenderTarget* offscreenRenderTarget) {
   DXGI_FORMAT rtvFormat = offscreenRenderTarget->GetFormat();

   // まずJSONから読み込みを試行
   if (LoadPipelineDefinitions(L"resources/pipelines/pipeline_registry.json", rtvFormat)) {
	  return; // 成功したら終了
   }

   // JSONロードに失敗した場合はハードコーディング定義を使用
   // ルートシグネチャ定義
   std::vector<RootSignatureDefinition> rootSigDefs = {
	  // Object3D用ルートシグネチャ
	  {
		  "Object3D",
		  {
			  { D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_PIXEL, 0 },
			  { D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_VERTEX, 0 },
			  { D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, D3D12_SHADER_VISIBILITY_PIXEL, 0, 0, 1 },
			  { D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_PIXEL, 1 },
			  { D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_PIXEL, 2 },
			  { D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_PIXEL, 3 },
			  { D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_PIXEL, 4 }
		  },
		  {
			  { D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				D3D12_COMPARISON_FUNC_NEVER, D3D12_FLOAT32_MAX, 0, D3D12_SHADER_VISIBILITY_PIXEL }
		  }
	  },
	  // Line3D用ルートシグネチャ
	  {
		  "Line3D",
		  {
			  { D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_VERTEX, 0 }
		  },
		  {}
	  },
	  // Particle用ルートシグネチャ
	  {
		  "Particle",
		  {
			  { D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_PIXEL, 0 },
			  { D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, D3D12_SHADER_VISIBILITY_VERTEX, 0, 0, 1 },
			  { D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, D3D12_SHADER_VISIBILITY_PIXEL, 1, 0, 1 }
		  },
		  {
			  { D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				D3D12_COMPARISON_FUNC_NEVER, D3D12_FLOAT32_MAX, 0, D3D12_SHADER_VISIBILITY_PIXEL }
		  }
	  },
	  // FullscreenTriangle用ルートシグネチャ
	  {
		  "FullscreenTriangle",
		  {
			  { D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, D3D12_SHADER_VISIBILITY_PIXEL, 0, 0, 1 }
		  },
		  {
			  { D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_COMPARISON_FUNC_NEVER, D3D12_FLOAT32_MAX, 0, D3D12_SHADER_VISIBILITY_PIXEL }
		  }
	  }
   };

   // ルートシグネチャを作成
   for (const auto& rootSigDef : rootSigDefs) {
	  CreateRootSignatureFromDefinition(rootSigDef);
   }

   // パイプライン定義
   std::vector<PipelineDefinition> pipelineDefs = {
	  // Object3D
	  {
		  "Object3D", "Object3D", "Object3D", "Object3D",
		  {
			  { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_APPEND_ALIGNED_ELEMENT, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			  { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, D3D12_APPEND_ALIGNED_ELEMENT, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			  { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, D3D12_APPEND_ALIGNED_ELEMENT, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		  },
		  true, BlendMode::kBlendModeNone, D3D12_CULL_MODE_BACK
	  },
	  // Sprite
	  {
		  "Sprite", "Object3D", "Object3D", "Object3D",
		  {
			  { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_APPEND_ALIGNED_ELEMENT, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			  { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, D3D12_APPEND_ALIGNED_ELEMENT, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			  { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, D3D12_APPEND_ALIGNED_ELEMENT, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		  },
		  true, BlendMode::kBlendModeNone, D3D12_CULL_MODE_NONE
	  },
	  // Line3D
	  {
		  "Line3D", "Line3D", "Line3D", "Line3D",
		  {
			  { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, D3D12_APPEND_ALIGNED_ELEMENT, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			  { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_APPEND_ALIGNED_ELEMENT, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		  },
		  false, BlendMode::kBlendModeNormal, D3D12_CULL_MODE_NONE, D3D12_FILL_MODE_SOLID,
		  TRUE, D3D12_DEPTH_WRITE_MASK_ZERO, D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE
	  },
	  // Particle
	  {
		  "Particle", "Particle", "Particle", "Particle",
		  {
			  { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_APPEND_ALIGNED_ELEMENT, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			  { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, D3D12_APPEND_ALIGNED_ELEMENT, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			  { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_APPEND_ALIGNED_ELEMENT, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		  },
		  true, BlendMode::kBlendModeNone, D3D12_CULL_MODE_NONE, D3D12_FILL_MODE_SOLID,
		  TRUE, D3D12_DEPTH_WRITE_MASK_ZERO
	  },
	  // FullscreenTriangle
	  {
		  "FullscreenTriangle", "FullscreenTriangle", "FullscreenTriangle", "FullscreenTriangle",
		  {},
		  false, BlendMode::kBlendModeNone, D3D12_CULL_MODE_NONE
	  }
   };

   // パイプラインを作成
   for (const auto& pipelineDef : pipelineDefs) {
	  CreatePipelineFromDefinition(pipelineDef, rtvFormat);
   }
}

bool PSOManager::CreateRootSignatureFromDefinition(const RootSignatureDefinition& definition) {
   // 既に存在する場合はスキップ
   if (rootSignatures_.find(definition.name) != rootSignatures_.end()) {
	  return true;
   }

   auto rootSignature = std::make_unique<RootSignature>();

   // ディスクリプタレンジのポインタを保持する配列
   std::vector<const D3D12_DESCRIPTOR_RANGE*> descriptorRangePointers;
   descriptorRangePointers.reserve(definition.parameters.size());

   // 1. すべてのディスクリプタレンジを先に作成（ベクターの再配置を防ぐ）
   for (const auto& param : definition.parameters) {
	  if (param.type == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
		 const D3D12_DESCRIPTOR_RANGE* range = rootSignature->CreateDescriptorRange(
			param.rangeType,
			param.descriptorCount,
			param.shaderRegister,
			param.registerSpace
		 );
		 descriptorRangePointers.push_back(range);
	  } else {
		 descriptorRangePointers.push_back(nullptr);
	  }
   }

   // 2. ルートパラメータを設定
   for (size_t i = 0; i < definition.parameters.size(); ++i) {
	  const auto& param = definition.parameters[i];
	  
	  if (param.type == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
		 rootSignature->SetRootParameter(
			param.type,
			param.visibility,
			param.shaderRegister,
			descriptorRangePointers[i],
			1  // 1つのレンジ
		 );
	  } else {
		 rootSignature->SetRootParameter(
			param.type,
			param.visibility,
			param.shaderRegister
		 );
	  }
   }

   // サンプラーを設定
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

   return true;
}

bool PSOManager::CreatePipelineFromDefinition(const PipelineDefinition& definition, DXGI_FORMAT rtvFormat) {
   // ルートシグネチャが存在しない場合は失敗
   if (rootSignatures_.find(definition.rootSignature) == rootSignatures_.end()) {
      Logger::GetInstance().Log("Root signature not found for pipeline: " + definition.name + " (requires: " + definition.rootSignature + ")", Logger::LogLevel::Error);
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
         config.inputElements = definition.inputLayout;

         // 深度書き込みマスクの設定
         if (blendMode == BlendMode::kBlendModeNone) {
            config.depthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
         } else {
            config.depthWriteMask = definition.depthWriteMask;
         }

         std::string pipelineName = CreatePipelineKey(definition.name, blendMode);
         if (!CreateCustomPipeline(pipelineName, config)) {
            Logger::GetInstance().Log("Failed to create pipeline: " + pipelineName, Logger::LogLevel::Error);
            return false;
         } else {
            Logger::GetInstance().Log("Successfully created pipeline: " + pipelineName);
         }
      }
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
      config.inputElements = definition.inputLayout;

      if (!CreateCustomPipeline(definition.name, config)) {
         Logger::GetInstance().Log("Failed to create pipeline: " + definition.name, Logger::LogLevel::Error);
         return false;
      } else {
         Logger::GetInstance().Log("Successfully created pipeline: " + definition.name);
      }
   }

   return true;
}

bool PSOManager::CreateCustomPipeline(const std::string& name, const PipelineConfig& config) {
   auto* vertexShader = shaderManager_->GetVertexShader(config.vertexShaderName);
   auto* pixelShader = shaderManager_->GetPixelShader(config.pixelShaderName);

   if (!vertexShader || !pixelShader) {
      Logger::GetInstance().Log("Shaders not found for pipeline: " + name + " (VS: " + config.vertexShaderName + ", PS: " + config.pixelShaderName + ")", Logger::LogLevel::Error);
      return false;
   }

   auto* rootSignature = GetRootSignature(config.rootSignatureName);
   if (!rootSignature) {
      Logger::GetInstance().Log("Root signature not found for pipeline: " + name + " (requires: " + config.rootSignatureName + ")", Logger::LogLevel::Error);
      return false;
   }

   auto pipeline = std::make_unique<PipelineState>();
   pipeline->SetRootSignature(rootSignature);

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

   // パイプラインの作成
   pipeline->CreatePipelineState(device_->GetDevice());

   pipelines_[name] = std::move(pipeline);
   Logger::GetInstance().Log("Pipeline created and stored: " + name);
   return true;
}

PipelineState* PSOManager::GetPipeline(const std::string& name, BlendMode blendMode) {
   std::string key = CreatePipelineKey(name, blendMode);
   auto it = pipelines_.find(key);
   if (it != pipelines_.end()) {
	  return it->second.get();
   }

   // ブレンドモードが指定されていても見つからない場合は、名前のみで検索
   it = pipelines_.find(name);
   return (it != pipelines_.end()) ? it->second.get() : nullptr;
}

RootSignature* PSOManager::GetRootSignature(const std::string& name) {
   auto it = rootSignatures_.find(name);
   return (it != rootSignatures_.end()) ? it->second.get() : nullptr;
}

void PSOManager::Clear() {
   pipelines_.clear();
   rootSignatures_.clear();
}

std::string PSOManager::CreatePipelineKey(const std::string& name, BlendMode blendMode) {
   return name + "_" + std::to_string(static_cast<int>(blendMode));
}
}