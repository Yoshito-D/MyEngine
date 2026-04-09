#include "TestScene.h"
#include "Framework/EngineContext.h"
#include "Component/AnimationComponent.h"
#include "ObjectEdit.h"
#include "MathUtils.h"

using namespace GameEngine;

void TestScene::Initialize() {
   BaseScene::Initialize();

   EngineContext::LoadModel("resources/models/planet", "planet.obj");
   EngineContext::LoadModel("resources/models/plane", "plane.obj");
   EngineContext::LoadModel("resources/models/cube", "AnimatedCube.gltf");
   EngineContext::LoadAnimation("resources/models/cube", "AnimatedCube.gltf");
   EngineContext::LoadModel("resources/models/simpleSkin", "simpleSkin.gltf");
   EngineContext::LoadAnimation("resources/models/simpleSkin", "simpleSkin.gltf");
   EngineContext::LoadModel("resources/models/human", "sneakWalk.gltf");
   EngineContext::LoadAnimation("resources/models/human", "sneakWalk.gltf");
   EngineContext::CreateMaterial("spherePhongMaterial", 0xffffffff, 3);
   EngineContext::CreateMaterial("sphereBlinnPhongMaterial", 0xffffffff, 4);
   EngineContext::CreateMaterial("planeMaterial");
   EngineContext::CreateMaterial("cubeGltfMaterial");
   EngineContext::CreateMaterial("skeletonDebugMaterial");
   EngineContext::LoadTexture("resources/textures/white1x1.png", "white1x1");
   EngineContext::LoadTexture("resources/textures/monsterBall.png", "monsterBall");
   EngineContext::LoadTexture("resources/textures/uvChecker.png", "uvChecker");

   auto spherePhongMaterial = EngineContext::GetMaterial("spherePhongMaterial");
   auto sphereBlinnPhongMaterial = EngineContext::GetMaterial("sphereBlinnPhongMaterial");
   auto planeMaterial = EngineContext::GetMaterial("planeMaterial");
   auto cubeGltfMaterial = EngineContext::GetMaterial("cubeGltfMaterial");
   auto skeletonDebugMaterial = EngineContext::GetMaterial("skeletonDebugMaterial");
   auto sphereModelAsset = EngineContext::GetModel("planet.obj");
   auto planeModelAsset = EngineContext::GetModel("plane.obj");
   auto cubeGltfModelAsset = EngineContext::GetModel("AnimatedCube.gltf");
   auto skeletonModelAsset = EngineContext::GetModel("simpleSkin.gltf");
   auto sneakWalkModelAsset = EngineContext::GetModel("sneakWalk.gltf");

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
   if (auto* animationComponent = testCubeGltfModel_->AddComponent<AnimationComponent>()) {
	  animationComponent->animationName = "AnimatedCube.gltf";
	  animationComponent->playing = true;
	  animationComponent->loop = true;
   }

   testSkeletonModel_ = std::make_unique<Model>();
   testSkeletonModel_->Create(skeletonModelAsset, skeletonDebugMaterial);
   testSkeletonModel_->SetPosition(Vector3(0.0f, -1.0f, 0.0f));
   if (auto* animationComponent = testSkeletonModel_->AddComponent<AnimationComponent>()) {
	  animationComponent->animationName = "simpleSkin.gltf";
	  animationComponent->playing = true;
	  animationComponent->loop = true;
      animationComponent->applyTranslation = false;
	  animationComponent->applyRotation = false;
	  animationComponent->applyScale = false;
   }

   testSneakWalkModel_ = std::make_unique<Model>();
   testSneakWalkModel_->Create(sneakWalkModelAsset, skeletonDebugMaterial);
   testSneakWalkModel_->SetPosition(Vector3(2.5f, -1.0f, 0.0f));
   testSneakWalkModel_->SetScale(Vector3(1.0f, 1.0f, 1.0f));
   if (auto* animationComponent = testSneakWalkModel_->AddComponent<AnimationComponent>()) {
	  animationComponent->animationName = "sneakWalk.gltf";
	  animationComponent->playing = true;
	  animationComponent->loop = true;
      animationComponent->applyTranslation = false;
	  animationComponent->applyRotation = false;
	  animationComponent->applyScale = false;
	  animationComponent->useSkinning = true;
   }

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
	  pointLight->GetPointLightData()->color = Vector4(0.0f, 1.0f, 0.0f, 1.0f);
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
	  spotLight->GetSpotLightData()->color = Vector4(1.0f, 1.0f, 0.0f, 1.0f);
   }

   EngineContext::CreateSpotLight(
	  "SecondSpotLight",
	  0x00ffffff,
	  Vector3(3.0f, 1.0f, -5.0f),
	  1.0f,
	  Vector3(-0.8f, -0.8f, 0.0f),
	  10.0f,
	  2.0f,
	  std::cos(ToRadians(30.0f)),
	  std::cos(ToRadians(20.0f))
   );

   auto areaLight = EngineContext::GetAreaLight("MainAreaLight");
   if (areaLight) {
	  areaLight->GetAreaLightData()->intensity = 0.5f;
	  areaLight->GetAreaLightData()->position = Vector3(8.0f, 0.0f, -5.0f);
	  areaLight->GetAreaLightData()->color = Vector4(0.0f, 0.0f, 1.0f, 1.0f);
   }

   EngineContext::CreateAreaLight(
	  "SecondAreaLight",
	  Vector3(6.0f, 0.0f, -5.0f),
	  Vector3(0.0f, -1.0f, 0.0f),
	  Vector3(1.0f, 0.0f, 0.0f),
	  Vector2(2.0f, 2.0f),
	  Vector3(1.0f, 0.0f, 0.0f),
	  1.0f
   );
}

void TestScene::Update() {
   BaseScene::Update();

   EngineContext::DebugDrawLights();

   if (testSkeletonModel_) {
	  EngineContext::DrawSkeleton(
		 testSkeletonModel_.get(),
		 0.025f,
		 Vector4(1.0f, 0.2f, 0.2f, 1.0f),
		 Vector4(0.2f, 1.0f, 1.0f, 1.0f),
        false
	  );
   }

   if (testSneakWalkModel_) {
	  EngineContext::DrawSkeleton(
		 testSneakWalkModel_.get(),
		 0.02f,
		 Vector4(1.0f, 0.8f, 0.1f, 1.0f),
		 Vector4(0.4f, 1.0f, 0.2f, 1.0f),
		 false
	  );
   }
}

void TestScene::Draw() {
  /* auto white = EngineContext::GetTexture("white1x1");
   auto monsterBallTex = EngineContext::GetTexture("monsterBall");
   auto uvCheckerTex = EngineContext::GetTexture("uvChecker");

   EngineContext::Draw(testSpherePhongModel_.get(), monsterBallTex);
   EngineContext::Draw(testSphereBlinnPhongModel_.get(), uvCheckerTex);
   EngineContext::Draw(testPlaneModel_.get(), white);
   EngineContext::Draw(testCubeGltfModel_.get(), uvCheckerTex);*/

   BaseScene::Draw();
}
