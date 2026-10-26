#pragma once

#include <d3d12.h>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include <optional>
#include "GameEngine/Graphics/Pipeline/PipelineState.h"
#include "GameEngine/Graphics/Pipeline/RootSignature.h"
#include "GameEngine/Graphics/Pipeline/PipelineDescriptor.h"
#include "GameEngine/Graphics/Pipeline/PipelineDefinitionLoader.h"
#include "GameEngine/Graphics/Pipeline/PipelineLibrary.h"
#include "GameEngine/Utility/Logger.h"
#include <nlohmann/json_fwd.hpp>

namespace GameEngine {
class GraphicsDevice;
class ShaderManager;

/// @brief パイプライン設定構造体
struct PipelineConfig {
   std::string vertexShaderName;                     ///< 参照する頂点シェーダーの登録名。
   std::string pixelShaderName;                      ///< 参照するピクセルシェーダーの登録名。
   std::string rootSignatureName;                    ///< 使用するルートシグネチャ名。
   BlendMode blendMode = BlendMode::kBlendModeNone;  ///< 優先ブレンドモード（nullopt = 加算）。
   D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_BACK;  ///< 描画から除外する面の向き。
   D3D12_FILL_MODE fillMode = D3D12_FILL_MODE_SOLID; ///< ポリゴンを塗りつぶすかワイヤー表示するかの設定。
   BOOL frontCounterClockwise = FALSE;               ///< TRUEの場合は反時計回りの面を前面として扱う
   BOOL depthEnable = TRUE;                          ///< 深度テストを有効にするかどうか。
   D3D12_DEPTH_WRITE_MASK depthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; ///< 深度バッファへ書き込むかどうかの設定。
   D3D12_COMPARISON_FUNC depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; ///< 深度テストで使う比較方法。
   /// @brief 頂点を点・線・三角形のどれとして組み立てるかの分類。
   D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
   DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM; ///< このPSOが出力するレンダーターゲットの形式。

   std::vector<InputElementDefinition> inputElements; ///< 頂点入力を構成する各要素の定義。
};

struct PipelineReflectionMetadata {
   bool hasVertexReflection = false;           ///< 頂点シェーダーのリフレクション情報を取得できたかどうか。
   bool hasPixelReflection = false;            ///< ピクセルシェーダーのリフレクション情報を取得できたかどうか。
   uint32_t vertexResourceCount = 0;           ///< 頂点シェーダーが参照するリソースの数。
   uint32_t pixelResourceCount = 0;            ///< ピクセルシェーダーが参照するリソースの数。
   uint32_t estimatedRequiredBindingCount = 0; ///< リフレクションから推定した必要バインド数。
   uint32_t rootParameterCount = 0;            ///< ルートシグネチャに定義されたパラメーター数。
   bool hasPotentialBindingMismatch = false;   ///< シェーダーとルート定義に不一致の可能性があるかどうか。
   uint32_t validationWarningCount = 0;        ///< このパイプラインの検証で検出した警告数。
   std::vector<std::string> missingSemantics;  ///< ルート定義に対応先がない必須セマンティクス。
};

struct ValidationSummary {
   uint32_t pipelineCount = 0;    ///< 検証対象のパイプライン数。
   uint32_t warningPipelines = 0; ///< 警告が検出されたパイプライン数。
   uint32_t totalWarnings = 0;    ///< 全パイプラインの警告数の合計。
};

/// @brief パイプライン管理クラス
class PSOManager {
public:
   /// @brief パイプラインマネージャーの初期化
   /// @param device グラフィックスデバイス
   /// @param shaderManager シェーダーマネージャー
   void Initialize(GraphicsDevice* device, ShaderManager* shaderManager);

   /// @brief ルートシグネチャとPSOのJSONをディレクトリから自動検出して読み込む。
   /// @param rtvFormat レンダーターゲットフォーマット。
   /// @param definitionDirectory root_signaturesとpsoを含むディレクトリ。
   /// @return すべての定義の検証とGPUオブジェクトの生成に成功した場合はtrue。
   bool LoadPipelineDefinitions(
      DXGI_FORMAT rtvFormat, const std::filesystem::path& definitionDirectory = "Resources/engine/pipelines");

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
   bool CreateComputePipeline(
      const std::string& name, const std::string& computeShaderName, const std::string& rootSignatureName);

   /// @brief パイプラインを取得（文字列とブレンドモード指定）
   /// @param name パイプライン名
   /// @param blendMode ブレンドモード
   /// @return パイプラインステート、見つからない場合はnullptr
   const PipelineState* GetPipeline(const std::string& name, BlendMode blendMode = BlendMode::kBlendModeNone) const;

   /// @brief 表裏反転用パイプラインの論理名を生成する
   /// @param name 元のパイプライン名
   /// @return 表裏反転用パイプライン名
   static std::string MakeReversedFacePipelineName(const std::string& name);

   /// @brief ルートシグネチャを取得
   /// @param name ルートシグネチャ名
   /// @return ルートシグネチャ、見つからない場合はnullptr
   const RootSignature* GetRootSignature(const std::string& name) const;

   /// @brief コンピュートパイプライン定義を取得
   const ComputePipelineDefinition* GetComputePipeline(const std::string& name) const;

   /// @brief パイプライン向けのsemanticからルートパラメータスロットを解決
   std::optional<UINT> ResolvePipelineRootParameter(const std::string& pipelineName, const std::string& semantic) const;

   /// @brief 検証済みのモデル契約を返す。利用不可または非互換のパイプラインならnullptr。
   const ModelPipelineDefinition* GetModelPipeline(const std::string& name) const;

   /// @brief 作成に成功したモデル互換パイプラインをエディター用に列挙する。
   std::vector<std::string> GetModelPipelineNames() const;

   /// @brief PSOをコンパイルせず、利用可能なブレンド状態を解決する。
   BlendMode ResolveModelBlendMode(const std::string& name, BlendMode requested) const;

   /// @brief すべてのパイプラインをクリア
   void Clear();

   /// @brief シェーダーマネージャーを取得
   const ShaderManager* GetShaderManager() const { return shaderManager_; }

   /// @brief パイプライン反射メタデータを取得
   const PipelineReflectionMetadata* GetPipelineReflectionMetadata(const std::string& name) const;

   /// @brief すべてのパイプライン反射メタデータを取得
   std::unordered_map<std::string, PipelineReflectionMetadata> GetAllPipelineReflectionMetadata() const;

   /// @brief 全パイプラインの検証結果サマリーを取得（テスト足場）
   ValidationSummary GetValidationSummary() const;

   /// @brief 検証結果をJSONオブジェクトで取得
   nlohmann::json BuildValidationReportJson() const;

   /// @brief 検証結果をJSONファイルへ保存
   bool SaveValidationReportJson(const std::string& filePath) const;

private:
   GraphicsDevice* device_ = nullptr;       ///< GPUリソースや描画コマンドの生成に使うデバイスへの参照。
   ShaderManager* shaderManager_ = nullptr; ///< シェーダーマネージャー。

   PipelineLibrary pipelineLibrary_; ///< 生成した描画・Computeパイプラインを所有する保管先。
   /// @brief ルートシグネチャ名ごとの読み込み済み定義。
   std::unordered_map<std::string, RootSignatureDefinition> rootDefinitions_;
   /// @brief モデルパイプライン名ごとのバインド・マテリアル契約。
   std::unordered_map<std::string, ModelPipelineDefinition> modelPipelines_;
   /// @brief ブレンド方式を固定したモデルパイプラインの対応表。
   std::unordered_map<std::string, BlendMode> fixedModelBlendModes_;
   bool ValidateModelPipeline(const PipelineDefinition& definition, ModelPipelineDefinition& model) const;

   // ルートシグネチャ格納用コンテナ
   /// @brief 名前ごとに登録するルートシグネチャの定義または所有実体。
   std::unordered_map<std::string, std::unique_ptr<RootSignature>> rootSignatures_;

   // パイプライン反射メタデータ
   /// @brief パイプライン名ごとのリソース検証結果。
   std::unordered_map<std::string, PipelineReflectionMetadata> pipelineReflectionMetadata_;

   // ルートシグネチャ定義のパラメータ数
   /// @brief ルートシグネチャ名ごとのパラメーター数。
   std::unordered_map<std::string, uint32_t> rootSignatureParameterCounts_;
   /// @brief ルートシグネチャごとの意味名からスロット番号への索引。
   std::unordered_map<std::string, std::unordered_map<std::string, UINT>> rootSignatureSemanticSlots_;
   /// @brief パイプラインごとの意味名からルートスロット番号への索引。
   std::unordered_map<std::string, std::unordered_map<std::string, UINT>> pipelineSemanticSlots_;

   // 同一警告の重複抑制
   std::unordered_set<std::string> emittedValidationWarnings_; ///< 同じ検証警告の重複出力を抑える識別キー集合。

   PipelineDefinitionLoader definitionLoader_; ///< 定義JSONを検出し、識別名を検証するローダー。

   /// @brief パイプラインキーを生成
   /// @param name パイプライン名
   /// @param blendMode ブレンドモード
   /// @return キー文字列
   std::string CreatePipelineKey(const std::string& name, BlendMode blendMode) const;

   /// @brief ルートシグネチャをJSONファイルから読み込み
   /// @param definitionPath 定義ファイルのパス
   /// @return 成功時はtrue
   bool LoadRootSignatureFromFile(const std::filesystem::path& definitionPath);

   /// @brief パイプラインをJSONファイルから読み込み
   /// @param definitionPath 定義ファイルのパス
   /// @param rtvFormat レンダーターゲットフォーマット
   /// @return 成功時はtrue
   bool LoadPipelineFromFile(const std::filesystem::path& definitionPath, DXGI_FORMAT rtvFormat);

   /// @brief 重複抑制付きログ出力
   void LogValidationMessage(const std::string& dedupeKey, const std::string& message, Logger::LogLevel level);

   /// @brief 頂点シェーダー反射情報から入力レイアウトを構築
   std::vector<InputElementDefinition> BuildInputLayoutFromVertexShaderReflection(
      const std::string& vertexShaderName) const;

   /// @brief 反射パラメータをDXGI_FORMATへ変換
   DXGI_FORMAT ConvertReflectionInputToFormat(BYTE mask, D3D_REGISTER_COMPONENT_TYPE componentType) const;

   std::optional<UINT> ResolveShaderRegisterBySemantic(const std::string& semantic,
      D3D12_ROOT_PARAMETER_TYPE parameterType, D3D12_SHADER_VISIBILITY visibility,
      D3D12_DESCRIPTOR_RANGE_TYPE rangeType, const std::string& vertexShader, const std::string& pixelShader,
      const std::string& computeShader) const;

   void RegisterPipelineSemanticSlots(const std::string& pipelineName, const std::string& rootSignatureName);
};
}
