#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <optional>

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
   std::vector<std::string> defines;
   std::vector<std::string> includes;
};

/// @brief コンパイル済みシェーダー
struct CompiledShader {
   ComPtr<IDxcBlob> blob;
   ShaderType type;
   std::string name;
   std::wstring filePath;
   uint64_t fileTimestamp = 0; // ホットリロード用
};

/// @brief シェーダー管理クラス
class ShaderManager {
public:
   /// @brief シェーダーマネージャーの初期化
   /// @param device グラフィックスデバイス
   void Initialize(GraphicsDevice* device);

   /// @brief シェーダーレジストリファイルから読み込み
   /// @param registryFilePath レジストリファイルのパス
   /// @return 成功時はtrue
   bool LoadShaderRegistry(const std::wstring& registryFilePath = L"resources/shaders/shader_registry.json");

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

   /// @brief 事前定義されたシェーダーを読み込み（後方互換性用）
   void LoadPredefinedShaders();

private:
   GraphicsDevice* device_ = nullptr;

   // DXCインターフェース
   ComPtr<IDxcUtils> dxcUtils_;
   ComPtr<IDxcCompiler3> dxcCompiler_;
   ComPtr<IDxcIncludeHandler> includeHandler_;

   // シェーダー格納用コンテナ（統一管理）
   std::unordered_map<std::string, CompiledShader> shaders_;

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
	  const std::vector<std::string>& defines = {}
   );

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
