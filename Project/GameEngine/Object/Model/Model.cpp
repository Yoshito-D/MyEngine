#include "pch.h"
#include "Model.h"
#include "ResourceHelper.h"
#include "Scene/Camera/Camera.h"

namespace {
Logger& log_ = Logger::GetInstance();
}

namespace GameEngine {

void Model::Create(ModelAsset* modelAsset, Material* material) {
   if (modelAsset) {
	  modelAsset_ = modelAsset;
   }

   CreateTransformationMatrix();

   if (material) {
	  materials_.clear();
	  materials_.push_back(material);
   }

   transform_.scale = Vector3(1.0f, 1.0f, 1.0f);
}

void Model::UpdateMatrix(Camera* camera) {
   Matrix4x4 worldMatrix;

   // Quaternionを使用するかチェック
   if (useQuaternion_) {
	  // Quaternionから回転行列を生成
	  Matrix4x4 scaleMatrix = MakeScaleMatrix(transform_.scale);
	  Matrix4x4 rotateMatrix = MakeRotateMatrix(quaternion_);
	  Matrix4x4 translateMatrix = MakeTranslateMatrix(transform_.translation);
	  worldMatrix = scaleMatrix * rotateMatrix * translateMatrix;
   } else {
	  // 通常のEuler角からアフィン変換行列を生成
	  worldMatrix = MakeAffineMatrix(transform_);
   }

   // modelAssetのrootNode.localMatrixを掛ける
   if (modelAsset_) {
	  worldMatrix = modelAsset_->GetRootNode().localMatrix * worldMatrix;
   }

   if (isUsingParentMatrix_) {
	  Matrix4x4 wVPMatrix = worldMatrix * parentMatrix_ * camera->GetViewProjectionMatrix();
	  transformationMatrix_->GetTransformationMatrixData()->world = worldMatrix * parentMatrix_;
	  transformationMatrix_->GetTransformationMatrixData()->wVP = wVPMatrix;
	  transformationMatrix_->GetTransformationMatrixData()->worldInverseTranspose = (worldMatrix * parentMatrix_).Inverse().Transpose();
   } else {
	  Matrix4x4 wVPMatrix = worldMatrix * camera->GetViewProjectionMatrix();
	  transformationMatrix_->GetTransformationMatrixData()->world = worldMatrix;
	  transformationMatrix_->GetTransformationMatrixData()->wVP = wVPMatrix;
	  transformationMatrix_->GetTransformationMatrixData()->worldInverseTranspose = worldMatrix.Inverse().Transpose();
   }
}
}