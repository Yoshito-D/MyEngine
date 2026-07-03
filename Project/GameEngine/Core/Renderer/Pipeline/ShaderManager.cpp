#include "pch.h"
#include "ShaderManager.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/ShaderCompiler.h"
#include "RootBindingSlots.h"
#include "BindingLayoutResolver.h"
#include "Utility/Logger.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <functional>

using json = nlohmann::json;

namespace {

std::wstring Utf8ToWString(const std::string& str) {
   if (str.empty()) {
	  return {};
   }

   const int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
   if (size <= 0) {
	  return {};
   }

   std::wstring result(static_cast<size_t>(size), L'\0');
   MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, result.data(), size);
   if (!result.empty() && result.back() == L'\0') {
	  result.pop_back();
   }
   return result;
}

std::string ToLowerString(std::string value) {
   std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
	  return static_cast<char>(std::tolower(c));
   });
   return value;
}

bool TryParseShaderType(const std::string& text, GameEngine::ShaderType& outType) {
   const std::string lower = ToLowerString(text);
   if (lower == "vertex" || lower == "vs") {
	  outType = GameEngine::ShaderType::Vertex;
	  return true;
   }
   if (lower == "pixel" || lower == "ps") {
	  outType = GameEngine::ShaderType::Pixel;
	  return true;
   }
   if (lower == "compute" || lower == "cs") {
	  outType = GameEngine::ShaderType::Compute;
	  return true;
   }
   if (lower == "geometry" || lower == "gs") {
	  outType = GameEngine::ShaderType::Geometry;
	  return true;
   }
   if (lower == "hull" || lower == "hs") {
	  outType = GameEngine::ShaderType::Hull;
	  return true;
   }
   if (lower == "domain" || lower == "ds") {
	  outType = GameEngine::ShaderType::Domain;
	  return true;
   }

   return false;
}

std::string NormalizePipelineName(std::string pipelineName) {
   if (pipelineName.rfind("PostProcess_", 0) == 0) {
	  pipelineName = pipelineName.substr(std::string("PostProcess_").size());
   }
   return pipelineName;
}
}

namespace GameEngine {
void ShaderManager::Initialize(GraphicsDevice* device) {
   device_ = device;
   InitializeDXC();
   if (!LoadShaderRegistry()) {
	  LoadPredefinedShaders();
   }
   BuildPipelineRootParameterTables();
}

bool ShaderManager::LoadShaderRegistry(const std::wstring& registryFilePath) {
   const std::string registryPath = std::filesystem::path(registryFilePath).string();
   std::ifstream inputStream;
   inputStream.open(registryPath);
   if (!inputStream.is_open()) {
	  return false;
   }

   try {
	  json root;
      inputStream >> root;

	  if (!root.contains("shaders") || !root["shaders"].is_array()) {
		 return false;
	  }

	  bool loadedAny = false;
	  bool allSucceeded = true;

	  for (const auto& shaderJson : root["shaders"]) {
		 if (!shaderJson.is_object()) {
			continue;
		 }

		 ShaderInfo info;
		 info.name = shaderJson.value("name", "");
		 if (info.name.empty()) {
			allSucceeded = false;
			continue;
		 }

		 std::string typeString = shaderJson.value("type", "");
		 if (!TryParseShaderType(typeString, info.type)) {
			allSucceeded = false;
			continue;
		 }

		 std::string filePathString = shaderJson.value("filePath", shaderJson.value("file", shaderJson.value("path", "")));
		 if (filePathString.empty()) {
			allSucceeded = false;
			continue;
		 }
		 info.filePath = Utf8ToWString(filePathString);

		 const std::string entryPointString = shaderJson.value("entryPoint", "main");
		 info.entryPoint = Utf8ToWString(entryPointString);
		 if (info.entryPoint.empty()) {
			info.entryPoint = L"main";
		 }

		 if (shaderJson.contains("defines") && shaderJson["defines"].is_array()) {
			for (const auto& define : shaderJson["defines"]) {
			   if (define.is_string()) {
				  info.defines.push_back(define.get<std::string>());
			   }
			}
		 }

		 if (!LoadShader(info)) {
			allSucceeded = false;
			continue;
		 }

		 loadedAny = true;
	  }

	  if (!loadedAny) {
		 return false;
	  }

      BuildPipelineRootParameterTables();
	  return allSucceeded;
   } catch (...) {
	  return false;
   }
}

bool ShaderManager::LoadVertexShader(const std::string& name, const std::wstring& filePath, const std::vector<std::string>& defines) {
   ShaderInfo info;
   info.name = name;
   info.filePath = filePath;
   info.type = ShaderType::Vertex;
   info.defines = defines;
   return LoadShader(info);
}

bool ShaderManager::LoadPixelShader(const std::string& name, const std::wstring& filePath, const std::vector<std::string>& defines) {
   ShaderInfo info;
   info.name = name;
   info.filePath = filePath;
   info.type = ShaderType::Pixel;
   info.defines = defines;
   return LoadShader(info);
}

bool ShaderManager::LoadComputeShader(const std::string& name, const std::wstring& filePath, const std::vector<std::string>& defines) {
   ShaderInfo info;
   info.name = name;
   info.filePath = filePath;
   info.type = ShaderType::Compute;
   info.defines = defines;
   return LoadShader(info);
}

bool ShaderManager::LoadShader(const ShaderInfo& info) {
   auto shader = CompileShader(info.filePath, GetShaderProfile(info.type), info.entryPoint, info.defines);
   if (!shader) {
	  return false;
   }

   CompiledShader compiled;
   compiled.blob = shader;
   compiled.type = info.type;
   compiled.name = info.name;
   compiled.filePath = info.filePath;
   compiled.entryPoint = info.entryPoint;
   compiled.defines = info.defines;
   compiled.reflection = ExtractReflectionInfo(shader.Get(), info.type);
   compiled.fileTimestamp = GetFileTimestamp(info.filePath);

   std::string key = CreateShaderKey(info.name, info.type);
   shaders_[key] = compiled;
   return true;
}

IDxcBlob* ShaderManager::GetVertexShader(const std::string& name) const {
   return GetShader(name, ShaderType::Vertex);
}

IDxcBlob* ShaderManager::GetPixelShader(const std::string& name) const {
   return GetShader(name, ShaderType::Pixel);
}

IDxcBlob* ShaderManager::GetComputeShader(const std::string& name) const {
   return GetShader(name, ShaderType::Compute);
}

IDxcBlob* ShaderManager::GetShader(const std::string& name, ShaderType type) const {
   std::string key = CreateShaderKey(name, type);
   auto it = shaders_.find(key);
   return (it != shaders_.end()) ? it->second.blob.Get() : nullptr;
}

const ShaderReflectionInfo* ShaderManager::GetShaderReflection(const std::string& name, ShaderType type) const {
   std::string key = CreateShaderKey(name, type);
   auto it = shaders_.find(key);
   if (it == shaders_.end()) {
	  return nullptr;
   }

   return &it->second.reflection;
}

std::optional<UINT> ShaderManager::ResolveObject3DRootParameter(const std::string& semanticName) const {
   return ResolvePipelineRootParameter("Object3D", semanticName);
}

const PipelineRootParameterTable& ShaderManager::GetObject3DRootParameterTable() const {
   static const PipelineRootParameterTable kEmptyTable{};
   const auto* table = GetPipelineRootParameterTable("Object3D");
   return table ? *table : kEmptyTable;
}

std::optional<UINT> ShaderManager::ResolvePipelineRootParameter(const std::string& pipelineName, const std::string& semanticName) const {
   ++resolveStats_.requests;
   const std::string normalizedPipeline = NormalizePipelineName(pipelineName);
   ResolveStats& pipelineStats = pipelineResolveStats_[normalizedPipeline];
   ++pipelineStats.requests;

   const auto* table = GetPipelineRootParameterTable(pipelineName);
   if (!table) {
    ++resolveStats_.misses;
    ++pipelineStats.misses;
	  return std::nullopt;
   }

   const std::string key = ToLowerString(semanticName);
   auto it = table->slotBySemanticName.find(key);
   if (it == table->slotBySemanticName.end()) {
    ++resolveStats_.misses;
    ++pipelineStats.misses;
	  return std::nullopt;
   }

   ++resolveStats_.hits;
   ++pipelineStats.hits;

   return it->second;
}

const PipelineRootParameterTable* ShaderManager::GetPipelineRootParameterTable(const std::string& pipelineName) const {
   const std::string normalized = NormalizePipelineName(pipelineName);

   auto it = pipelineRootTables_.find(normalized);
   if (it != pipelineRootTables_.end()) {
	  return &it->second;
   }

   return nullptr;
}

void ShaderManager::LogRootParameterTablesDebug() const {
   static const std::vector<std::string> kMajorPipelines = {
	  "Object3D", "Sprite", "Particle", "Line3D", "FullscreenTriangle",
	  "PostProcess_Grayscale", "PostProcess_RadialBlur", "PostProcess_GaussFilter",
	  "PostProcess_Vignette", "PostProcess_ChromaticAberration", "PostProcess_ShockWave",
	  "PostProcess_Pixelation", "PostProcess_Bloom", "PostProcess_BoxFilter", "PostProcess_Outline",
	  "PostProcess_WhiteNoise"
   };

   const auto stageInfos = GetPipelineStageMatchInfos();

   Logger::Info("[ShaderManager] Root parameter table debug dump begin");
   for (const auto& pipelineName : kMajorPipelines) {
	  const PipelineRootParameterTable* table = GetPipelineRootParameterTable(pipelineName);
	  if (!table) {
		 Logger::Warning("[ShaderManager] " + pipelineName + ": table not found");
		 continue;
	  }

	  Logger::Info("[ShaderManager] " + pipelineName +
		 ": hasReflectionData=" + std::string(table->hasReflectionData ? "true" : "false") +
		 ", entries=" + std::to_string(table->slotBySemanticName.size()));

	  auto stageIt = stageInfos.find(pipelineName);
	  if (stageIt != stageInfos.end()) {
		 const auto& info = stageIt->second;
		 Logger::Info("  [VS] reflection=" + std::string(info.vertex.hasReflection ? "present" : "missing") +
			", resources=" + std::to_string(info.vertex.resourceCount) +
			", matchedByName=" + std::to_string(info.vertex.matchedByName));
		 Logger::Info("  [PS] reflection=" + std::string(info.pixel.hasReflection ? "present" : "missing") +
			", resources=" + std::to_string(info.pixel.resourceCount) +
			", matchedByName=" + std::to_string(info.pixel.matchedByName));
	  }

	  for (const auto& [semantic, slot] : table->slotBySemanticName) {
		 Logger::Info("  - " + semantic + " -> " + std::to_string(slot));
	  }
   }
   Logger::Info("[ShaderManager] Root parameter table debug dump end");
}

std::unordered_map<std::string, PipelineStageMatchInfo> ShaderManager::GetPipelineStageMatchInfos() const {
   std::unordered_map<std::string, PipelineStageMatchInfo> results;
   for (const auto& [pipelineName, table] : pipelineRootTables_) {
	  PipelineStageMatchInfo info{};

	  const auto fillStage = [&](ShaderType stage, ShaderStageMatchInfo& out) {
		 const auto* reflection = GetShaderReflection(pipelineName, stage);
		 if (!reflection || !reflection->isValid) {
			 return;
		 }

		 out.hasReflection = true;
		 out.resourceCount = static_cast<uint32_t>(reflection->boundResources.size());
		 for (const auto& resource : reflection->boundResources) {
			 if (!resource.name.empty() && table.slotBySemanticName.contains(ToLowerString(resource.name))) {
				 ++out.matchedByName;
			 }
		 }
	  };

	  fillStage(ShaderType::Vertex, info.vertex);
	  fillStage(ShaderType::Pixel, info.pixel);
	  results[pipelineName] = info;
   }

   return results;
}

bool ShaderManager::HasShader(const std::string& name, ShaderType type) const {
   std::string key = CreateShaderKey(name, type);
   return shaders_.find(key) != shaders_.end();
}

void ShaderManager::Clear() {
   shaders_.clear();
   pipelineRootTables_.clear();
   resolveStats_ = {};
   pipelineResolveStats_.clear();
}

void ShaderManager::UnloadShader(const std::string& name, ShaderType type) {
   std::string key = CreateShaderKey(name, type);
   shaders_.erase(key);
}

bool ShaderManager::ReloadShader(const std::string& name, ShaderType type) {
   std::string key = CreateShaderKey(name, type);
   auto it = shaders_.find(key);
   if (it == shaders_.end()) {
	  return false;
   }

   // 既存のシェーダー情報を取得
   const auto& oldShader = it->second;

   // 再コンパイル
   auto newBlob = CompileShader(oldShader.filePath, GetShaderProfile(type), oldShader.entryPoint, oldShader.defines);
   if (!newBlob) {
	  return false;
   }

   // 更新
   it->second.blob = newBlob;
   it->second.reflection = ExtractReflectionInfo(newBlob.Get(), oldShader.type);
   it->second.fileTimestamp = GetFileTimestamp(oldShader.filePath);
   BuildPipelineRootParameterTables();

   return true;
}

void ShaderManager::ReloadAllShaders() {
   for (auto& [key, shader] : shaders_) {
   auto newBlob = CompileShader(shader.filePath, GetShaderProfile(shader.type), shader.entryPoint, shader.defines);
	  if (newBlob) {
		 shader.blob = newBlob;
       shader.reflection = ExtractReflectionInfo(newBlob.Get(), shader.type);
		 shader.fileTimestamp = GetFileTimestamp(shader.filePath);
	  }
   }
   BuildPipelineRootParameterTables();
}

void ShaderManager::LoadPredefinedShaders() {
   // 事前定義されたシェーダーの読み込み
   LoadVertexShader("Object3D", L"resources/shaders/Object3d.VS.hlsl");
   LoadPixelShader("Object3D", L"resources/shaders/Object3d.PS.hlsl");
   LoadVertexShader("SkinningObject3D", L"resources/shaders/SkinningObject3d.VS.hlsl");

   LoadVertexShader("Line3D", L"resources/shaders/Line3d.VS.hlsl");
   LoadPixelShader("Line3D", L"resources/shaders/Line3d.PS.hlsl");

   LoadVertexShader("Particle", L"resources/shaders/Particle.VS.hlsl");
   LoadPixelShader("Particle", L"resources/shaders/Particle.PS.hlsl");

   LoadVertexShader("FullscreenTriangle", L"resources/shaders/postprocess/FullscreenTriangle.VS.hlsl");
   LoadPixelShader("FullscreenTriangle", L"resources/shaders/postprocess/FullscreenTriangle.PS.hlsl");

   // ポストプロセス用シェーダー
   LoadPixelShader("Grayscale", L"resources/shaders/postprocess/Grayscale.PS.hlsl");
   LoadPixelShader("RadialBlur", L"resources/shaders/postprocess/RadialBlur.PS.hlsl");
   LoadPixelShader("GaussFilter", L"resources/shaders/postprocess/GaussFilter.PS.hlsl");
   LoadPixelShader("Vignette", L"resources/shaders/postprocess/Vignette.PS.hlsl");
   LoadPixelShader("ChromaticAberration", L"resources/shaders/postprocess/ChromaticAberration.PS.hlsl");
   LoadPixelShader("ShockWave", L"resources/shaders/postprocess/ShockWave.PS.hlsl");
   LoadPixelShader("Pixelation", L"resources/shaders/postprocess/Pixelation.PS.hlsl");
   LoadPixelShader("Bloom", L"resources/shaders/postprocess/Bloom.PS.hlsl");
   LoadPixelShader("BoxFilter", L"resources/shaders/postprocess/BoxFilter.PS.hlsl");
   LoadPixelShader("WhiteNoise", L"resources/shaders/postprocess/WhiteNoise.PS.hlsl");
   BuildPipelineRootParameterTables();
}

void ShaderManager::InitializeDXC() {
   HRESULT result = S_FALSE;

   result = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
   assert(SUCCEEDED(result));

   result = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
   assert(SUCCEEDED(result));

   result = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
   assert(SUCCEEDED(result));
}

ComPtr<IDxcBlob> ShaderManager::CompileShader(
   const std::wstring& filePath,
   const wchar_t* profile,
   const std::wstring& entryPoint,
   const std::vector<std::string>& defines
) {
   return ShaderCompiler::CompileShader(filePath, profile, dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get(), entryPoint, defines);
}

ShaderReflectionInfo ShaderManager::ExtractReflectionInfo(IDxcBlob* shaderBlob, ShaderType type) const {
   ShaderReflectionInfo info;
   if (!shaderBlob || !dxcUtils_) {
	  return info;
   }

   DxcBuffer reflectionBuffer{};
   reflectionBuffer.Ptr = shaderBlob->GetBufferPointer();
   reflectionBuffer.Size = shaderBlob->GetBufferSize();
   reflectionBuffer.Encoding = 0;

   DxcBuffer reflectionArg{};
   reflectionArg = reflectionBuffer;

   ComPtr<ID3D12ShaderReflection> reflection;
   HRESULT hr = dxcUtils_->CreateReflection(&reflectionArg, IID_PPV_ARGS(&reflection));
   if (FAILED(hr) || !reflection) {
	  return info;
   }

   D3D12_SHADER_DESC shaderDesc{};
   if (FAILED(reflection->GetDesc(&shaderDesc))) {
	  return info;
   }

   for (UINT i = 0; i < shaderDesc.BoundResources; ++i) {
	  D3D12_SHADER_INPUT_BIND_DESC bindDesc{};
	  if (SUCCEEDED(reflection->GetResourceBindingDesc(i, &bindDesc))) {
		 ShaderResourceBindingInfo binding{};
		 if (bindDesc.Name) {
			binding.name = bindDesc.Name;
		 }
		 binding.type = bindDesc.Type;
		 binding.bindPoint = bindDesc.BindPoint;
		 binding.bindCount = bindDesc.BindCount;
		 binding.space = bindDesc.Space;
		 info.boundResources.push_back(std::move(binding));
	  }
   }

   for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i) {
	  ID3D12ShaderReflectionConstantBuffer* cb = reflection->GetConstantBufferByIndex(i);
	  if (!cb) {
		 continue;
	  }

	  D3D12_SHADER_BUFFER_DESC cbDesc{};
	  if (SUCCEEDED(cb->GetDesc(&cbDesc))) {
		 ShaderConstantBufferInfo cbInfo{};
		 if (cbDesc.Name) {
			cbInfo.name = cbDesc.Name;
		 }
		 cbInfo.size = cbDesc.Size;
		 cbInfo.variableCount = cbDesc.Variables;
		 info.constantBuffers.push_back(std::move(cbInfo));
	  }
   }

   if (type == ShaderType::Vertex) {
	  for (UINT i = 0; i < shaderDesc.InputParameters; ++i) {
		 D3D12_SIGNATURE_PARAMETER_DESC inputDesc{};
		 if (SUCCEEDED(reflection->GetInputParameterDesc(i, &inputDesc))) {
			ShaderInputParameterInfo input{};
			if (inputDesc.SemanticName) {
			   input.semanticName = inputDesc.SemanticName;
			}
			input.semanticIndex = inputDesc.SemanticIndex;
			input.mask = inputDesc.Mask;
			input.componentType = inputDesc.ComponentType;
			info.inputParameters.push_back(std::move(input));
		 }
	  }
   }

   info.isValid = true;
   return info;
}

void ShaderManager::BuildObject3DRootParameterTable() {
   BuildPipelineRootParameterTables();
}

void ShaderManager::BuildPipelineRootParameterTables() {
   BindingLayoutResolver resolver;
   resolver.BuildPipelineRootParameterTables(
	  pipelineRootTables_,
	  [this](const std::string& shaderName, ShaderType stage) {
		 return GetShaderReflection(shaderName, stage);
	  });
}

std::string ShaderManager::CreateShaderKey(const std::string& name, ShaderType type) const {
   return name + "_" + std::to_string(static_cast<int>(type));
}

const wchar_t* ShaderManager::GetShaderProfile(ShaderType type) const {
   switch (type) {
	  case ShaderType::Vertex:   return L"vs_6_0";
	  case ShaderType::Pixel:    return L"ps_6_0";
	  case ShaderType::Compute:  return L"cs_6_0";
	  case ShaderType::Geometry: return L"gs_6_0";
	  case ShaderType::Hull:     return L"hs_6_0";
	  case ShaderType::Domain:   return L"ds_6_0";
	  default:                   return L"vs_6_0";
   }
}

uint64_t ShaderManager::GetFileTimestamp(const std::wstring& filePath) const {
   try {
	  auto ftime = std::filesystem::last_write_time(filePath);
	  return ftime.time_since_epoch().count();
   }
   catch (...) {
	  return 0;
   }
}
}
