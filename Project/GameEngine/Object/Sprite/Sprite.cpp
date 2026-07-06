#include "pch.h"
#include "Sprite.h"
#include "Texture.h"
#include "Component/MaterialComponent.h"
#include "Component/PrimitiveMeshComponent.h"
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Core/Graphics/Mesh.h"
#include "Core/Graphics/TransformationMatrix.h"
#include <algorithm>

namespace GameEngine {

namespace {
std::string BuildDefaultSpriteName(const std::vector<Sprite*>& registeredSprites) {
   auto exists = [&registeredSprites](const std::string& name) {
	  for (const auto* sprite : registeredSprites) {
		 if (sprite && sprite->GetObjectName() == name) {
			return true;
		 }
	  }
	  return false;
   };

   uint32_t index = 1;
   while (true) {
	  const std::string candidate = "Sprite_" + std::to_string(index++);
	  if (!exists(candidate)) {
		 return candidate;
	  }
   }
}
}

Sprite::Sprite() {
   SetObjectName(BuildDefaultSpriteName(sRegisteredSprites_));
   sRegisteredSprites_.push_back(this);
}

Sprite::~Sprite() {
   UnregisterSprite(this);
}

void Sprite::UnregisterSprite(Sprite* sprite) {
   if (!sprite) {
	  return;
   }

   auto it = std::find(sRegisteredSprites_.begin(), sRegisteredSprites_.end(), sprite);
   if (it != sRegisteredSprites_.end()) {
	  sRegisteredSprites_.erase(it);
   }
}

const std::vector<Sprite*>& Sprite::GetRegisteredSprites() {
   return sRegisteredSprites_;
}

void Sprite::Create(const Vector2& size, Material* material, const Vector2& anchorPoint) {
   if (auto* primitiveMeshComponent = AddComponent<PrimitiveMeshComponent>()) {
      primitiveMeshComponent->SetPrimitiveType(PrimitiveMeshComponent::PrimitiveType::Quad);
      primitiveMeshComponent->SetQuadSize(size);
      primitiveMeshComponent->SetQuadAnchorPoint(anchorPoint);
      primitiveMeshComponent->CreateMesh();
   }

   if (material) {
      if (auto* materialComponent = GetComponent<MaterialComponent>()) {
		 materialComponent->AssignMaterial(material);
	  }
   }

   if (auto* renderComponent = AddComponent<RenderComponent>()) {
      renderComponent->renderSpace = RenderComponent::RenderSpace::Screen;
   }

   auto* transformComponent = GetComponent<TransformComponent>();
   if (transformComponent) {
	  transformComponent->transform.scale = { 1.0f, 1.0f, 1.0f };
	  transformComponent->transform.translation.z = 1.0f;
	  transformComponent->EnsureTransformationMatrix();
   }

   UpdateVertexPositions();
}

void Sprite::SetAnchorPoint(const Vector2& anchorPoint) {
   if (auto* primitiveMeshComponent = GetPrimitiveMeshComponent()) {
      primitiveMeshComponent->SetQuadAnchorPoint(anchorPoint);
      primitiveMeshComponent->ApplyToMesh();
   }
}

void Sprite::SetSize(const Vector2& size) {
   if (auto* primitiveMeshComponent = GetPrimitiveMeshComponent()) {
      primitiveMeshComponent->SetQuadSize(size);
      primitiveMeshComponent->ApplyToMesh();
   }
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
   transformComponent->transform.SetRotationQuaternion(Vector3(0.0f, 0.0f, rotation).ToQuaternion().Normalize());
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
   return transformComponent->transform.GetActiveEuler().z;
}

Vector2 Sprite::GetSize() const {
   if (const auto* primitiveMeshComponent = GetPrimitiveMeshComponent()) {
      return primitiveMeshComponent->GetQuadSize();
   }
   return Vector2(1.0f, 1.0f);
}

Vector2 Sprite::GetAnchorPoint() const {
   if (const auto* primitiveMeshComponent = GetPrimitiveMeshComponent()) {
      return primitiveMeshComponent->GetQuadAnchorPoint();
   }
   return Vector2(0.0f, 0.0f);
}

bool Sprite::IsFlipX() const {
   if (const auto* primitiveMeshComponent = GetPrimitiveMeshComponent()) {
      return primitiveMeshComponent->IsFlipX();
   }
   return false;
}

bool Sprite::IsFlipY() const {
   if (const auto* primitiveMeshComponent = GetPrimitiveMeshComponent()) {
      return primitiveMeshComponent->IsFlipY();
   }
   return false;
}

void Sprite::SetFlipX(bool isFlip) {
   if (auto* primitiveMeshComponent = GetPrimitiveMeshComponent()) {
      primitiveMeshComponent->SetFlipX(isFlip);
      primitiveMeshComponent->ApplyToMesh();
   }
}

void Sprite::SetFlipY(bool isFlip) {
   if (auto* primitiveMeshComponent = GetPrimitiveMeshComponent()) {
      primitiveMeshComponent->SetFlipY(isFlip);
      primitiveMeshComponent->ApplyToMesh();
   }
}

void Sprite::SetTextureUV(const Vector2& leftTop, const Vector2& size) {
   if (auto* materialComponent = GetMaterialComponent()) {
      materialComponent->SetTextureUV(leftTop, size);
   }
}

void Sprite::SetTextureLeftTop(const Vector2& leftTop) {
   if (auto* materialComponent = GetMaterialComponent()) {
      materialComponent->SetTextureLeftTop(leftTop);
   }
}

void Sprite::SetTextureSize(const Vector2& size) {
   if (auto* materialComponent = GetMaterialComponent()) {
      materialComponent->SetTextureSize(size);
   }
}

Vector2 Sprite::GetTextureLeftTop() const {
   if (const auto* materialComponent = GetMaterialComponent()) {
      return materialComponent->GetTextureLeftTop();
   }
   return Vector2(0.0f, 0.0f);
}

Vector2 Sprite::GetTextureSize() const {
   if (const auto* materialComponent = GetMaterialComponent()) {
      return materialComponent->GetTextureSize();
   }
   return Vector2(0.0f, 0.0f);
}

Mesh* Sprite::GetMesh() const {
   if (const auto* primitiveMeshComponent = GetPrimitiveMeshComponent()) {
      return primitiveMeshComponent->GetMesh();
   }
   return nullptr;
}

TransformationMatrix* Sprite::GetTransformationMatrix() {
   auto* transformComponent = GetTransformComponent();
   if (!transformComponent) {
      return nullptr;
   }
   return transformComponent->EnsureTransformationMatrix();
}

void Sprite::Update(Camera* camera, Texture* texture) {
   auto* transformComponent = GetTransformComponent();
	if (!transformComponent || !camera) {
	   return;
	}

   auto* transformationMatrix = transformComponent->EnsureTransformationMatrix();
   if (!transformationMatrix) {
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
   transformationMatrix->GetTransformationMatrixData()->wVP = wVPMatrix;
   transformationMatrix->GetTransformationMatrixData()->world = worldMatrix;
   transformationMatrix->GetTransformationMatrixData()->worldInverseTranspose = worldMatrix.Inverse().Transpose();
}

void Sprite::UpdateMatrixForUI(Camera* camera, Texture* texture, AnchorPoint anchorPoint, uint32_t screenWidth, uint32_t screenHeight) {
   auto* transformComponent = GetTransformComponent();
	if (!transformComponent || !camera) {
	   return;
	}

   auto* transformationMatrix = transformComponent->EnsureTransformationMatrix();
   if (!transformationMatrix) {
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
   transformationMatrix->GetTransformationMatrixData()->wVP = wVPMatrix;
   transformationMatrix->GetTransformationMatrixData()->world = worldMatrix;
   transformationMatrix->GetTransformationMatrixData()->worldInverseTranspose = worldMatrix.Inverse().Transpose();
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
   if (auto* primitiveMeshComponent = GetPrimitiveMeshComponent()) {
      primitiveMeshComponent->ApplyToMesh();
   }
}

void Sprite::UpdateTextureCoordinates(Texture* texture) {
   auto* primitiveMeshComponent = GetPrimitiveMeshComponent();
   if (!primitiveMeshComponent) {
      return;
   }

   primitiveMeshComponent->ApplyTextureCoordinates(texture, GetTextureLeftTop(), GetTextureSize());
}

PrimitiveMeshComponent* Sprite::GetPrimitiveMeshComponent() {
   return GetComponent<PrimitiveMeshComponent>();
}

const PrimitiveMeshComponent* Sprite::GetPrimitiveMeshComponent() const {
   return GetComponent<PrimitiveMeshComponent>();
}

MaterialComponent* Sprite::GetMaterialComponent() {
   return GetComponent<MaterialComponent>();
}

const MaterialComponent* Sprite::GetMaterialComponent() const {
   return GetComponent<MaterialComponent>();
}

TransformComponent* Sprite::GetTransformComponent() {
   return GetComponent<TransformComponent>();
}

const TransformComponent* Sprite::GetTransformComponent() const {
   return GetComponent<TransformComponent>();
}

}
