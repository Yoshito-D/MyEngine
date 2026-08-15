#include "pch.h"
#include "ShaderCompiler.h"
#include "Utility/Logger.h"

namespace {

std::wstring Utf8ToWString(const std::string& str) {
   if (str.empty()) {
	  return {};
   }

   const int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
   if (size <= 0) {
	  return {};
   }

   std::wstring result(static_cast<size_t>(size), L'\0');
   MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, result.data(), size);
   if (!result.empty() && result.back() == L'\0') {
	  result.pop_back();
   }
   return result;
}
}

namespace GameEngine {
ComPtr<IDxcBlob> ShaderCompiler::CompileShader(
   const std::wstring& filePath,
   const wchar_t* profile,
   IDxcUtils* dxcUtils,
   IDxcCompiler3* dxcCompiler,
   IDxcIncludeHandler* includeHandler,
   const std::wstring& entryPoint,
   const std::vector<std::string>& defines
) {
   // 1. hlslファイルを読む

   // これからシェーダーをコンパイルする旨をログに出す
   Logger::Info(std::format(L"Begin CompileShader, path:{}, profile:{}", filePath, profile));
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

   std::vector<std::wstring> defineArguments;
   // UTF-8のJSON defineをDXC引数用のUTF-16文字列へ変換し、argumentsが参照する実体を
   // コンパイル完了まで別vectorで保持する。
   defineArguments.reserve(defines.size());
   for (const auto& define : defines) {
	  if (define.empty()) {
		 continue;
	  }
	  defineArguments.push_back(L"-D" + Utf8ToWString(define));
   }

   std::vector<LPCWSTR> arguments;
   arguments.reserve(12 + defineArguments.size());
   arguments.push_back(filePath.c_str()); // コンパイル対象のhlslファイル名
   arguments.push_back(L"-E");
   arguments.push_back(entryPoint.empty() ? L"main" : entryPoint.c_str()); // エントリーポイント
   arguments.push_back(L"-T");
   arguments.push_back(profile); // ShaderProfile
   arguments.push_back(L"-Zi");
   // デバッグ情報をBlobへ埋め込み、最適化を無効化してGPUデバッガ上のHLSL追跡を優先する。
   // 行列はエンジン側のメモリ規約に合わせてrow-majorでコンパイルする。
   arguments.push_back(L"-Qembed_debug");
   arguments.push_back(L"-Od");
   arguments.push_back(L"-Zpr");

   for (const auto& defineArg : defineArguments) {
	  arguments.push_back(defineArg.c_str());
   }

   // 実際にShaderをコンパイルする
   IDxcResult* shaderResult = nullptr;
   hr = dxcCompiler->Compile(
	  &shaderSourceBuffer, // 読み込んだファイル
      arguments.data(),    // コンパイルオプション
	  static_cast<UINT>(arguments.size()), // コンパイルオプションの数
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
	  Logger::Info(shaderError->GetStringPointer());
	  // 警告・エラーはダメ
	  assert(false);
   }

   // 4. Compile結果を受け取って返す

   // コンパイル結果から実行用のバイナリ部分を取得
   IDxcBlob* shaderBlob = nullptr;
   hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
   assert(SUCCEEDED(hr));
   // 成功したログを出す
   Logger::Info(std::format(L"Compile Succeeded, path:{}, profile:{}", filePath, profile));
   // もう使わないリソースを解放
   shaderSource->Release();
   shaderResult->Release();

   // 実行用のバイナリを返却
   return shaderBlob;
}
}