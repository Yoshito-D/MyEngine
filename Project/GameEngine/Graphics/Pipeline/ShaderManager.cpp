#include "GameEngine/pch.h"
#include <cstdint>
#include "GameEngine/Graphics/Pipeline/ShaderManager.h"
#include "GameEngine/Utility/Logger.h"
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <functional>
#include <regex>
#include <unordered_set>

namespace GameEngine {
bool ShaderManager::Initialize(const std::wstring& shaderDirectory) {
   if (!dxcUtils_ && FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_)))) {
      Logger::Error("[ShaderManager] Failed to initialize DXC utilities.");
      return false;
   }

   if (!LoadShaderDirectory(shaderDirectory)) {
      return false;
   }

   shaderDirectory_ = shaderDirectory;
   return true;
}

bool ShaderManager::LoadShaderDirectory(const std::wstring& shaderDirectory) {
   try {
      std::vector<std::filesystem::path> paths;

      for (const auto& entry : std::filesystem::recursive_directory_iterator(shaderDirectory)) {
         if (entry.is_regular_file() && entry.path().extension() == L".cso") {
            paths.push_back(entry.path());
         }
      }

      if (paths.empty()) {
         Logger::Error(L"[ShaderManager] No compiled shaders found in: " + shaderDirectory);
         return false;
      }

      std::sort(paths.begin(), paths.end());
      std::unordered_map<std::string, CompiledShader> loaded;
      std::unordered_set<std::string> identifiers;

      for (const auto& path : paths) {
         CompiledShader shader;

         if (!ReadShaderBinary(path.wstring(), shader)) {
            return false;
         }

         const std::string key = CreateShaderKey(shader.name, shader.type);
         std::string insensitiveKey = key;
         std::transform(insensitiveKey.begin(), insensitiveKey.end(), insensitiveKey.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

         // Windows上のビルドと同じく、ディレクトリや大文字小文字だけが違う同名を拒否する。
         if (!identifiers.insert(insensitiveKey).second) {
            Logger::Error(L"[ShaderManager] Duplicate shader name and stage: " + path.wstring());
            return false;
         }

         loaded.emplace(key, std::move(shader));
      }

      // 途中で不正バイナリが見つかっても、稼働中のシェーダーを部分的に置き換えない。
      shaders_.swap(loaded);
      Logger::Info("[ShaderManager] Loaded compiled shaders: " + std::to_string(shaders_.size()));
      return true;
   } catch (const std::exception& error) {
      Logger::Error(L"[ShaderManager] Cannot load shader directory: " + shaderDirectory);
      Logger::Error(error.what());
      return false;
   }
}

bool ShaderManager::ReadShaderBinary(const std::wstring& filePath, CompiledShader& shader) const {
   const std::filesystem::path path(filePath);
   const std::string fileName = path.filename().string();
   static const std::regex pattern(R"(^([A-Za-z_][A-Za-z0-9_]*)\.(VS|PS|CS|GS|HS|DS)\.cso$)");
   std::smatch match;

   if (!dxcUtils_ || !std::regex_match(fileName, match, pattern)) {
      Logger::Error(L"[ShaderManager] Expected <Name>.<VS|PS|CS|GS|HS|DS>.cso: " + filePath);
      return false;
   }

   const std::string stage = match[2].str();
   ShaderType type = ShaderType::Vertex;

   if (stage == "PS") {
      type = ShaderType::Pixel;
   } else if (stage == "CS") {
      type = ShaderType::Compute;
   } else if (stage == "GS") {
      type = ShaderType::Geometry;
   } else if (stage == "HS") {
      type = ShaderType::Hull;
   } else if (stage == "DS") {
      type = ShaderType::Domain;
   }

   ComPtr<IDxcBlobEncoding> blob;

   if (FAILED(dxcUtils_->LoadFile(filePath.c_str(), nullptr, &blob)) || !blob || blob->GetBufferSize() == 0) {
      Logger::Error(L"[ShaderManager] Cannot read compiled shader: " + filePath);
      return false;
   }

   auto reflection = ExtractReflectionInfo(blob.Get(), type);

   if (!reflection.isValid) {
      Logger::Error(L"[ShaderManager] Invalid shader binary, reflection, or stage: " + filePath);
      return false;
   }

   shader.blob = blob;
   shader.type = type;
   shader.name = match[1].str();
   shader.filePath = filePath;
   shader.reflection = std::move(reflection);
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

IDxcBlob* ShaderManager::GetGeometryShader(const std::string& name) const {
   return GetShader(name, ShaderType::Geometry);
}

IDxcBlob* ShaderManager::GetHullShader(const std::string& name) const {
   return GetShader(name, ShaderType::Hull);
}

IDxcBlob* ShaderManager::GetDomainShader(const std::string& name) const {
   return GetShader(name, ShaderType::Domain);
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
   auto it = shaders_.find(CreateShaderKey(name, type));

   if (it == shaders_.end()) {
      return false;
   }

   CompiledShader shader;

   // 読み込みと反射情報の検証が完了するまで既存Blobを保持する。
   if (!ReadShaderBinary(it->second.filePath, shader)) {
      return false;
   }

   it->second = std::move(shader);
   return true;
}

bool ShaderManager::ReloadAllShaders() {
   return LoadShaderDirectory(shaderDirectory_);
}

ShaderReflectionInfo ShaderManager::ExtractReflectionInfo(IDxcBlob* shaderBlob, ShaderType type) const {
   ShaderReflectionInfo info;

   if (!shaderBlob || !dxcUtils_) {
      return info;
   }

   // DXCコンテナ内の反射チャンクをID3D12ShaderReflectionとして開き、
   // PSO入力レイアウトとルート定義検証に必要な最小情報だけをコピーする。
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

   // 改名された別ステージのバイナリをPSOへ渡さないよう、ファイル名と実体も照合する。
   constexpr D3D12_SHADER_VERSION_TYPE stageTypes[] = {D3D12_SHVER_VERTEX_SHADER, D3D12_SHVER_PIXEL_SHADER,
      D3D12_SHVER_COMPUTE_SHADER, D3D12_SHVER_GEOMETRY_SHADER, D3D12_SHVER_HULL_SHADER, D3D12_SHVER_DOMAIN_SHADER};

   if (D3D12_SHVER_GET_TYPE(shaderDesc.Version) != static_cast<UINT>(stageTypes[static_cast<size_t>(type)])) {
      return info;
   }

   // CBV/SRV/UAV/Samplerを含むバインド資源は、semanticからレジスタを推定する際に使う。
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
         binding.dimension = bindDesc.Dimension;
         info.boundResources.push_back(std::move(binding));
      }
   }

   // 定数バッファはサイズと変数数も保存し、将来のレイアウト診断で利用できるようにする。
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

         // ConstantBuffer<Struct>と通常のcbufferを同じABI記述へ平坦化する。
         std::function<void(ID3D12ShaderReflectionType*, const std::string&, UINT)> visit;
         visit = [&](ID3D12ShaderReflectionType* field, const std::string& name, UINT offset) {
            D3D12_SHADER_TYPE_DESC desc{};

            if (!field || FAILED(field->GetDesc(&desc))) return;

            if (desc.Class == D3D_SVC_STRUCT && desc.Elements == 0) {
               for (UINT member = 0; member < desc.Members; ++member) {
                  auto* child = field->GetMemberTypeByIndex(member);
                  D3D12_SHADER_TYPE_DESC childDesc{};

                  if (child && SUCCEEDED(child->GetDesc(&childDesc))) {
                     visit(child, field->GetMemberTypeName(member), offset + childDesc.Offset);
                  }
               }
            } else {
               cbInfo.variables.push_back(
                  {name, offset, desc.Type, desc.Class, desc.Rows, desc.Columns, desc.Elements});
            }
         };

         for (UINT variable = 0; variable < cbDesc.Variables; ++variable) {
            auto* field = cb->GetVariableByIndex(variable);
            D3D12_SHADER_VARIABLE_DESC desc{};

            if (field && SUCCEEDED(field->GetDesc(&desc))) {
               visit(field->GetType(), desc.Name, desc.StartOffset);
            }
         }

         info.constantBuffers.push_back(std::move(cbInfo));
      }
   }

   if (type == ShaderType::Vertex) {
      // Input Assemblerのレイアウトを自動生成できるのは頂点シェーダー入力だけなので、
      // 他ステージではシグネチャ走査を省く。
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

std::string ShaderManager::CreateShaderKey(const std::string& name, ShaderType type) const {
   return name + "_" + std::to_string(static_cast<std::int32_t>(type));
}

}
