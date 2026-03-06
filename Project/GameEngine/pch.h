#pragma once
#define NOMINMAX

#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#include "../externals/DirectXTex/d3dx12.h"
#include "../externals/DirectXTex/DirectXTex.h"

#include <cstdint>
#include <cassert>
#include <cstdlib>	
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <map>
#include <chrono>

#include "Utility/Logger.h"
#include "Utility/VectorMath.h"