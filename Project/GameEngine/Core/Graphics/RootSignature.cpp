#include "pch.h"
#include "RootSignature.h"
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace GameEngine {
void RootSignature::CreateRootSignature(ID3D12Device* device) {
   HRESULT result = S_FALSE;

   ComPtr<ID3DBlob> errorBlob;

   D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
   descriptionRootSignature.Flags = rootSignatureFlags_;

   descriptionRootSignature.pParameters = rootParameters_.data();
   descriptionRootSignature.NumParameters = static_cast<UINT>(rootParameters_.size());

   descriptionRootSignature.pStaticSamplers = staticSamplers_.data();
   descriptionRootSignature.NumStaticSamplers = static_cast<UINT>(staticSamplers_.size());

   // CPU側で組み立てたパラメータ／静的サンプラー配列を一度バイナリ化し、
   // D3D12が検証済みの不変レイアウトとして生成する。
   result = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &rootSignatureBlob_, &errorBlob);
   if (FAILED(result)) {
      Logger::Error(errorBlob ? std::string(static_cast<const char*>(errorBlob->GetBufferPointer())) :
         "[RootSignature] Serialization failed");
      return;
   }

   result = device->CreateRootSignature(0, rootSignatureBlob_->GetBufferPointer(), rootSignatureBlob_->GetBufferSize(), IID_PPV_ARGS(rootSignature_.GetAddressOf()));
   if (FAILED(result)) Logger::Error("[RootSignature] Creation failed: " + std::to_string(result));
}

void RootSignature::SetRootParameter(D3D12_ROOT_PARAMETER_TYPE type, D3D12_SHADER_VISIBILITY visibility, UINT shaderRegister, const D3D12_DESCRIPTOR_RANGE* descriptorRanges, UINT numDescriptorRanges, UINT num32BitValues) {
   rootParameters_.resize(rootParameters_.size() + 1);
   auto& rootParameter = rootParameters_.back();
   rootParameter.ParameterType = type;
   rootParameter.ShaderVisibility = visibility;
   // Root Parameter種別ごとに有効なunionメンバーだけを設定し、JSON配列の追加順をスロット番号として保つ。
   if (type == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
	  rootParameter.DescriptorTable.pDescriptorRanges = descriptorRanges;
	  rootParameter.DescriptorTable.NumDescriptorRanges = numDescriptorRanges;
   } else if (type == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS) {
	  rootParameter.Constants.ShaderRegister = shaderRegister;
	  rootParameter.Constants.RegisterSpace = 0;
	  rootParameter.Constants.Num32BitValues = num32BitValues;
   } else {
	  rootParameter.Descriptor.ShaderRegister = shaderRegister;
	  rootParameter.Descriptor.RegisterSpace = 0;
   }
}

void RootSignature::SetSampler(D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE addressU, D3D12_TEXTURE_ADDRESS_MODE addressV, D3D12_TEXTURE_ADDRESS_MODE addressW, D3D12_COMPARISON_FUNC comparisonFunc, FLOAT maxLOD, UINT shaderRegister, D3D12_SHADER_VISIBILITY shaderVisibility) {
   staticSamplers_.resize(staticSamplers_.size() + 1);
   auto& staticSampler = staticSamplers_.back();
   staticSampler.Filter = filter;
   staticSampler.AddressU = addressU;
   staticSampler.AddressV = addressV;
   staticSampler.AddressW = addressW;
   staticSampler.ComparisonFunc = comparisonFunc;
   staticSampler.MaxLOD = maxLOD;
   staticSampler.ShaderRegister = shaderRegister;
   staticSampler.ShaderVisibility = shaderVisibility;
}

const D3D12_DESCRIPTOR_RANGE* RootSignature::CreateDescriptorRange(
   D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
   UINT numDescriptors,
   UINT baseShaderRegister,
   UINT registerSpace) {
   
   D3D12_DESCRIPTOR_RANGE range{};
   range.RangeType = rangeType;
   range.NumDescriptors = numDescriptors;
   range.BaseShaderRegister = baseShaderRegister;
   range.RegisterSpace = registerSpace;
   range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
   
   // Root parameters retain pointers until serialization; deque keeps them stable.
   descriptorRanges_.push_back(range);
   return &descriptorRanges_.back();
}

void RootSignature::SetFlags(D3D12_ROOT_SIGNATURE_FLAGS flags) {
   rootSignatureFlags_ = flags;
}
}
