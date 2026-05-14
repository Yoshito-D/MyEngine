#pragma once
#include <d3d12.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include <optional>
#include "Graphics/PipelineState.h"
#include "Graphics/RootSignature.h"
#include "PipelineDescriptor.h"
#include "PipelineDefinitionLoader.h"
#include "PipelineLibrary.h"
#include "BindingLayoutResolver.h"
#include "Utility/Logger.h"
#include <nlohmann/json_fwd.hpp>

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

struct PipelineReflectionMetadata {
   bool hasVertexReflection = false;
   bool hasPixelReflection = false;
   uint32_t vertexResourceCount = 0;
   uint32_t pixelResourceCount = 0;
   uint32_t estimatedRequiredBindingCount = 0;
   uint32_t rootParameterCount = 0;
   bool hasPotentialBindingMismatch = false;
   uint32_t validationWarningCount = 0;
   std::vector<std::string> missingSemantics;
};

struct ValidationSummary {
   uint32_t pipelineCount = 0;
   uint32_t warningPipelines = 0;
   uint32_t totalWarnings = 0;
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

   /// @brief コンピュートパイプライン定義を登録
   bool CreateComputePipeline(const std::string& name, const std::string& computeShaderName, const std::string& rootSignatureName);

   /// @brief パイプラインを取得（文字列とブレンドモード指定）
   /// @param name パイプライン名
   /// @param blendMode ブレンドモード
   /// @return パイプラインステート、見つからない場合はnullptr
   PipelineState* GetPipeline(const std::string& name, BlendMode blendMode = BlendMode::kBlendModeNone);

   /// @brief ルートシグネチャを取得
   /// @param name ルートシグネチャ名
   /// @return ルートシグネチャ、見つからない場合はnullptr
   RootSignature* GetRootSignature(const std::string& name);

   /// @brief コンピュートパイプライン定義を取得
   const ComputePipelineDefinition* GetComputePipeline(const std::string& name) const;

   /// @brief パイプライン向けのsemanticからルートパラメータスロットを解決
   std::optional<UINT> ResolvePipelineRootParameter(const std::string& pipelineName, const std::string& semantic) const;

   /// @brief すべてのパイプラインをクリア
   void Clear();

   /// @brief シェーダーマネージャーを取得
   ShaderManager* GetShaderManager() const { return shaderManager_; }

   /// @brief パイプライン反射メタデータを取得
   const PipelineReflectionMetadata* GetPipelineReflectionMetadata(const std::string& name) const;

   /// @brief すべてのパイプライン反射メタデータを取得
   std::unordered_map<std::string, PipelineReflectionMetadata> GetAllPipelineReflectionMetadata() const { return pipelineReflectionMetadata_; }

   /// @brief 全パイプラインの検証結果サマリーを取得（テスト足場）
   ValidationSummary GetValidationSummary() const;

   /// @brief 検証結果をJSONオブジェクトで取得
   nlohmann::json BuildValidationReportJson() const;

   /// @brief 検証結果をJSONファイルへ保存
   bool SaveValidationReportJson(const std::string& filePath) const;

private:
   GraphicsDevice* device_ = nullptr;
   ShaderManager* shaderManager_ = nullptr;

   PipelineLibrary pipelineLibrary_;

   // ルートシグネチャ格納用コンテナ
   std::unordered_map<std::string, std::unique_ptr<RootSignature>> rootSignatures_;

   // パイプライン反射メタデータ
   std::unordered_map<std::string, PipelineReflectionMetadata> pipelineReflectionMetadata_;

   // ルートシグネチャ定義のパラメータ数
   std::unordered_map<std::string, uint32_t> rootSignatureParameterCounts_;
   std::unordered_map<std::string, std::unordered_map<std::string, UINT>> rootSignatureSemanticSlots_;
   std::unordered_map<std::string, std::unordered_map<std::string, UINT>> pipelineSemanticSlots_;

   // 同一警告の重複抑制
   std::unordered_set<std::string> emittedValidationWarnings_;

   PipelineDefinitionLoader definitionLoader_;
   BindingLayoutResolver bindingLayoutResolver_;

   /// @brief パイプラインキーを生成
   /// @param name パイプライン名
   /// @param blendMode ブレンドモード
   /// @return キー文字列
   std::string CreatePipelineKey(const std::string& name, BlendMode blendMode) const;

   /// @brief ルートシグネチャをJSONファイルから読み込み
   /// @param filePath ファイルパス
   /// @return 成功時はtrue
   bool LoadRootSignatureFromFile(const std::string& filePath);

   /// @brief パイプラインをJSONファイルから読み込み
   /// @param filePath ファイルパス
   /// @param rtvFormat レンダーターゲットフォーマット
   /// @return 成功時はtrue
   bool LoadPipelineFromFile(const std::string& filePath, DXGI_FORMAT rtvFormat);

   /// @brief 重複抑制付きログ出力
   void LogValidationMessage(const std::string& dedupeKey, const std::string& message, Logger::LogLevel level);

   /// @brief 頂点シェーダー反射情報から入力レイアウトを構築
   std::vector<InputElementDefinition> BuildInputLayoutFromVertexShaderReflection(const std::string& vertexShaderName) const;

   /// @brief 反射パラメータをDXGI_FORMATへ変換
   DXGI_FORMAT ConvertReflectionInputToFormat(BYTE mask, D3D_REGISTER_COMPONENT_TYPE componentType) const;

   std::optional<UINT> ResolveShaderRegisterBySemantic(
      const std::string& semantic,
      D3D12_ROOT_PARAMETER_TYPE parameterType,
      D3D12_SHADER_VISIBILITY visibility,
      D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
      const std::string& vertexShader,
      const std::string& pixelShader,
      const std::string& computeShader) const;

   void RegisterPipelineSemanticSlots(const std::string& pipelineName, const std::string& rootSignatureName);
};
}