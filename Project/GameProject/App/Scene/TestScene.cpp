#include "TestScene.h"
#include "Framework/EngineContext.h"
#include "ObjectEdit.h"
#include "MathUtils.h"
using namespace GameEngine;

void TestScene::Initialize() {
   BaseScene::Initialize();

   EngineContext::LoadModel("resources/models/planet", "planet.obj");
   EngineContext::LoadModel("resources/models/plane", "plane.obj");
   EngineContext::LoadModel("resources/models/cube", "cube.gltf");
   EngineContext::CreateMaterial("spherePhongMaterial", 0xffffffff, 3);
   EngineContext::CreateMaterial("sphereBlinnPhongMaterial", 0xffffffff, 4);
   EngineContext::CreateMaterial("planeMaterial");
   EngineContext::CreateMaterial("cubeGltfMaterial");
   EngineContext::LoadTexture("resources/white1x1.png", "white1x1");
   EngineContext::LoadTexture("resources/monsterBall.png", "monsterBall");
   EngineContext::LoadTexture("resources/uvChecker.png", "uvChecker");

   auto spherePhongMaterial = EngineContext::GetMaterial("spherePhongMaterial");
   auto sphereBlinnPhongMaterial = EngineContext::GetMaterial("sphereBlinnPhongMaterial");
   auto planeMaterial = EngineContext::GetMaterial("planeMaterial");
   auto cubeGltfMaterial = EngineContext::GetMaterial("cubeGltfMaterial");
   auto sphereModelAsset = EngineContext::GetModel("planet.obj");
   auto planeModelAsset = EngineContext::GetModel("plane.obj");
   auto cubeGltfModelAsset = EngineContext::GetModel("cube.gltf");

   testSpherePhongModel_ = std::make_unique<Model>();
   testSpherePhongModel_->Create(sphereModelAsset, spherePhongMaterial);
   testSpherePhongModel_->SetPosition(Vector3(-4.0f, 1.0f, 0.0f));

   testSphereBlinnPhongModel_ = std::make_unique<Model>();
   testSphereBlinnPhongModel_->Create(sphereModelAsset, sphereBlinnPhongMaterial);
   testSphereBlinnPhongModel_->SetPosition(Vector3(4.0f, 1.0f, 0.0f));

   testPlaneModel_ = std::make_unique<Model>();
   testPlaneModel_->Create(planeModelAsset, planeMaterial);
   testPlaneModel_->SetRotation(Vector3(ToRadians(90.0f), 0.0f, 0.0f));
   testPlaneModel_->SetScale(Vector3(20.0f, 20.0f, 20.0f));
   testPlaneModel_->SetPosition(Vector3(0.0f, -1.0f, 0.0f));

   testCubeGltfModel_ = std::make_unique<Model>();
   testCubeGltfModel_->Create(cubeGltfModelAsset, cubeGltfMaterial);
   testCubeGltfModel_->SetPosition(Vector3(0.0f, 1.0f, 3.0f));

#ifdef USE_IMGUI
   isDebugCameraActive_ = true;
#endif 

   auto directionalLight = EngineContext::GetDirectionalLight("MainDirectionalLight");
   if (directionalLight) {
	  directionalLight->GetDirectionalLightData()->intensity = 0.5f;
	  directionalLight->GetDirectionalLightData()->direction = Vector3(-1.0f, -1.0f, 1.0f).Normalize();
   }

   auto pointLight = EngineContext::GetPointLight("MainPointLight");
   if (pointLight) {
	  pointLight->GetPointLightData()->intensity = 5.0f;
	  pointLight->GetPointLightData()->position = Vector3(-6.0f, 0.0f, -5.0f);
	  pointLight->GetPointLightData()->decay = 2.0f;
	  pointLight->GetPointLightData()->color = Vector4(0.0f, 1.0f, 0.0f, 1.0f); // 緑色
   }

   EngineContext::CreatePointLight("SecondPointLight", 0x00ffffff, Vector3(-4.0f, 0.0f, -5.0f), 5.0f);
   auto secondPointLight = EngineContext::GetPointLight("SecondPointLight");
   if (secondPointLight) {
	  secondPointLight->GetPointLightData()->intensity = 5.0f;
	  secondPointLight->GetPointLightData()->decay = 2.0f;
   }

   auto spotLight = EngineContext::GetSpotLight("MainSpotLight");
   if (spotLight) {
	  spotLight->GetSpotLightData()->intensity = 1.0f;
	  spotLight->GetSpotLightData()->position = Vector3(-3.0f, 1.0f, -5.0f);
	  spotLight->GetSpotLightData()->direction = Vector3(0.8f, -0.8f, 0.0f).Normalize();
	  spotLight->GetSpotLightData()->distance = 10.0f;
	  spotLight->GetSpotLightData()->decay = 2.0f;
	  spotLight->GetSpotLightData()->cosAngle = std::cos(ToRadians(30.0f));
	  spotLight->GetSpotLightData()->cosFalloffStart = std::cos(ToRadians(20.0f));
	  spotLight->GetSpotLightData()->color = Vector4(1.0f, 1.0f, 0.0f, 1.0f); // 黄色
   }

   EngineContext::CreateSpotLight("SecondSpotLight", 0x00ffffff, Vector3(3.0f, 1.0f, -5.0f), 1.0f, Vector3(-0.8f, -0.8f, 0.0f), 10.0f, 2.0f, std::cos(ToRadians(30.0f)), std::cos(ToRadians(20.0f)));

   auto areaLight = EngineContext::GetAreaLight("MainAreaLight");
   if (areaLight) {
	  areaLight->GetAreaLightData()->intensity = 0.5f;
	  areaLight->GetAreaLightData()->position = Vector3(8.0f, 0.0f, -5.0f);
	  areaLight->GetAreaLightData()->color = Vector4(0.0f, 0.0f, 1.0f, 1.0f); // 青色
   }

   EngineContext::CreateAreaLight("SecondAreaLight", 
	  Vector3(6.0f, 0.0f, -5.0f),           // position (中心座標)
	  Vector3(0.0f, -1.0f, 0.0f),          // normal (照射方向: 下向き)
	  Vector3(1.0f, 0.0f, 0.0f),           // tangent (右方向)
	  Vector2(2.0f, 2.0f),                 // size (幅と高さ)
	  Vector3(1.0f, 0.0f, 0.0f),           // color (白)
	  1.0f                                 // intensity (強度)
   );

}

void TestScene::Update() {
   BaseScene::Update();

#ifdef USE_IMGUI
   ObjectEdit::ModelEdit("MonsterBall", testSpherePhongModel_.get());
   ObjectEdit::ModelEdit("UVCheckerSphere", testSphereBlinnPhongModel_.get());
   ObjectEdit::ModelEdit("Plane", testPlaneModel_.get());
   ObjectEdit::ModelEdit("CubeGltf", testCubeGltfModel_.get());
   ImGui::ShowDemoWindow();
#endif 
   // ライトのデバッグ表示を呼び出す
   EngineContext::DebugDrawLights();
}

void TestScene::Draw() {
   // モデルの描画
   auto white = EngineContext::GetTexture("white1x1");
   auto monsterBallTex = EngineContext::GetTexture("monsterBall");
   auto uvCheckerTex = EngineContext::GetTexture("uvChecker");

   EngineContext::Draw(testSpherePhongModel_.get(), monsterBallTex);
   EngineContext::Draw(testSphereBlinnPhongModel_.get(), uvCheckerTex);
   EngineContext::Draw(testPlaneModel_.get(), white);
   EngineContext::Draw(testCubeGltfModel_.get(), uvCheckerTex);

   // 基底クラスの描画（フェードなど）
   BaseScene::Draw();
}
