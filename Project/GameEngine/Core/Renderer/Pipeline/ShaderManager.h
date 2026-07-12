#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <optional>
#include <d3d12shader.h>

using namespace Microsoft::WRL;

namespace GameEngine {
class GraphicsDevice;

/// @brief シェーダータイプ
enum class ShaderType {
   Vertex,
   Pixel,
   Compute,
   Geometry,
   Hull,
   Domain
};

/// @brief シェーダー情報
struct ShaderInfo {
   std::string name;
   std::wstring filePath;
   ShaderType type;
   std::wstring entryPoint = L"main";
   std::vector<std::string> defines;
   std::vector<std::string> includes;
};

/// @brief シェーダー入力パラメータ情報
struct ShaderInputParameterInfo {
   std::string semanticName;
   UINT semanticIndex = 0;
   BYTE mask = 0;
   D3D_REGISTER_COMPONENT_TYPE componentType = D3D_REGISTER_COMPONENT_FLOAT32;
};

/// @brief シェーダーリソースバインド情報
struct ShaderResourceBindingInfo {
   std::string name;
   D3D_SHADER_INPUT_TYPE type = D3D_SIT_CBUFFER;
   UINT bindPoint = 0;
   UINT bindCount = 0;
   UINT space = 0;
};

/// @brief 定数バッファ情報
struct ShaderConstantBufferInfo {
   std::string name;
   UINT size = 0;
   UINT variableCount = 0;
};

/// @brief シェーダーリフレクション情報
struct ShaderReflectionInfo {
   bool isValid = false;
   std::vector<ShaderInputParameterInfo> inputParameters;
   std::vector<ShaderResourceBindingInfo> boundResources;
   std::vector<ShaderConstantBufferInfo> constantBuffers;
};

/// @brief パイプライン向けルートパラメータ解決テーブル
struct PipelineRootParameterTable {
   bool hasReflectionData = false;
   std::unordered_map<std::string, UINT> slotBySemanticName;
};

struct ResolveStats {
   uint64_t requests = 0;
   uint64_t hits = 0;
   uint64_t misses = 0;
};

struct ShaderStageMatchInfo {
   bool hasReflection = false;
   uint32_t resourceCount = 0;
   uint32_t matchedByName = 0;
};

struct PipelineStageMatchInfo {
   ShaderStageMatchInfo vertex;
   ShaderStageMatchInfo pixel;
};

/// @brief コンパイル済みシェーダー
struct CompiledShader {
   ComPtr<IDxcBlob> blob;
   ShaderType type;
   std::string name;
   std::wstring filePath;
   std::wstring entryPoint = L"main";
   std::vector<std::string> defines;
   ShaderReflectionInfo reflection;
   uint64_t fileTimestamp = 0; // ホットリロード用
};

/// @brief シェーダー管理クラス
class ShaderManager {
public:
   /// @brief シェーダーマネージャーの初期化
   /// @param device グラフィックスデバイス
   /// @return 初期化とシェーダーレジストリ読み込みに成功した場合はtrue
   bool Initialize(GraphicsDevice* device);

   /// @brief シェーダーレジストリファイルから読み込み
   /// @param registryFilePath レジストリファイルのパス
   /// @return 成功時はtrue
   bool LoadShaderRegistry(const std::wstring& registryFilePath = L"resources/engine/shaders/shader_registry.json");

   /// @brief 頂点シェーダーをコンパイルして登録
   /// @param name シェーダー名
   /// @param filePath HLSLファイルパス
   /// @param defines マクロ定義（オプション）
   /// @return コンパイル成功時はtrue
   bool LoadVertexShader(const std::string& name, const std::wstring& filePath, const std::vector<std::string>& defines = {});

   /// @brief ピクセルシェーダーをコンパイルして登録
   /// @param name シェーダー名
   /// @param filePath HLSLファイルパス
   /// @param defines マクロ定義（オプション）
   /// @return コンパイル成功時はtrue
   bool LoadPixelShader(const std::string& name, const std::wstring& filePath, const std::vector<std::string>& defines = {});

   /// @brief コンピューティングシェーダーをコンパイルして登録
   /// @param name シェーダー名
   /// @param filePath HLSLファイルパス
   /// @param defines マクロ定義（オプション）
   /// @return コンパイル成功時はtrue
   bool LoadComputeShader(const std::string& name, const std::wstring& filePath, const std::vector<std::string>& defines = {});

   /// @brief ジオメトリシェーダーをコンパイルして登録
   /// @param name シェーダー名
   /// @param filePath HLSLファイルパス
   /// @param defines マクロ定義（オプション）
   /// @return コンパイル成功時はtrue
   bool LoadGeometryShader(const std::string& name, const std::wstring& filePath, const std::vector<std::string>& defines = {});

   /// @brief ハルシェーダーをコンパイルして登録
   /// @param name シェーダー名
   /// @param filePath HLSLファイルパス
   /// @param defines マクロ定義（オプション）
   /// @return コンパイル成功時はtrue
   bool LoadHullShader(const std::string& name, const std::wstring& filePath, const std::vector<std::string>& defines = {});

   /// @brief ドメインシェーダーをコンパイルして登録
   /// @param name シェーダー名
   /// @param filePath HLSLファイルパス
   /// @param defines マクロ定義（オプション）
   /// @return コンパイル成功時はtrue
   bool LoadDomainShader(const std::string& name, const std::wstring& filePath, const std::vector<std::string>& defines = {});

   /// @brief シェーダーを読み込み（タイプ指定）
   /// @param info シェーダー情報
   /// @return コンパイル成功時はtrue
   bool LoadShader(const ShaderInfo& info);

   /// @brief 頂点シェーダーを取得
   /// @param name シェーダー名
   /// @return シェーダーブロブ、見つからない場合はnullptr
   IDxcBlob* GetVertexShader(const std::string& name) const;

   /// @brief ピクセルシェーダーを取得
   /// @param name シェーダー名
   /// @return シェーダーブロブ、見つからない場合はnullptr
   IDxcBlob* GetPixelShader(const std::string& name) const;

   /// @brief コンピュートシェーダーを取得
   /// @param name シェーダー名
   /// @return シェーダーブロブ、見つからない場合はnullptr
   IDxcBlob* GetComputeShader(const std::string& name) const;

   /// @brief ジオメトリシェーダーを取得
   /// @param name シェーダー名
   /// @return シェーダーブロブ、見つからない場合はnullptr
   IDxcBlob* GetGeometryShader(const std::string& name) const;

   /// @brief ハルシェーダーを取得
   /// @param name シェーダー名
   /// @return シェーダーブロブ、見つからない場合はnullptr
   IDxcBlob* GetHullShader(const std::string& name) const;

   /// @brief ドメインシェーダーを取得
   /// @param name シェーダー名
   /// @return シェーダーブロブ、見つからない場合はnullptr
   IDxcBlob* GetDomainShader(const std::string& name) const;

   /// @brief シェーダーを取得（タイプ指定）
   /// @param name シェーダー名
   /// @param type シェーダータイプ
   /// @return シェーダーブロブ、見つからない場合はnullptr
   IDxcBlob* GetShader(const std::string& name, ShaderType type) const;

   /// @brief シェーダーのリフレクション情報を取得
   /// @param name シェーダー名
   /// @param type シェーダータイプ
   /// @return リフレクション情報、見つからない場合はnullptr
   const ShaderReflectionInfo* GetShaderReflection(const std::string& name, ShaderType type) const;

   /// @brief 互換用の空ルートパラメータ解決テーブルを取得
   const PipelineRootParameterTable& GetObject3DRootParameterTable() const;

   /// @brief 互換用テーブルからObject3D向けの意味名を解決
   std::optional<UINT> ResolveObject3DRootParameter(const std::string& semanticName) const;

   /// @brief 互換用テーブルから任意パイプライン向けの意味名を解決
   std::optional<UINT> ResolvePipelineRootParameter(const std::string& pipelineName, const std::string& semanticName) const;

   /// @brief 互換用の任意パイプライン向け解決テーブルを取得
   const PipelineRootParameterTable* GetPipelineRootParameterTable(const std::string& pipelineName) const;

   /// @brief 解決統計を取得
   ResolveStats GetResolveStats() const { return resolveStats_; }

   /// @brief パイプライン別の解決統計を取得
   std::unordered_map<std::string, ResolveStats> GetPipelineResolveStats() const { return pipelineResolveStats_; }

   /// @brief 主要パイプライン解決テーブルをログ出力
   void LogRootParameterTablesDebug() const;

   /// @brief パイプライン別ステージ一致情報を取得
   std::unordered_map<std::string, PipelineStageMatchInfo> GetPipelineStageMatchInfos() const;

   /// @brief シェーダーが存在するか確認
   /// @param name シェーダー名
   /// @param type シェーダータイプ
   /// @return 存在する場合はtrue
   bool HasShader(const std::string& name, ShaderType type) const;

   /// @brief すべてのシェーダーをクリア
   void Clear();

   /// @brief シェーダーをアンロード
   /// @param name シェーダー名
   /// @param type シェーダータイプ
   void UnloadShader(const std::string& name, ShaderType type);

   /// @brief シェーダーをリロード（開発用）
   /// @param name シェーダー名
   /// @param type シェーダータイプ
   /// @return 成功時はtrue
   bool ReloadShader(const std::string& name, ShaderType type);

   /// @brief すべてのシェーダーをリロード（開発用）
   void ReloadAllShaders();

private:
   GraphicsDevice* device_ = nullptr;

   // DXCインターフェース
   ComPtr<IDxcUtils> dxcUtils_;
   ComPtr<IDxcCompiler3> dxcCompiler_;
   ComPtr<IDxcIncludeHandler> includeHandler_;

   // シェーダー格納用コンテナ（統一管理）
   std::unordered_map<std::string, CompiledShader> shaders_;
   std::unordered_map<std::string, PipelineRootParameterTable> pipelineRootTables_;
   mutable ResolveStats resolveStats_{};
   mutable std::unordered_map<std::string, ResolveStats> pipelineResolveStats_;

   /// @brief DXCコンポーネントの初期化
   void InitializeDXC();

   /// @brief シェーダーコンパイル処理
   /// @param filePath HLSLファイルパス
   /// @param profile シェーダープロファイル
   /// @param defines マクロ定義
   /// @return コンパイル済みシェーダーブロブ
   ComPtr<IDxcBlob> CompileShader(
	  const std::wstring& filePath,
	  const wchar_t* profile,
    const std::wstring& entryPoint = L"main",
	  const std::vector<std::string>& defines = {}
   );

   /// @brief シェーダーリフレクション情報を抽出
   ShaderReflectionInfo ExtractReflectionInfo(IDxcBlob* shaderBlob, ShaderType type) const;

   /// @brief 互換用ルートパラメータ解決テーブルをクリア
   void BuildObject3DRootParameterTable();

   /// @brief パイプライン向け互換解決テーブルをクリア
   void BuildPipelineRootParameterTables();

   /// @brief シェーダーキーを生成
   /// @param name シェーダー名
   /// @param type シェーダータイプ
   /// @return キー文字列
   std::string CreateShaderKey(const std::string& name, ShaderType type) const;

   /// @brief シェーダータイプからプロファイルを取得
   /// @param type シェーダータイプ
   /// @return プロファイル文字列
   const wchar_t* GetShaderProfile(ShaderType type) const;

   /// @brief ファイルのタイムスタンプを取得
   /// @param filePath ファイルパス
   /// @return タイムスタンプ
   uint64_t GetFileTimestamp(const std::wstring& filePath) const;
};
}
