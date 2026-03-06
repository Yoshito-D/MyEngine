#include "pch.h"
#include "PipelineState.h"

#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace {
Logger& log_ = Logger::GetInstance();
}

namespace GameEngine {
void PipelineState::CreatePipelineState(ID3D12Device* device) {
   HRESULT result = S_FALSE;

   assert(vertexShaderBlob_ != nullptr);
   assert(pixelShaderBlob_ != nullptr);

   D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
   graphicsPipelineStateDesc.pRootSignature = rootSignature_->GetRootSignature();
   if (!inputElementDescs_.empty()) {
	  graphicsPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDescs_.data();
	  graphicsPipelineStateDesc.InputLayout.NumElements = static_cast<UINT>(inputElementDescs_.size());
   } else {
	  graphicsPipelineStateDesc.InputLayout.pInputElementDescs = nullptr;
	  graphicsPipelineStateDesc.InputLayout.NumElements = 0;
   }
   graphicsPipelineStateDesc.VS = { vertexShaderBlob_->GetBufferPointer(),vertexShaderBlob_->GetBufferSize() };
   graphicsPipelineStateDesc.PS = { pixelShaderBlob_->GetBufferPointer(),pixelShaderBlob_->GetBufferSize() };
   graphicsPipelineStateDesc.BlendState = blendDesc_;
   graphicsPipelineStateDesc.RasterizerState = rasterizerDesc_;
   // 書き込むRTVの情報
   graphicsPipelineStateDesc.NumRenderTargets = 1;
   graphicsPipelineStateDesc.RTVFormats[0] = rtvFormat_;
   // 利用するトポロジ（形状の）タイプ
   graphicsPipelineStateDesc.PrimitiveTopologyType = primitiveTopologyType_;
   // どのように色を打ち込むかの設定（気にしなくても良い）
   graphicsPipelineStateDesc.SampleDesc.Count = 1;
   graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

   // DepthStencilの設定
  // depthStencilDesc_.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
   graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc_;
   if (depthStencilDesc_.DepthEnable == FALSE) {
	  graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
   } else {
	  graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
   }

   // 実際に生成
   result = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(graphicsPipelineState_.GetAddressOf()));

}

void PipelineState::SetRootSignature(RootSignature* rootSignature) {
   rootSignature_ = rootSignature;
}

void PipelineState::SetInputLayOut(const char* semanticName, UINT semanticIndex, DXGI_FORMAT format, UINT alignedByteOffset, UINT inputSlot, D3D12_INPUT_CLASSIFICATION inputSlotClass, UINT instanceDataStepRate) {
   inputElementDescs_.resize(inputElementDescs_.size() + 1);
   auto& desc = inputElementDescs_.back();

   desc.SemanticName = semanticName;
   desc.SemanticIndex = semanticIndex;
   desc.Format = format;
   desc.InputSlot = inputSlot;
   desc.AlignedByteOffset = alignedByteOffset;
   desc.InputSlotClass = inputSlotClass;
   desc.InstanceDataStepRate = instanceDataStepRate;
}

void PipelineState::SetBlendState(BlendMode blendMode) {
   blendDesc_.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
   blendDesc_.AlphaToCoverageEnable = FALSE;
   blendDesc_.IndependentBlendEnable = FALSE;

   switch (blendMode) {
	  case BlendMode::kBlendModeNone:
		 blendDesc_.RenderTarget[0].BlendEnable = FALSE;
		 blendDesc_.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		 blendDesc_.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

		 break;
	  case  BlendMode::kBlendModeNormal:
		 blendDesc_.RenderTarget[0].BlendEnable = TRUE;
		 blendDesc_.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		 blendDesc_.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		 blendDesc_.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		 blendDesc_.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		 blendDesc_.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		 blendDesc_.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		 break;
	  case  BlendMode::kBlendModeAdd:
		 blendDesc_.RenderTarget[0].BlendEnable = TRUE;
		 blendDesc_.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		 blendDesc_.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		 blendDesc_.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		 blendDesc_.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		 blendDesc_.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		 blendDesc_.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

		 break;
	  case  BlendMode::kBlendModeSubtract:
		 blendDesc_.RenderTarget[0].BlendEnable = TRUE;
		 blendDesc_.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		 blendDesc_.RenderTarget[0].BlendOp = D3D12_BLEND_OP_SUBTRACT;
		 blendDesc_.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		 blendDesc_.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		 blendDesc_.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		 blendDesc_.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

		 break;
	  case BlendMode::kBlendModeMultiply:
		 blendDesc_.RenderTarget[0].BlendEnable = TRUE;
		 blendDesc_.RenderTarget[0].SrcBlend = D3D12_BLEND_ZERO;
		 blendDesc_.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		 blendDesc_.RenderTarget[0].DestBlend = D3D12_BLEND_SRC_COLOR;
		 blendDesc_.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		 blendDesc_.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		 blendDesc_.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		 break;
	  case BlendMode::kBlendModeScreen:
		 blendDesc_.RenderTarget[0].BlendEnable = TRUE;
		 blendDesc_.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
		 blendDesc_.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		 blendDesc_.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		 blendDesc_.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		 blendDesc_.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		 blendDesc_.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		 break;
   }

}

void PipelineState::SetRasterizer(
   D3D12_CULL_MODE cullMode,
   D3D12_FILL_MODE fillMode,
   BOOL frontCounterClockwise,
   INT depthBias,
   FLOAT depthBiasClamp,
   FLOAT slopeScaledDepthBias,
   BOOL depthClipEnable,
   BOOL multisampleEnable,
   BOOL antialiasedLineEnable,
   UINT forcedSampleCount,
   D3D12_CONSERVATIVE_RASTERIZATION_MODE conservativeRaster)
{
   rasterizerDesc_.FillMode = fillMode;
   rasterizerDesc_.CullMode = cullMode;
   rasterizerDesc_.FrontCounterClockwise = frontCounterClockwise;
   rasterizerDesc_.DepthBias = depthBias;
   rasterizerDesc_.DepthBiasClamp = depthBiasClamp;
   rasterizerDesc_.SlopeScaledDepthBias = slopeScaledDepthBias;
   rasterizerDesc_.DepthClipEnable = depthClipEnable;
   rasterizerDesc_.MultisampleEnable = multisampleEnable;
   rasterizerDesc_.AntialiasedLineEnable = antialiasedLineEnable;
   rasterizerDesc_.ForcedSampleCount = forcedSampleCount;
   rasterizerDesc_.ConservativeRaster = conservativeRaster;
}

void PipelineState::SetDepthStencil(BOOL depthEnable, D3D12_DEPTH_WRITE_MASK writeMask, D3D12_COMPARISON_FUNC depthFunc) {
   depthStencilDesc_.DepthEnable = depthEnable;
   depthStencilDesc_.DepthWriteMask = writeMask;
   depthStencilDesc_.DepthFunc = depthFunc;
}

void PipelineState::SetShaders(IDxcBlob* vertexShader, IDxcBlob* pixelShader) {
   vertexShaderBlob_ = vertexShader;
   pixelShaderBlob_ = pixelShader;
}

void PipelineState::SetRTVFormat(DXGI_FORMAT format) {
   rtvFormat_ = format;
}

void PipelineState::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType) {
   primitiveTopologyType_ = topologyType;
}
}