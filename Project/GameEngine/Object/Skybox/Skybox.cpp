#include "pch.h"
#include "Skybox.h"
#include "Component/Skybox/SkyboxComponent.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/ResourceHelper.h"
#include <algorithm>

namespace GameEngine {

std::vector<Skybox*> Skybox::sRegisteredSkyboxes_{};

namespace {
std::string BuildDefaultSkyboxName(const std::vector<Skybox*>& registeredSkyboxes) {
   // エディター上で安定して識別できるよう、生存中の Skybox と衝突しない最小番号を選ぶ。
   // 削除済みの番号は再利用し、表示名が不必要に増え続けることを避ける。
   auto exists = [&registeredSkyboxes](const std::string& name) {
	  for (const auto* skybox : registeredSkyboxes) {
		 if (skybox && skybox->GetObjectName() == name) {
			return true;
		 }
	  }
	  return false;
   };

   uint32_t index = 1;
   while (true) {
	  const std::string candidate = "Skybox_" + std::to_string(index++);
	  if (!exists(candidate)) {
		 return candidate;
	  }
   }
}
}

Skybox::Skybox() {
   // テクスチャと色の所有窓口を SkyboxComponent に一本化し、どの生成経路でも同じ構成にする。
   AddComponent<SkyboxComponent>();
   SetObjectName(BuildDefaultSkyboxName(sRegisteredSkyboxes_));
   sRegisteredSkyboxes_.push_back(this);
}

Skybox::~Skybox() {
   UnregisterSkybox(this);
}

void Skybox::UnregisterSkybox(Skybox* skybox) {
   // 破棄済みポインターを静的一覧へ残さず、名前生成やエディター列挙から参照されないようにする。
   auto it = std::find(sRegisteredSkyboxes_.begin(), sRegisteredSkyboxes_.end(), skybox);
   if (it != sRegisteredSkyboxes_.end()) {
	  sRegisteredSkyboxes_.erase(it);
   }
}

const std::vector<Skybox*>& Skybox::GetRegisteredSkyboxes() {
   return sRegisteredSkyboxes_;
}

void Skybox::Create(GraphicsDevice* device) {
   // Skybox 専用メッシュと、フレーム中に更新する定数バッファを描画準備時に確保する。
   // バッファは永続マップし、UpdateTransform／SetColor から直接最新値を書き込めるようにする。
   mesh_.CreateSkybox();

   transformResource_ = ResourceHelper::CreateBufferResource(device->GetDevice(), sizeof(SkyboxTransformData));
   transformResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformData_));
   // 最初の UpdateTransform より前に参照されても未初期化値を GPU へ送らないよう、単位行列で初期化する。
   transformData_->wVP = MakeIdentity4x4();
   transformData_->world = MakeIdentity4x4();
   transformData_->worldInverseTranspose = MakeIdentity4x4();

   materialResource_ = ResourceHelper::CreateBufferResource(device->GetDevice(), sizeof(SkyboxMaterialData));
   materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
   const auto* skyboxComponent = GetComponent<SkyboxComponent>();
   materialData_->color = skyboxComponent ? skyboxComponent->GetColor() : Vector4(1.0f, 1.0f, 1.0f, 1.0f);
}

void Skybox::SetTexture(Texture* texture) {
   if (auto* skyboxComponent = GetComponent<SkyboxComponent>()) {
      skyboxComponent->SetTexture(texture);
   }
}

Texture* Skybox::GetTexture() const {
   const auto* skyboxComponent = GetComponent<SkyboxComponent>();
   return skyboxComponent ? skyboxComponent->GetTexture() : nullptr;
}

void Skybox::SetColor(const Vector4& color) {
   // シリアライズ対象のコンポーネントと、初期化済み GPU バッファの双方を同時に更新する。
   // Create 前の呼び出しではコンポーネントに保持され、Create 時にバッファへ反映される。
   if (auto* skyboxComponent = GetComponent<SkyboxComponent>()) {
      skyboxComponent->SetColor(color);
   }
   if (materialData_) {
	  materialData_->color = color;
   }
}

ID3D12Resource* Skybox::GetTransformResource() const {
   return transformResource_.Get();
}

ID3D12Resource* Skybox::GetMaterialResource() const {
   return materialResource_.Get();
}

void Skybox::UpdateTransform(const Matrix4x4& viewProjectionMatrix) {
   // エディター等でコンポーネント値が直接変更される場合もあるため、描画直前に GPU 側の色を同期する。
   if (materialData_) {
      if (const auto* skyboxComponent = GetComponent<SkyboxComponent>()) {
         materialData_->color = skyboxComponent->GetColor();
      }
   }
   if (transformData_) {
	  // VSシェーダーは wVP のみ使用する
	  // Skybox 固有のローカル変換は持たず、呼び出し側で平行移動を除いたビュー射影行列を供給する。
	  transformData_->wVP = viewProjectionMatrix;
	  transformData_->world = MakeIdentity4x4();
	  transformData_->worldInverseTranspose = MakeIdentity4x4();
   }
}

} // namespace GameEngine
