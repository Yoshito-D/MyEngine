#include "pch.h"  
#include "Audio.h"  
#include <fstream>  
#include <cassert>  

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfobjects.h>
#include <comdef.h>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")

namespace GameEngine {
void Audio::Initialize() {
   HRESULT result = S_FALSE;
   result = MFStartup(MF_VERSION);
   assert(SUCCEEDED(result));

   result = XAudio2Create(xAudio2_.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR);
   assert(SUCCEEDED(result));

   result = xAudio2_->CreateMasteringVoice(&masteringVoice_);
   assert(SUCCEEDED(result));
}

void Audio::Finalize() {
   if (xAudio2_) {
	  xAudio2_->StopEngine();
	  masteringVoice_->DestroyVoice();
   }
   MFShutdown();
}
}