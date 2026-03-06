#include "pch.h"
#include "ShaderCompiler.h"
#include "Utility/Logger.h"

namespace {
Logger& log_ = Logger::GetInstance();
}

namespace GameEngine {
ComPtr<IDxcBlob> ShaderCompiler::CompileShader(const std::wstring& filePath, const wchar_t* profile, IDxcUtils* dxcUtils, IDxcCompiler3* dxcCompiler, IDxcIncludeHandler* includeHandler) {
   // 1. hlslファイルを読む

   // これからシェーダーをコンパイルする旨をログに出す
   log_.Log(std::format(L"Begin CompileShader, path:{}, profile:{}", filePath, profile));
   // hlslファイルを読む
   IDxcBlobEncoding* shaderSource = nullptr;
   HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
   // 読めなかったら止める
   assert(SUCCEEDED(hr));
   // 読み込んだファイルの内容を設定する
   DxcBuffer shaderSourceBuffer;
   shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
   shaderSourceBuffer.Size = shaderSource->GetBufferSize();
   shaderSourceBuffer.Encoding = DXC_CP_UTF8; // UTF8の文字コードであることを通知 

   // 2. Compileする

   LPCWSTR arguments[] = {
	   filePath.c_str(), // コンパイル対象のhlslファイル名
	   L"-E",L"main", // エントリーポイントの指定
	   L"-T",profile, // ShaderProfileの設定
	   L"-Zi",L"-Qembed_debug", // デバッグ用の情報を埋め込む
	   L"-Od", // 最適化を外しておく
	   L"-Zpr" // メモリレイアウトは行優先
   };

   // 実際にShaderをコンパイルする
   IDxcResult* shaderResult = nullptr;
   hr = dxcCompiler->Compile(
	  &shaderSourceBuffer, // 読み込んだファイル
	  arguments,           // コンパイルオプション
	  _countof(arguments), // コンパイルオプションの数
	  includeHandler,      // includeが含まれた諸々
	  IID_PPV_ARGS(&shaderResult) // コンパイル結果
   );
   // コンパイルエラーではなくdxcが起動できないなど致命的な状況
   assert(SUCCEEDED(hr));

   // 3. 警告・エラーが出ていないか確認する

   // 警告・エラーが出てたらログを出して止める
   IDxcBlobUtf8* shaderError = nullptr;
   shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
   if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
	  log_.Log(shaderError->GetStringPointer());
	  // 警告・エラーはダメ
	  assert(false);
   }

   // 4. Compile結果を受け取って返す

   // コンパイル結果から実行用のバイナリ部分を取得
   IDxcBlob* shaderBlob = nullptr;
   hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
   assert(SUCCEEDED(hr));
   // 成功したログを出す
   log_.Log(std::format(L"Compile Succeeded, path:{}, profile:{}", filePath, profile));
   // もう使わないリソースを解放
   shaderSource->Release();
   shaderResult->Release();

   // 実行用のバイナリを返却
   return shaderBlob;
}
}