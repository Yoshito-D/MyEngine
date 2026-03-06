#pragma once
#include <d3d12.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include "Graphics/PipelineState.h"
#include "Graphics/RootSignature.h"
#include "PipelineDescriptor.h"

namespace GameEngine {
class GraphicsDevice;
class ShaderManager;
class OffscreenRenderTarget;

/// @brief パイプライン設定構造体
struct PipelineConfig {
   std::string vertexShaderName;
   std::string pixelShaderName;
   std::string rootSignatureName;
   BlendMode blendMode = BlendMode::kBlendModeNone;
   D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_BACK;
   D3D12_FILL_MODE fillMode = D3D12_FILL_MODE_SOLID;
   BOOL depthEnable = TRUE;
   D3D12_DEPTH_WRITE_MASK depthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
   D3D12_COMPARISON_FUNC depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
   D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
   DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

   std::vector<InputElementDefinition> inputElements;
};

/// @brief パイプライン管理クラス
class PSOManager {
public:
   /// @brief パイプラインマネージャーの初期化
   /// @param device グラフィックスデバイス
   /// @param shaderManager シェーダーマネージャー
   void Initialize(GraphicsDevice* device, ShaderManager* shaderManager);

   /// @brief パイプライン定義ファイルから読み込み
   /// @param definitionFilePath 定義ファイルのパス
   /// @param rtvFormat レンダーターゲットフォーマット
   /// @return 成功時はtrue
   bool LoadPipelineDefinitions(const std::wstring& definitionFilePath, DXGI_FORMAT rtvFormat);

   /// @brief 事前定義されたパイプラインを作成（後方互換性用）
   /// @param offscreenRenderTarget オフスクリーンレンダーターゲット
   void CreatePredefinedPipelines(OffscreenRenderTarget* offscreenRenderTarget);

   /// @brief ルートシグネチャ定義から作成
   /// @param definition ルートシグネチャ定義
   /// @return 作成成功時はtrue
   bool CreateRootSignatureFromDefinition(const RootSignatureDefinition& definition);

   /// @brief パイプライン定義からパイプラインを作成
   /// @param definition パイプライン定義
   /// @param rtvFormat レンダーターゲットフォーマット
   /// @return 作成成功時はtrue
   bool CreatePipelineFromDefinition(const PipelineDefinition& definition, DXGI_FORMAT rtvFormat);

   /// @brief カスタムパイプラインを作成
   /// @param name パイプライン名
   /// @param config パイプライン設定
   /// @return 作成成功時はtrue
   bool CreateCustomPipeline(const std::string& name, const PipelineConfig& config);

   /// @brief パイプラインを取得（文字列とブレンドモード指定）
   /// @param name パイプライン名
   /// @param blendMode ブレンドモード
   /// @return パイプラインステート、見つからない場合はnullptr
   PipelineState* GetPipeline(const std::string& name, BlendMode blendMode = BlendMode::kBlendModeNone);

   /// @brief ルートシグネチャを取得
   /// @param name ルートシグネチャ名
   /// @return ルートシグネチャ、見つからない場合はnullptr
   RootSignature* GetRootSignature(const std::string& name);

   /// @brief すべてのパイプラインをクリア
   void Clear();

private:
   GraphicsDevice* device_ = nullptr;
   ShaderManager* shaderManager_ = nullptr;

   // パイプライン格納用コンテナ（名前 + ブレンドモードをキーとする）
   std::unordered_map<std::string, std::unique_ptr<PipelineState>> pipelines_;

   // ルートシグネチャ格納用コンテナ
   std::unordered_map<std::string, std::unique_ptr<RootSignature>> rootSignatures_;

   /// @brief パイプラインキーを生成
   /// @param name パイプライン名
   /// @param blendMode ブレンドモード
   /// @return キー文字列
   std::string CreatePipelineKey(const std::string& name, BlendMode blendMode);

   /// @brief ルートシグネチャをJSONファイルから読み込み
   /// @param filePath ファイルパス
   /// @return 成功時はtrue
   bool LoadRootSignatureFromFile(const std::string& filePath);

   /// @brief パイプラインをJSONファイルから読み込み
   /// @param filePath ファイルパス
   /// @param rtvFormat レンダーターゲットフォーマット
   /// @return 成功時はtrue
   bool LoadPipelineFromFile(const std::string& filePath, DXGI_FORMAT rtvFormat);
};
}