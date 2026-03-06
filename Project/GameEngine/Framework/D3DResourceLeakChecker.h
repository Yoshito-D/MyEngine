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
   struct D3DResourceLeakChecker {
	  ~D3DResourceLeakChecker() {
		 Microsoft::WRL::ComPtr<IDXGIDebug> debug;
		 if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
			debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		 }
	  }
   };
}
#endif