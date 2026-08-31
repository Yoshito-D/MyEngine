#pragma once

// Windows／DirectX 12 の各翻訳単位で共通して利用する低レベル API。
// プリコンパイル済みヘッダーへ集約し、SDK の大きなヘッダー群を繰り返し解析するコストを抑える。
#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#ifdef _DEBUG
// DXGI のデバッグインターフェースはデバッグビルドでのみ使用し、リリース版への依存を持ち込まない。
#include <dxgidebug.h>
#endif

// DirectX 12 の補助型とテクスチャ読み込み処理は、エンジン全体で頻繁に参照される。
#include "../Externals/DirectXTex/d3dx12.h"
#include "../Externals/DirectXTex/DirectXTex.h"

// 実装ファイルで広く使用する C++ 標準ライブラリ。
#include <cstdint>
#include <cassert>
#include <cstdlib>	
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <map>
#include <chrono>
#include <numbers>

#include "Utility/Logger.h"
#include "Utility/VectorMath.h"