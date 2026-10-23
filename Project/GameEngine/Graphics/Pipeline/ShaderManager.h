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
   std::string name;                  ///< 識別とエディター表示に使う名前。
   std::wstring filePath;             ///< コンパイル対象となるHLSLソースファイルのパス。
   ShaderType type;                   ///< コンパイルする頂点・ピクセル・Computeなどのシェーダーステージ。
   std::wstring entryPoint = L"main"; ///< シェーダーのコンパイル開始関数名。
   std::vector<std::string> defines;  ///< シェーダーのコンパイル時に適用するプリプロセッサ定義。
   std::vector<std::string> includes; ///< シェーダーが参照するインクルードファイル。
};

/// @brief シェーダー入力パラメータ情報
struct ShaderInputParameterInfo {
   std::string semanticName; ///< 頂点入力要素をシェーダーへ対応付けるセマンティクス名。
   UINT semanticIndex = 0;   ///< 同名セマンティクス内の要素番号。
   BYTE mask = 0;            ///< 入力で使用するxyzw成分を示すビット集合。
   /// @brief シェーダーレジスタ成分の整数・浮動小数点などの型。
   D3D_REGISTER_COMPONENT_TYPE componentType = D3D_REGISTER_COMPONENT_FLOAT32;
};

/// @brief シェーダーリソースバインド情報
struct ShaderResourceBindingInfo {
   std::string name;                                        ///< 識別とエディター表示に使う名前。
   D3D_SHADER_INPUT_TYPE type = D3D_SIT_CBUFFER;            ///< 定数バッファ・テクスチャ・UAVなどのリソース種別。
   UINT bindPoint = 0;                                      ///< シェーダーリソースの先頭レジスタ番号。
   UINT bindCount = 0;                                      ///< 連続してバインドするリソースの数。
   UINT space = 0;                                          ///< シェーダーレジスタの名前空間番号。
   D3D_SRV_DIMENSION dimension = D3D_SRV_DIMENSION_UNKNOWN; ///< シェーダーリソースの次元と参照方式。
};

/// @brief 定数バッファー内の相対オフセットを持つ、リフレクション結果の末端フィールド。
struct ShaderVariableInfo {
   std::string name;                                         ///< 識別とエディター表示に使う名前。
   UINT offset = 0;                                          ///< 定数バッファ先頭からこのパラメーターまでのバイト数。
   D3D_SHADER_VARIABLE_TYPE type = D3D_SVT_VOID;             ///< 定数の浮動小数点・整数などの要素型。
   D3D_SHADER_VARIABLE_CLASS variableClass = D3D_SVC_SCALAR; ///< 定数のスカラー・ベクトル・行列などの分類。
   UINT rows = 0;                                            ///< 行列または定数要素の行数。
   UINT columns = 0;                                         ///< 行列または定数要素の列数。
   UINT elements = 0;                                        ///< 配列として宣言された定数の要素数。
};

/// @brief 定数バッファ情報
struct ShaderConstantBufferInfo {
   std::string name;                          ///< 識別とエディター表示に使う名前。
   UINT size = 0;                             ///< 定数バッファ全体のサイズ（バイト）。
   UINT variableCount = 0;                    ///< 定数バッファ内の変数の数。
   std::vector<ShaderVariableInfo> variables; ///< 定数バッファ内の変数名・型・配置情報。
};

/// @brief シェーダーリフレクション情報
struct ShaderReflectionInfo {
   bool isValid = false;                                  ///< 解析または検証に成功し、結果を利用できるかどうか。
   std::vector<ShaderInputParameterInfo> inputParameters; ///< シェーダーの入力セマンティクスと成分情報。
   std::vector<ShaderResourceBindingInfo> boundResources; ///< シェーダーが参照するリソースとレジスタ配置。
   std::vector<ShaderConstantBufferInfo> constantBuffers; ///< シェーダー内の定数バッファと変数配置。
};

/// @brief コンパイル済みシェーダー
struct CompiledShader {
   ComPtr<IDxcBlob> blob;             ///< コンパイル済みシェーダーの所有バイトコード。
   ShaderType type;                   ///< コンパイル済みバイトコードのシェーダーステージ。
   std::string name;                  ///< 識別とエディター表示に使う名前。
   std::wstring filePath;             ///< ホットリロードの更新確認に使うHLSLソースファイルのパス。
   std::wstring entryPoint = L"main"; ///< シェーダーのコンパイル開始関数名。
   std::vector<std::string> defines;  ///< シェーダーのコンパイル時に適用するプリプロセッサ定義。
   ShaderReflectionInfo reflection;   ///< コンパイル済みシェーダーから取得した入力・リソース情報。
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
   bool LoadVertexShader(
      const std::string& name, const std::wstring& filePath, const std::vector<std::string>& defines = {});

   /// @brief ピクセルシェーダーをコンパイルして登録
   /// @param name シェーダー名
   /// @param filePath HLSLファイルパス
   /// @param defines マクロ定義（オプション）
   /// @return コンパイル成功時はtrue
   bool LoadPixelShader(
      const std::string& name, const std::wstring& filePath, const std::vector<std::string>& defines = {});

   /// @brief コンピューティングシェーダーをコンパイルして登録
   /// @param name シェーダー名
   /// @param filePath HLSLファイルパス
   /// @param defines マクロ定義（オプション）
   /// @return コンパイル成功時はtrue
   bool LoadComputeShader(
      const std::string& name, const std::wstring& filePath, const std::vector<std::string>& defines = {});

   /// @brief ジオメトリシェーダーをコンパイルして登録
   /// @param name シェーダー名
   /// @param filePath HLSLファイルパス
   /// @param defines マクロ定義（オプション）
   /// @return コンパイル成功時はtrue
   bool LoadGeometryShader(
      const std::string& name, const std::wstring& filePath, const std::vector<std::string>& defines = {});

   /// @brief ハルシェーダーをコンパイルして登録
   /// @param name シェーダー名
   /// @param filePath HLSLファイルパス
   /// @param defines マクロ定義（オプション）
   /// @return コンパイル成功時はtrue
   bool LoadHullShader(
      const std::string& name, const std::wstring& filePath, const std::vector<std::string>& defines = {});

   /// @brief ドメインシェーダーをコンパイルして登録
   /// @param name シェーダー名
   /// @param filePath HLSLファイルパス
   /// @param defines マクロ定義（オプション）
   /// @return コンパイル成功時はtrue
   bool LoadDomainShader(
      const std::string& name, const std::wstring& filePath, const std::vector<std::string>& defines = {});

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
   GraphicsDevice* device_ = nullptr; ///< GPUリソースや描画コマンドの生成に使うデバイスへの参照。

   // DXCインターフェース
   ComPtr<IDxcUtils> dxcUtils_;                ///< DXCのバイト列作成とリフレクションを行う所有ユーティリティ。
   ComPtr<IDxcCompiler3> dxcCompiler_;         ///< HLSLをコンパイルする所有DXCコンパイラー。
   ComPtr<IDxcIncludeHandler> includeHandler_; ///< シェーダーのインクルードファイルを解決する所有ハンドラー。

   // シェーダー格納用コンテナ（統一管理）
   /// @brief 登録名から引くコンパイル済みシェーダーのキャッシュ。
   std::unordered_map<std::string, CompiledShader> shaders_;

   /// @brief DXCコンポーネントの初期化
   void InitializeDXC();

   /// @brief シェーダーコンパイル処理
   /// @param filePath HLSLファイルパス
   /// @param profile シェーダープロファイル
   /// @param defines マクロ定義
   /// @return コンパイル済みシェーダーブロブ
   ComPtr<IDxcBlob> CompileShader(const std::wstring& filePath, const wchar_t* profile,
      const std::wstring& entryPoint = L"main", const std::vector<std::string>& defines = {});

   /// @brief シェーダーリフレクション情報を抽出
   ShaderReflectionInfo ExtractReflectionInfo(IDxcBlob* shaderBlob, ShaderType type) const;

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
