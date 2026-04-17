#include "pch.h"
#include "Sprite.h"
#include "Texture.h"
#include "Component/MaterialComponent.h"
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include <algorithm>

namespace GameEngine {
Sprite::Sprite() {
   sRegisteredSprites_.push_back(this);

   static uint32_t spriteCounter = 0;
   SetObjectName("Sprite_" + std::to_string(++spriteCounter));
}

Sprite::~Sprite() {
   auto it = std::find(sRegisteredSprites_.begin(), sRegisteredSprites_.end(), this);
   if (it != sRegisteredSprites_.end()) {
	  sRegisteredSprites_.erase(it);
   }
}

const std::vector<Sprite*>& Sprite::GetRegisteredSprites() {
   return sRegisteredSprites_;
}

void Sprite::Create(const Vector2& size, Material* material, const Vector2& anchorPoint) {
   size_ = size;

   mesh_ = std::make_unique<Mesh>();
   mesh_->CreateSprite(size_.x, size_.y);

   transformationMatrix_ = std::make_unique<TransformationMatrix>();
   transformationMatrix_->Create();

   if (material) {
      if (auto* materialComponent = GetComponent<MaterialComponent>()) {
		 materialComponent->AssignMaterial(material);
	  }
   }

   AddComponent<RenderComponent>();

   anchorPoint_ = anchorPoint;

   auto* transformComponent = GetComponent<TransformComponent>();
   if (transformComponent) {
	  transformComponent->transform.scale = { 1.0f, 1.0f, 1.0f };
	  transformComponent->transform.translation.z = 1.0f;
   }
   // テクスチャサイズをリセット（UpdateTextureCoordinatesで自動設定されるように）
   textureLeftTop_ = { 0.0f, 0.0f };
   textureSize_ = { 0.0f, 0.0f };  // 0にすることで自動設定をトリガー

   // 初期頂点位置を設定（これがないと初期状態で描画されない）
   UpdateVertexPositions();
}

void Sprite::SetAnchorPoint(const Vector2& anchorPoint) {
   anchorPoint_ = anchorPoint;
}

void Sprite::SetSize(const Vector2& size) {
   size_ = size;
}

void Sprite::SetScale(const Vector2& scale) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }
   transformComponent->transform.scale.x = scale.x;
   transformComponent->transform.scale.y = scale.y;
   transformComponent->transform.scale.z = 1.0f;
}

void Sprite::SetPosition(const Vector2& position) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }
   transformComponent->transform.translation.x = position.x;
   transformComponent->transform.translation.y = position.y;
   transformComponent->transform.translation.z = 1.0f;
}

void Sprite::SetRotation(float rotation) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }
   transformComponent->transform.rotation.x = 0.0f;
   transformComponent->transform.rotation.y = 0.0f;
   transformComponent->transform.rotation.z = rotation;
}

Vector2 Sprite::GetScale() const {
   const auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return Vector2(1.0f, 1.0f);
   }
   return Vector2{ transformComponent->transform.scale.x, transformComponent->transform.scale.y };
}

Vector2 Sprite::GetPosition() const {
   const auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return Vector2(0.0f, 0.0f);
   }
   return Vector2{ transformComponent->transform.translation.x, transformComponent->transform.translation.y };
}

float Sprite::GetRotation() const {
   const auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return 0.0f;
   }
   return transformComponent->transform.rotation.z;
}

void Sprite::Update(Camera* camera, Texture* texture) {
 const auto* transformComponent = GetComponent<TransformComponent>();
	if (!transformComponent || !transformationMatrix_) {
	   return;
	}

   UpdateVertexPositions();
   UpdateTextureCoordinates(texture);

   // 最終的なワールド行列
   Matrix4x4 worldMatrix = MakeAffineMatrix(transformComponent->transform);
   if (transformComponent->useParentMatrix) {
	  worldMatrix = worldMatrix * transformComponent->parentMatrix;
   }
   Matrix4x4 wVPMatrix = worldMatrix * camera->GetViewProjectionMatrix();
   transformationMatrix_->GetTransformationMatrixData()->wVP = wVPMatrix;
   transformationMatrix_->GetTransformationMatrixData()->world = worldMatrix;
}

void Sprite::UpdateMatrixForUI(Camera* camera, Texture* texture, AnchorPoint anchorPoint, uint32_t screenWidth, uint32_t screenHeight) {
   const auto* transformComponent = GetComponent<TransformComponent>();
	if (!transformComponent || !transformationMatrix_) {
	   return;
	}

   // 頂点位置の更新
   UpdateVertexPositions();
   // テクスチャ座標の更新
   UpdateTextureCoordinates(texture);


   // アンカーポイントに基づいてベース座標を計算
   Vector3 anchorPos = CalculateAnchorPosition(anchorPoint, screenWidth, screenHeight);

   Transform finalTransform = transformComponent->transform;
   // アンカーポイントを考慮した座標調整
   finalTransform.translation.x += anchorPos.x;
   finalTransform.translation.y += anchorPos.y;
 finalTransform.translation.z = transformComponent->transform.translation.z;

   // 最終的なワールド行列
   Matrix4x4 worldMatrix = MakeAffineMatrix(finalTransform);
   if (transformComponent->useParentMatrix) {
	  worldMatrix = worldMatrix * transformComponent->parentMatrix;
   }

   Matrix4x4 wVPMatrix = worldMatrix * camera->GetViewProjectionMatrix();
   transformationMatrix_->GetTransformationMatrixData()->wVP = wVPMatrix;
   transformationMatrix_->GetTransformationMatrixData()->world = worldMatrix;
}

Vector3 Sprite::CalculateAnchorPosition(AnchorPoint anchorPoint, uint32_t screenWidth, uint32_t screenHeight) const {
   Vector3 position = { 0.0f, 0.0f, 0.0f };

   float halfWidth = screenWidth * 0.5f;
   float halfHeight = screenHeight * 0.5f;

   switch (anchorPoint) {
	  case AnchorPoint::TopLeft:
		 position.x = -halfWidth;
		 position.y = halfHeight;  // 上部なのでプラス
		 break;
	  case AnchorPoint::TopCenter:
		 position.x = 0.0f;
		 position.y = halfHeight;  // 上部なのでプラス
		 break;
	  case AnchorPoint::TopRight:
		 position.x = halfWidth;
		 position.y = halfHeight;  // 上部なのでプラス
		 break;
	  case AnchorPoint::MiddleLeft:
		 position.x = -halfWidth;
		 position.y = 0.0f;
		 break;
	  case AnchorPoint::MiddleCenter:
		 position.x = 0.0f;
		 position.y = 0.0f;
		 break;
	  case AnchorPoint::MiddleRight:
		 position.x = halfWidth;
		 position.y = 0.0f;
		 break;
	  case AnchorPoint::BottomLeft:
		 position.x = -halfWidth;
		 position.y = -halfHeight;  // 下部なのでマイナス
		 break;
	  case AnchorPoint::BottomCenter:
		 position.x = 0.0f;
		 position.y = -halfHeight;  // 下部なのでマイナス
		 break;
	  case AnchorPoint::BottomRight:
		 position.x = halfWidth;
		 position.y = -halfHeight;  // 下部なのでマイナス
		 break;
   }

   return position;
}

void Sprite::UpdateVertexPositions() {
   float left = 0.0f - anchorPoint_.x * size_.x;
   float right = size_.x - anchorPoint_.x * size_.x;
   float top = size_.y - anchorPoint_.y * size_.y;
   float bottom = 0.0f - anchorPoint_.y * size_.y;

   if (isFlipX_) {
	  left = -left;
	  right = -right;
   }

   if (isFlipY_) {
	  top = -top;
	  bottom = -bottom;
   }

   mesh_->GetVertexData()[0].position = Vector4(left, bottom, 0.0f, 1.0f);
   mesh_->GetVertexData()[1].position = Vector4(left, top, 0.0f, 1.0f);
   mesh_->GetVertexData()[2].position = Vector4(right, bottom, 0.0f, 1.0f);
   mesh_->GetVertexData()[3].position = Vector4(right, top, 0.0f, 1.0f);
}

void Sprite::UpdateTextureCoordinates(Texture* texture) {
   const DirectX::TexMetadata& metadata = texture->GetMetadata();

   // textureSize_が未設定（0または異常値）の場合、テクスチャ全体を使用
   Vector2 actualTextureSize = textureSize_;
   Vector2 actualTextureLeftTop = textureLeftTop_;

   // テクスチャサイズが0、デフォルト値、または実際のテクスチャサイズより大きい場合
   // テクスチャ全体を使用するように調整
   if (textureSize_.x <= 0.0f || textureSize_.y <= 0.0f ||
	  textureSize_.x > static_cast<float>(metadata.width) ||
	  textureSize_.y > static_cast<float>(metadata.height)) {
	  actualTextureSize.x = static_cast<float>(metadata.width);
	  actualTextureSize.y = static_cast<float>(metadata.height);
	  actualTextureLeftTop = { 0.0f, 0.0f };
	  textureSize_ = actualTextureSize;
   }

   float texLeft = actualTextureLeftTop.x / static_cast<float>(metadata.width);
   float texRight = (actualTextureLeftTop.x + actualTextureSize.x) / static_cast<float>(metadata.width);
   float texTop = actualTextureLeftTop.y / static_cast<float>(metadata.height);
   float texBottom = (actualTextureLeftTop.y + actualTextureSize.y) / static_cast<float>(metadata.height);

   mesh_->GetVertexData()[0].texCoord = Vector2(texLeft, texBottom);
   mesh_->GetVertexData()[1].texCoord = Vector2(texLeft, texTop);
   mesh_->GetVertexData()[2].texCoord = Vector2(texRight, texBottom);
   mesh_->GetVertexData()[3].texCoord = Vector2(texRight, texTop);
}

}