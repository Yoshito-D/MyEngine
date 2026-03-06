#include "pch.h"
#include "Sound.h"

#include <mfapi.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mftransform.h>
#include <comdef.h>
#include <locale>
#include <codecvt>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")

namespace {
IXAudio2* sXAudio2_ = nullptr;
}

namespace GameEngine {
void Sound::Initialize(IXAudio2* xAudio2) {
   sXAudio2_ = xAudio2;
}

Sound::~Sound() {
   if (sourceVoice_) {
	  sourceVoice_->Stop();
	  sourceVoice_->FlushSourceBuffers();
	  sourceVoice_->DestroyVoice();
	  sourceVoice_ = nullptr;
   }
}

void Sound::Load(const std::wstring& filepath) {
   if (!sXAudio2_) return;

   HRESULT result = S_FALSE;

   // SourceReader作成
   ComPtr<IMFSourceReader> sourceReader;
   result = MFCreateSourceReaderFromURL(filepath.c_str(), nullptr, &sourceReader);
   if (FAILED(result)) throw std::runtime_error("Failed to create source reader.");

   // PCM形式での出力を指定
   ComPtr<IMFMediaType> pcmType;
   result = MFCreateMediaType(&pcmType);
   if (FAILED(result)) throw std::runtime_error("Failed to create media type.");

   result = pcmType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
   result = pcmType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
   result = sourceReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pcmType.Get());

   if (FAILED(result)) throw std::runtime_error("Failed to set media type.");

   audioData_.clear();

   // フォーマット取得
   ComPtr<IMFMediaType> nativeType;
   result = sourceReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &nativeType);
   if (FAILED(result)) throw std::runtime_error("Failed to get current media type.");

   UINT32 waveFormatSize = 0;
   WAVEFORMATEX* wf = nullptr;
   result = MFCreateWaveFormatExFromMFMediaType(nativeType.Get(), &wf, &waveFormatSize);
   if (FAILED(result)) throw std::runtime_error("Failed to create wave format.");

   memcpy(&waveFormat_, wf, sizeof(WAVEFORMATEX));
   CoTaskMemFree(wf);

   // サンプル読み込みループ
   while (true) {
	  DWORD streamIndex, flags;
	  LONGLONG timestamp;
	  ComPtr<IMFSample> sample;
	  result = sourceReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &streamIndex, &flags, &timestamp, &sample);
	  if (FAILED(result)) throw std::runtime_error("Failed to read sample.");

	  if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;

	  if (sample) {
		 ComPtr<IMFMediaBuffer> buffer;
		 result = sample->ConvertToContiguousBuffer(&buffer);
		 if (FAILED(result)) throw std::runtime_error("Failed to get contiguous buffer.");

		 BYTE* data = nullptr;
		 DWORD maxLen = 0, currentLen = 0;
		 result = buffer->Lock(&data, &maxLen, &currentLen);
		 if (FAILED(result)) throw std::runtime_error("Failed to lock buffer.");

		 audioData_.insert(audioData_.end(), data, data + currentLen);

		 result = buffer->Unlock();
		 if (FAILED(result)) throw std::runtime_error("Failed to unlock buffer.");
	  }
   }

   // 既存のVoice破棄
   if (sourceVoice_) {
	  sourceVoice_->Stop();
	  sourceVoice_->FlushSourceBuffers();
	  sourceVoice_->DestroyVoice();
	  sourceVoice_ = nullptr;
   }

   result = sXAudio2_->CreateSourceVoice(&sourceVoice_, (WAVEFORMATEX*)&waveFormat_);
   if (FAILED(result)) throw std::runtime_error("Failed to create source voice.");
}

void Sound::Play(float volume, bool loop, bool restart) {
   if (!sourceVoice_ || audioData_.empty()) return;

   XAUDIO2_VOICE_STATE state = {};
   sourceVoice_->GetState(&state);

   if (state.BuffersQueued == 0) {
	  isPlaying_ = false;
   }

   // 再生中で、再スタートしないなら volume と loop の更新だけ行う
   if (!restart && isPlaying_) {
	  sourceVoice_->SetVolume(volume);
	  isLooping_ = loop;  // ループ状態の更新（必要であれば）
	  return;
   }

   if (!restart && !isPlaying_) {
	  sourceVoice_->SetVolume(volume);
	  sourceVoice_->Start(0);
	  isLooping_ = loop;  // ループ状態の更新（必要であれば）
	  return;
   }

   // 再生し直す
   sourceVoice_->Stop();
   sourceVoice_->FlushSourceBuffers();

   buffer_ = {};
   buffer_.AudioBytes = static_cast<UINT32>(audioData_.size());
   buffer_.pAudioData = audioData_.data();
   buffer_.Flags = XAUDIO2_END_OF_STREAM;
   buffer_.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

   sourceVoice_->SubmitSourceBuffer(&buffer_);
   sourceVoice_->SetVolume(volume);
   sourceVoice_->Start(0);

   isLooping_ = loop;
   isPlaying_ = true;
}


void Sound::Stop() {
   if (sourceVoice_) {
	  sourceVoice_->Stop();
	  isPlaying_ = false;
   }
}

void Sound::Reset() {
   if (sourceVoice_) {
	  sourceVoice_->Stop();
	  sourceVoice_->FlushSourceBuffers();
   }
}

void Sound::SetVolume(float volume) {
   if (sourceVoice_) {
	  sourceVoice_->SetVolume(volume);
   }
}
}