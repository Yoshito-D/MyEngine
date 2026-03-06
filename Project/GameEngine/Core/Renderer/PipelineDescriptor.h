#pragma once
#include <string>
#include <vector>
#include <d3d12.h>
#include "Graphics/PipelineState.h" // BlendModeの定義を含む

namespace GameEngine {

/// @brief ルートパラメータ定義
struct RootParameterDefinition {
   D3D12_ROOT_PARAMETER_TYPE type = D3D12_ROOT_PARAMETER_TYPE_CBV;
   D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL;
   UINT shaderRegister = 0;
   UINT registerSpace = 0;
   UINT descriptorCount = 1; // DESCRIPTOR_TABLE用
   D3D12_DESCRIPTOR_RANGE_TYPE rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // ディスクリプタテーブル用
};

/// @brief サンプラー定義
struct SamplerDefinition {
   D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
   D3D12_TEXTURE_ADDRESS_MODE addressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
   D3D12_TEXTURE_ADDRESS_MODE addressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
   D3D12_TEXTURE_ADDRESS_MODE addressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
   D3D12_COMPARISON_FUNC comparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
   FLOAT maxLOD = D3D12_FLOAT32_MAX;
   UINT shaderRegister = 0;
   D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_PIXEL;
};

/// @brief ルートシグネチャ定義
struct RootSignatureDefinition {
   std::string name;
   std::vector<RootParameterDefinition> parameters;
   std::vector<SamplerDefinition> samplers;
};

/// @brief 入力レイアウト要素定義
struct InputElementDefinition {
   std::string semanticName;
   UINT semanticIndex;
   DXGI_FORMAT format;
   UINT alignedByteOffset;
   UINT inputSlot;
   D3D12_INPUT_CLASSIFICATION inputSlotClass;
   UINT instanceDataStepRate;
};

/// @brief パイプライン定義構造体
struct PipelineDefinition {
   std::string name;
   std::string vertexShader;
   std::string pixelShader;
   std::string rootSignature;
   std::vector<InputElementDefinition> inputLayout;
   bool supportBlendModes = false;
   BlendMode defaultBlendMode = BlendMode::kBlendModeNone;
   D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_BACK;
   D3D12_FILL_MODE fillMode = D3D12_FILL_MODE_SOLID;
   BOOL depthEnable = TRUE;
   D3D12_DEPTH_WRITE_MASK depthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
   D3D12_COMPARISON_FUNC depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
   D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
};

/// @brief パイプラインレジストリ定義
struct PipelineRegistryDefinition {
   std::vector<RootSignatureDefinition> rootSignatures;
   std::vector<PipelineDefinition> pipelines;
};
}
