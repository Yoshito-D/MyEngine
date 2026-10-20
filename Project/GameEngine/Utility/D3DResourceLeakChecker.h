#pragma once

#include <dxgi1_3.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <dbghelp.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#pragma comment(lib, "Dbghelp.lib")

#ifdef _DEBUG
namespace GameEngine {

/// @brief Direct3D終了時にデバッグレイヤーのリソースリークを報告する
struct D3DResourceLeakChecker {
   ~D3DResourceLeakChecker();
};
}
#endif
