#include "pch.h"
#include "ShaderManager.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/ShaderCompiler.h"
#include "Utility/JsonDataManager.h"
#include <filesystem>

namespace GameEngine {
void ShaderManager::Initialize(GraphicsDevice* device) {
   device_ = device;
   InitializeDXC();
   LoadPredefinedShaders(); // 自動的に事前定義シェーダーを読み込む
}

bool ShaderManager::LoadShaderRegistry(const std::wstring& /*registryFilePath*/) {
   // ここではシンプルに事前定義シェーダーを読み込む
   // 実際にはJsonDataManagerを使用してファイルから読み込む実装を追加
   LoadPredefinedShaders();

   // TODO: JSONファイルからシェーダー定義を読み込む実装
   // JsonDataManagerを使用して動的に読み込む
   // std::string registryFilePathA = ...conversion...

   return true;
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
   auto shader = CompileShader(info.filePath, GetShaderProfile(info.type), info.defines);
   if (!shader) {
	  return false;
   }

   CompiledShader compiled;
   compiled.blob = shader;
   compiled.type = info.type;
   compiled.name = info.name;
   compiled.filePath = info.filePath;
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

bool ShaderManager::HasShader(const std::string& name, ShaderType type) const {
   std::string key = CreateShaderKey(name, type);
   return shaders_.find(key) != shaders_.end();
}

void ShaderManager::Clear() {
   shaders_.clear();
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
   auto newBlob = CompileShader(oldShader.filePath, GetShaderProfile(type));
   if (!newBlob) {
	  return false;
   }

   // 更新
   it->second.blob = newBlob;
   it->second.fileTimestamp = GetFileTimestamp(oldShader.filePath);

   return true;
}

void ShaderManager::ReloadAllShaders() {
   for (auto& [key, shader] : shaders_) {
	  auto newBlob = CompileShader(shader.filePath, GetShaderProfile(shader.type));
	  if (newBlob) {
		 shader.blob = newBlob;
		 shader.fileTimestamp = GetFileTimestamp(shader.filePath);
	  }
   }
}

void ShaderManager::LoadPredefinedShaders() {
   // 事前定義されたシェーダーの読み込み
   LoadVertexShader("Object3D", L"resources/shaders/Object3d.VS.hlsl");
   LoadPixelShader("Object3D", L"resources/shaders/Object3d.PS.hlsl");

   LoadVertexShader("Line3D", L"resources/shaders/Line3d.VS.hlsl");
   LoadPixelShader("Line3D", L"resources/shaders/Line3d.PS.hlsl");

   LoadVertexShader("Particle", L"resources/shaders/Particle.VS.hlsl");
   LoadPixelShader("Particle", L"resources/shaders/Particle.PS.hlsl");

   LoadVertexShader("FullscreenTriangle", L"resources/shaders/postprocess/FullscreenTriangle.VS.hlsl");
   LoadPixelShader("FullscreenTriangle", L"resources/shaders/postprocess/FullscreenTriangle.PS.hlsl");

   // ポストプロセス用シェーダー
   LoadPixelShader("Grayscale", L"resources/shaders/postprocess/Grayscale.PS.hlsl");
   LoadPixelShader("RadialBlur", L"resources/shaders/postprocess/RadialBlur.PS.hlsl");
   LoadPixelShader("GaussBlur", L"resources/shaders/postprocess/GaussBlur.PS.hlsl");
   LoadPixelShader("Vignette", L"resources/shaders/postprocess/Vignette.PS.hlsl");
   LoadPixelShader("ChromaticAberration", L"resources/shaders/postprocess/ChromaticAberration.PS.hlsl");
   LoadPixelShader("ShockWave", L"resources/shaders/postprocess/ShockWave.PS.hlsl");
   LoadPixelShader("Pixelation", L"resources/shaders/postprocess/Pixelation.PS.hlsl");
   LoadPixelShader("Bloom", L"resources/shaders/postprocess/Bloom.PS.hlsl");
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
   const std::vector<std::string>& /*defines*/
) {
   // TODO: definesを使用してマクロ定義をコンパイル時に渡す実装
   return ShaderCompiler::CompileShader(filePath, profile, dxcUtils_.Get(), dxcCompiler_.Get(), includeHandler_.Get());
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