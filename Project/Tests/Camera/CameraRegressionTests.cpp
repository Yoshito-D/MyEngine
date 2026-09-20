#include "PlayerRearFollowCamera.h"
#include "Scene/Camera/Core/CinemachineBrain.h"
#include "Scene/Camera/Camera.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {
using namespace GameEngine;

void Require(bool condition, const char* message) {
   if (!condition) throw std::runtime_error(message);
}

void UpdateCamera(App::PlayerRearFollowCamera& camera, CameraState& state, float deltaTime) {
   auto* owner = camera.GetOwnerCamera();
   Require(owner != nullptr, "Camera must be attached to a VirtualCamera");
   owner->SetState(state);
   owner->Update(deltaTime);
   state = owner->GetState();
}

void Append(std::vector<float>& trace, const Vector3& value) {
   trace.insert(trace.end(), { value.x, value.y, value.z });
}

void TestInterruptedBlend() {
   VirtualCamera first, second, third;
   auto Place = [](VirtualCamera& camera, float x, int priority) {
      CameraState state;
      state.transform.translation.x = x;
      camera.Initialize(state);
      camera.SetPriority(priority);
   };
   Place(first, 0.0f, 3);
   Place(second, 10.0f, 2);
   Place(third, 20.0f, 1);
   CinemachineBrain brain;
   brain.RegisterVirtualCamera(&first);
   brain.RegisterVirtualCamera(&second);
   brain.RegisterVirtualCamera(&third);
   brain.Update(0.0f);
   second.SetPriority(4);
   brain.Update(0.2f);
   const float interruptedPosition = brain.GetCurrentState().transform.translation.x;
   third.SetPriority(5);
   brain.Update(0.0f);
   Require(brain.GetCurrentState().transform.translation.x == interruptedPosition, "Retargeting changed the blend start");
   brain.Update(0.5f);
   for (int frame = 0; frame < 8; ++frame) {
      brain.Update(0.1f);
      Require(brain.GetCurrentState().transform.translation.x == 20.0f, "Obsolete blend resumed after retargeting");
   }
}

void TestUnregisterBlendTarget() {
   VirtualCamera remaining;
   auto removed = std::make_unique<VirtualCamera>();
   auto unrelated = std::make_unique<VirtualCamera>();
   CameraState target;
   target.transform.translation.x = 10.0f;
   removed->Initialize(target);
   remaining.SetPriority(1);
   removed->SetPriority(0);
   unrelated->SetPriority(-1);
   CinemachineBrain brain;
   brain.RegisterVirtualCamera(&remaining);
   brain.RegisterVirtualCamera(removed.get());
   brain.RegisterVirtualCamera(unrelated.get());
   brain.Update(0.0f);
   removed->SetPriority(2);
   brain.Update(0.1f);
   brain.UnregisterVirtualCamera(unrelated.get());
   unrelated.reset();
   brain.Update(0.1f);
   Require(brain.GetCurrentState().transform.translation.x < 10.0f, "Unrelated removal interrupted the blend");
   brain.UnregisterVirtualCamera(removed.get());
   removed.reset();
   brain.Update(0.1f);
   Require(brain.GetActiveCamera() == &remaining, "Removed camera is still selected");
   Require(brain.GetCurrentState().transform.translation.x == 0.0f, "Removed blend target is still being evaluated");
   brain.UnregisterVirtualCamera(&remaining);
   brain.Update(0.1f);
   Require(brain.GetActiveCamera() == nullptr, "Empty brain retained a camera");
}

void TestPositionConstraints() {
   for (float dt : { 0.0f, 1.0f / 120.0f, 1.0f / 60.0f, 1.0f / 30.0f }) {
      VirtualCamera cameraOwner;
      auto& camera = *cameraOwner.AddComponent<App::PlayerRearFollowCamera>();
      camera.Deserialize(nlohmann::json::object());
      CameraState state;
      state.transform.translation = { 0.0f, -15.0f, 0.0f };
      UpdateCamera(camera, state, dt);
      Require(state.transform.translation.y == -15.0f, "First frame no longer preserves saved placement");
      Vector3 previous = state.transform.translation;
      for (int frame = 0; frame < 180; ++frame) {
         UpdateCamera(camera, state, dt);
         const Vector3 offset = state.transform.translation;
         Require(offset.Length() >= camera.distance - 0.0001f, "Height correction violated minimum distance");
         const float angle = std::acos(std::clamp(previous.Normalize().Dot(offset.Normalize()), -1.0f, 1.0f));
         Require(angle <= camera.eyeDirectionMaxAngularSpeed * dt + 0.0005f, "Height correction bypassed angular limit");
         previous = offset;
      }
      if (dt > 0.0f) Require(state.transform.translation.y >= 0.49f, "Camera did not recover from invalid initial height");
   }
}

void TestCameraReset() {
   VirtualCamera cameraOwner;
   auto& camera = *cameraOwner.AddComponent<App::PlayerRearFollowCamera>();
   const auto settings = camera.Serialize();
   CameraState state;
   state.transform.translation = { 0.0f, 4.0f, -15.0f };
   camera.SetAirborne(true);
   camera.SetPlayerSpeed(50.0f);
   camera.SetLandingPrediction({ 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, {}, 0.1f);
   UpdateCamera(camera, state, 0.1f);
   camera.StartCameraMeasurement();
   camera.Deserialize(settings);
   VirtualCamera freshOwner;
   auto& fresh = *freshOwner.AddComponent<App::PlayerRearFollowCamera>();
   fresh.Deserialize(settings);
   CameraState actual, expected;
   actual.transform.translation = expected.transform.translation = { 0.0f, 4.0f, -15.0f };
   UpdateCamera(camera, actual, 1.0f / 60.0f);
   UpdateCamera(fresh, expected, 1.0f / 60.0f);
   Require(!camera.IsCameraMeasurementActive() && camera.GetCameraMeasurementSampleCount() == 0, "Reset retained measurement state");
   Require(actual.fov == expected.fov, "Reset retained speed effects");
   const auto actualView = actual.GetViewMatrix();
   const auto expectedView = expected.GetViewMatrix();
   Require(std::memcmp(&actualView, &expectedView, sizeof(actualView)) == 0, "Reset retained camera history");
}

void TestLandingPlaneConstraints() {
   App::RearCameraSettings settings;
   App::RearCameraInput input;
   input.isAirborne = true;
   input.predictedLandingUp = { 0.0f, 0.0f, 1.0f };
   App::RearCameraTransitionState transition;
   transition.currentPreLandingBlend = 1.0f;
   App::RearCameraDirectionState direction;
   App::RearCameraSpeedState speed;
   App::RearCameraAimState aim;
   App::RearCameraPositionSolver position;
   CameraState state;
   state.transform.translation = { 5.0f, 4.0f, -15.0f };
   const Vector3 up{ 0.0f, 1.0f, 0.0f };
   constexpr float dt = 1.0f / 60.0f;
   Vector3 previous = position.Update(input, settings, transition, direction, speed, aim, state, up, 0.0f, dt);
   for (int frame = 0; frame < 120; ++frame) {
      const Vector3 offset = position.Update(input, settings, transition, direction, speed, aim, state, up, 0.0f, dt);
      Require(offset.Length() >= settings.distance - 0.0001f, "Landing plane correction violated minimum distance");
      const float angle = std::acos(std::clamp(previous.Normalize().Dot(offset.Normalize()), -1.0f, 1.0f));
      Require(angle <= settings.eyeDirectionMaxAngularSpeed * dt + 0.0005f, "Landing plane correction bypassed angular limit");
      previous = offset;
   }
   Require(previous.Dot(input.predictedLandingUp) >= settings.preLandingMinOutwardHeight - 0.0001f,
      "Camera did not recover to the predicted landing plane");
}

void TestMeasurementLifecycle() {
   App::RearCameraSettings settings;
   App::RearCameraInput input;
   App::RearCameraTransition transition;
   App::RearCameraDirectionTracker direction;
   App::RearCameraPlanetGuide planet;
   App::RearCameraSpeedEffects speed;
   App::RearCameraAimSolver aim;
   App::RearCameraGravityUp gravity;
   const App::RearCameraFrameView frame{ input, transition, direction, planet, speed, gravity, aim };
   App::RearCameraMeasurementRecorder recorder;
   recorder.StartCameraMeasurement("regression", frame);
   recorder.RecordCameraMeasurementSample(0.01f, 42, settings, frame);
   recorder.RecordCameraMeasurementSample(0.01f, 42, settings, frame);
   Require(recorder.GetState().cameraMeasurementSamples.size() == 1, "One game frame was measured twice");
   recorder.RecordCameraMeasurementSample(0.02f, 43, settings, frame);
   Require(recorder.GetState().cameraMeasurementSamples.size() == 2, "Next game frame was not measured");
   Require(std::abs(recorder.GetState().cameraMeasurementSamples.back().timeSeconds - 0.03f) < 1e-6f,
      "Duplicate frame advanced measurement time");
   Require(recorder.StopCameraMeasurement(false, settings), "Stopping without export failed");
   recorder.RecordCameraMeasurementSample(0.01f, 44, settings, frame);
   Require(recorder.GetState().cameraMeasurementSamples.size() == 2, "Stopped recorder accepted a frame");
   recorder.StartCameraMeasurement("restart", frame);
   Require(recorder.GetState().cameraMeasurementSamples.empty(), "Restart retained earlier samples");
   recorder.ClearCameraMeasurement();
   Require(!recorder.GetState().cameraMeasurementActive, "Clear did not stop recording");
}

void TestComponentPipelineLifecycle() {
   VirtualCamera owner;
   owner.Deserialize({ { "components", nlohmann::json::array({
      { { "componentName", "PlayerRearFollowCamera" }, { "enabled", true }, { "data", { { "distance", 19.0f } } } }
   }) } });
   auto* camera = owner.GetComponent<App::PlayerRearFollowCamera>();
   Require(camera && camera->HasRequiredComponents(), "Legacy scene did not expand into pipeline components");
   const std::vector<std::string> expectedNames = {
      "PlayerRearFollowCamera", "RearCameraTransition", "RearCameraGravityUp", "RearCameraPlanetGuide",
      "RearCameraDirectionTracker", "RearCameraSpeedEffects", "RearCameraPositionSolver", "RearCameraAimSolver",
      "RearCameraMeasurementRecorder", "RearCameraDebugView"
   };
   Require(owner.GetComponents().size() == expectedNames.size(), "Rear camera component count differs");
   for (size_t index = 0; index < expectedNames.size(); ++index) {
      const auto& component = owner.GetComponents()[index];
      Require(component->GetComponentName() == expectedNames[index], "Rear camera pipeline order differs");
      Require(component->GetOwnerCamera() == &owner, "Pipeline component is not owned by VirtualCamera");
      Require(component->GetStage() == (index < 7 ? CinemachineStage::Body : CinemachineStage::Aim),
         "Rear camera pipeline stage differs");
      Require(owner.AddComponentByName(expectedNames[index]) == component.get(), "Factory duplicated a pipeline component");
   }

   // 個別の無効化がパイプライン呼び出しに反映され、保存・復元でも維持される。
   owner.GetComponent<App::RearCameraSpeedEffects>()->SetEnabled(false);
   auto serialized = owner.Serialize();
   std::reverse(serialized["components"].begin(), serialized["components"].end());
   VirtualCamera restored;
   restored.Deserialize(serialized);
   Require(restored.GetComponents().size() == expectedNames.size(), "Reversed scene order duplicated components");
   Require(restored.GetComponent<App::PlayerRearFollowCamera>()->distance == 19.0f, "Legacy settings were lost");
   Require(!restored.GetComponent<App::RearCameraSpeedEffects>()->IsEnabled(), "Disabled component flag was lost");
   for (size_t index = 0; index < expectedNames.size(); ++index) {
      Require(restored.GetComponents()[index]->GetComponentName() == expectedNames[index], "Deserialization changed execution order");
   }

   CameraState state;
   state.transform.translation = { 0.0f, 4.0f, -19.0f };
   state.fov = 0.7f;
   camera->SetPlayerSpeed(50.0f);
   UpdateCamera(*camera, state, 0.1f);
   Require(state.fov == 0.7f, "Disabled speed component still updated FOV");
   owner.GetComponent<App::RearCameraPositionSolver>()->SetEnabled(false);
   const Vector3 positionBeforeDisable = state.transform.translation;
   camera->SetPivotTarget({ 20.0f, 10.0f, 30.0f });
   UpdateCamera(*camera, state, 0.1f);
   Require((state.transform.translation - positionBeforeDisable).Length() == 0.0f,
      "Disabled position component still moved the camera");

   camera->SetEnabled(false);
   const auto disabledView = state.GetViewMatrix();
   camera->SetPivotTarget({ -100.0f, 10.0f, 0.0f });
   UpdateCamera(*camera, state, 0.1f);
   const auto afterDisabledView = state.GetViewMatrix();
   Require(std::memcmp(&disabledView, &afterDisabledView, sizeof(disabledView)) == 0, "Disabled root still executed the pipeline");
   camera->SetEnabled(true);

   owner.RemoveComponent(owner.GetComponent<App::RearCameraGravityUp>());
   Require(!camera->HasRequiredComponents() && !camera->GetFrameView(), "Missing dependency was not detected");
   UpdateCamera(*camera, state, 0.1f);
   const auto missingDependencyView = state.GetViewMatrix();
   Require(std::memcmp(&disabledView, &missingDependencyView, sizeof(disabledView)) == 0, "Incomplete pipeline partially changed output");
   owner.AddComponentByName("RearCameraGravityUp");
   Require(camera->HasRequiredComponents(), "Removed component could not be recreated");

   owner.RemoveComponent(owner.GetComponent<App::RearCameraMeasurementRecorder>());
   camera->StartCameraMeasurement();
   Require(!camera->IsCameraMeasurementActive(), "Missing recorder remained active");
   UpdateCamera(*camera, state, 0.1f);
   owner.RemoveComponent(camera);
   owner.Update(0.1f);
   owner.AddComponentByName("PlayerRearFollowCamera");
   Require(owner.GetComponents().size() == expectedNames.size(), "Recreating the root duplicated existing components");
}

std::vector<float> RunCameraTrace() {
   std::vector<nlohmann::json> configurations = { nlohmann::json::object() };
   for (const char* filename : { "GameTest", "Tutorial" }) {
      std::ifstream sceneFile(std::string("Resources/game/scenes/") + filename + ".json");
      Require(sceneFile.good(), "Scene fixture is missing");
      const auto scene = nlohmann::json::parse(sceneFile);
      for (const auto& camera : scene.at("cameras").at("virtualCameras")) {
         for (const auto& component : camera.at("components")) {
            if (component.at("componentName") == "PlayerRearFollowCamera") {
               configurations.push_back(component.at("data"));
            }
         }
      }
   }
   configurations.push_back({ { "enablePreLandingCamera", false }, { "enableAirbornePlanetDirectionGuide", false } });
   configurations.push_back({ { "takeoffFramingBlendSeconds", 0 }, { "landingFramingBlendSeconds", 0 },
      { "jumpPlanetDirectionDelaySeconds", 0 }, { "jumpPlanetDirectionRestoreSeconds", 0 } });
   configurations.push_back({ { "gravityUpLerpSpeed", 0 }, { "rearLerpSpeed", 0 },
      { "eyeDirectionMaxAngularSpeed", 0 }, { "rotationLerpSpeed", 0 } });
   configurations.push_back({ { "accelToFovKick", 0.0025f }, { "turboFovKickMax", 0.06f },
      { "accelToDistanceKick", 0.04f }, { "turboDistanceKickMax", 2.0f },
      { "jumpPlanetDirectionRampSeconds", 0.8f } });

   std::vector<float> trace;
   for (const auto& settings : configurations) {
      for (int timing = 0; timing < 4; ++timing) {
         VirtualCamera cameraOwner;
         auto& camera = *cameraOwner.AddComponent<App::PlayerRearFollowCamera>();
         camera.Deserialize(settings);
         const auto savedSettings = camera.Serialize();
         VirtualCamera restoredOwner;
         auto& restored = *restoredOwner.AddComponent<App::PlayerRearFollowCamera>();
         restored.Deserialize(savedSettings);
         Require(restored.Serialize() == savedSettings, "Camera settings do not round-trip");
         CameraState state;
         state.transform.translation = { 0.0f, 4.0f, -15.0f };
         for (int frame = 0; frame < 480; ++frame) {
            // Exercise short hops, interrupted landing release, antipodal Up, low speed and prediction loss.
            const float phase = static_cast<float>(frame) * 0.02f;
            const bool airborne = (frame % 160 >= 30 && frame % 160 < 115) || frame % 160 == 118;
            camera.SetPivotTarget({ phase * 2.0f, std::sin(phase), phase });
            camera.SetPlanetCenter({ 0.0f, -50.0f, 0.0f });
            camera.SetGravityUp(frame < 240 ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 0.0f, -1.0f, 0.0f });
            camera.SetFollowForward({ std::sin(phase), 0.0f, std::cos(phase) });
            camera.SetAirborneMoveForward({ 1.0f, 0.0f, 0.0f });
            camera.SetAirborne(airborne);
            camera.SetPlayerVelocity(frame % 71 == 0 ? Vector3{} : Vector3{ std::sin(phase) * 5.0f, std::cos(phase) * 20.0f, 0.0f });
            camera.SetPlayerSpeed(frame % 90 < 45 ? 13.0f : 30.0f);
            camera.SetAutoSpeed(13.0f);
            camera.ClearLandingPrediction();
            if (airborne && frame % 160 >= 65 && frame % 11 != 0) {
               camera.SetLandingPrediction({ 0.0f, 0.8f, 0.6f }, { 0.0f, 0.6f, -0.8f },
                  { phase * 2.0f, -1.0f, phase + 5.0f }, static_cast<float>(115 - frame % 160) / 60.0f);
            }
            float dt = timing == 0 ? 1.0f / 30.0f : timing == 1 ? 1.0f / 60.0f : 1.0f / 120.0f;
            if (timing == 3) dt = frame % 47 == 0 ? 0.3f : frame % 13 == 0 ? 0.0f : 1.0f / 60.0f;
            UpdateCamera(camera, state, dt);
            Append(trace, state.transform.translation);
            const auto rotation = state.transform.GetActiveQuaternion();
            trace.insert(trace.end(), { rotation.x, rotation.y, rotation.z, rotation.w,
               state.fov, state.nearClip, state.farClip, state.hasViewMatrixOverride ? 1.0f : 0.0f });
            for (const auto& row : state.GetViewMatrix().m) trace.insert(trace.end(), std::begin(row), std::end(row));
            Append(trace, camera.GetCameraRight());
            Append(trace, camera.GetCameraUp());
            Append(trace, camera.GetCameraForward());
            Require(std::abs(camera.GetCameraRight().Dot(camera.GetCameraUp())) < 0.0001f, "Camera axes are not orthogonal");
            Require(std::abs(camera.GetCameraForward().Length() - 1.0f) < 0.0001f, "Camera forward is not normalized");
         }
      }
   }
   for (float value : trace) Require(std::isfinite(value), "Camera output is not finite");
   return trace;
}
}

int main(int argc, char** argv) {
   try {
      TestInterruptedBlend();
      TestUnregisterBlendTarget();
      TestPositionConstraints();
      TestCameraReset();
      TestLandingPlaneConstraints();
      TestMeasurementLifecycle();
      TestComponentPipelineLifecycle();
      const auto trace = RunCameraTrace();
      if (argc == 3 && std::string(argv[1]) == "--trace") {
         std::ofstream output(argv[2], std::ios::binary);
         output.write(reinterpret_cast<const char*>(trace.data()), static_cast<std::streamsize>(trace.size() * sizeof(float)));
         Require(output.good(), "Could not save baseline trace");
      } else if (argc == 3 && std::string(argv[1]) == "--compare") {
         std::ifstream input(argv[2], std::ios::binary);
         const std::vector<char> baseline{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
         Require(baseline.size() == trace.size() * sizeof(float), "Baseline trace length differs");
         Require(std::memcmp(baseline.data(), trace.data(), baseline.size()) == 0, "Refactored camera differs from baseline");
      }
      std::cout << "PASS: " << trace.size() / 36 << " camera frames (" << trace.size() << " float outputs)\n";
      return 0;
   } catch (const std::exception& error) {
      std::cerr << "FAIL: " << error.what() << '\n';
      return 1;
   }
}
