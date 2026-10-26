#pragma once

#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <d3d12shader.h>

using namespace Microsoft::WRL;

namespace GameEngine {
/// @brief シェーダータイプ
enum class ShaderType {
   Vertex,
   Pixel,
   Compute,
   Geometry,
   Hull,
   Domain
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
   ComPtr<IDxcBlob> blob;           ///< コンパイル済みシェーダーの所有バイトコード。
   ShaderType type;                 ///< コンパイル済みバイトコードのシェーダーステージ。
   std::string name;                ///< ファイル名から取得するシェーダーの識別名。
   std::wstring filePath;           ///< 再読み込みに使う生成済みバイナリのパス。
   ShaderReflectionInfo reflection; ///< コンパイル済みシェーダーから取得した入力・リソース情報。
};

/// @brief シェーダー管理クラス
class ShaderManager {
public:
   /// @brief 生成済みバイナリを再帰的に検出し、ファイル名からシェーダーを登録する。
   /// @param shaderDirectory ビルドで配置されたシェーダーバイナリのディレクトリ。
   /// @return すべてのバイナリの読み込みとリフレクション取得に成功した場合はtrue。
   bool Initialize(const std::wstring& shaderDirectory = L"Resources/engine/shaders");

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

   /// @brief 再ビルドされたバイナリを読み直す。失敗時は既存のシェーダーを保持する。
   /// @param name シェーダー名
   /// @param type シェーダータイプ
   /// @return 成功時はtrue
   bool ReloadShader(const std::string& name, ShaderType type);

   /// @brief ディレクトリを再走査し、追加・削除を含めて生成済みシェーダーを読み直す。
   /// @return 全件成功時はtrue。失敗時は既存のシェーダーを保持する。
   bool ReloadAllShaders();

private:
   ComPtr<IDxcUtils> dxcUtils_;   ///< バイナリ読み込みとリフレクションに使うDXCユーティリティ。
   std::wstring shaderDirectory_; ///< 再走査に使う生成済みバイナリのディレクトリ。
   std::unordered_map<std::string, CompiledShader> shaders_; ///< 識別名とステージで引くシェーダー。

   /// @brief 全バイナリの読み込みに成功した場合だけ登録内容を更新する。
   bool LoadShaderDirectory(const std::wstring& shaderDirectory);

   /// @brief ファイル名・バイトコード・リフレクションを検証してバイナリを読み込む。
   bool ReadShaderBinary(const std::wstring& filePath, CompiledShader& shader) const;

   /// @brief シェーダーリフレクション情報を抽出する。
   ShaderReflectionInfo ExtractReflectionInfo(IDxcBlob* shaderBlob, ShaderType type) const;

   /// @brief シェーダーの識別名とステージからキャッシュのキーを生成する。
   std::string CreateShaderKey(const std::string& name, ShaderType type) const;
};
}
