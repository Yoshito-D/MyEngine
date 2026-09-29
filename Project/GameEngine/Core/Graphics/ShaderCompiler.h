#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <string>
#include <vector>

using namespace Microsoft::WRL;

namespace GameEngine {
/// @brief シェーダーコンパイラ
namespace ShaderCompiler {
/// @brief シェーダーをコンパイルする
/// @param filePath コンパイルするHLSLファイルのパス
/// @param profile シェーダープロファイル（例: "vs_6_0"）
/// @param dxcUtils DXCユーティリティ
/// @param dxcCompiler DXCコンパイラ
/// @param includeHandler インクルードハンドラー
/// @return コンパイル済みオブジェクト。読み込みまたはコンパイル失敗時は診断情報を出力してnullptr。
ComPtr<IDxcBlob> CompileShader(
   const std::wstring& filePath,
   const wchar_t* profile,
   IDxcUtils* dxcUtils,
   IDxcCompiler3* dxcCompiler,
   IDxcIncludeHandler* includeHandler,
   const std::wstring& entryPoint = L"main",
   const std::vector<std::string>& defines = {}
);
}
}
