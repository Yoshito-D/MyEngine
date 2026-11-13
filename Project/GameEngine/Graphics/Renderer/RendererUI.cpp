#include "GameEngine/pch.h"
#include "GameEngine/Graphics/Renderer/Renderer.h"
#include "GameEngine/UI/UISystem.h"
#include "GameEngine/UI/UIComponents.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Input/InputActionService.h"
#include "GameEngine/Scene/Scene.h"
#include "GameEngine/Assets/AssetManager.h"
#include "GameEngine/Assets/Texture/TextureManager.h"
#include "GameEngine/Assets/Font/FontManager.h"
#include "GameEngine/Graphics/Resources/Material.h"
#include "GameEngine/Object/Component/UI/UITextComponent.h"
#include "GameEngine/Object/Component/Rendering/MaterialComponent.h"
#include <algorithm>

namespace GameEngine {
UIInputFrame Renderer::ReadUIInput(Input& input, InputActionService& actions) {
   UIInputFrame frame;
   frame.gamepadConnected = input.GetConnectedGamePadCount() != 0;
   POINT pointer{};
   GetCursorPos(&pointer);
   bool standalone = true;
#ifdef USE_IMGUI
   standalone = !imGuiManager_ || !imGuiManager_->IsDockSpaceVisible();
#endif
   if (standalone) {
      RECT rectangle{};
      const auto hwnd = GetActiveWindow();
      GetClientRect(hwnd, &rectangle);
      POINT origin{};
      ClientToScreen(hwnd, &origin);
      gameUIOrigin_ = {static_cast<float>(origin.x), static_cast<float>(origin.y)};
      gameUISize_ = {static_cast<float>(rectangle.right), static_cast<float>(rectangle.bottom)};
      gameUIActive_ = hwnd && GetForegroundWindow() == hwnd;
   }
#ifdef USE_IMGUI
   // Viewports無効時のImGui座標はクライアント座標であり、デスクトップ座標を直接引かない。
   if (!standalone && !imGuiManager_->IsMultiViewportEnabled()) {
      const auto client = input.GetMouseScreenPosition();
      pointer = {static_cast<LONG>(client.x), static_cast<LONG>(client.y)};
   }
#endif
   DWORD foregroundProcess = 0;
   GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
   frame.active =
      foregroundProcess == GetCurrentProcessId() && gameUIActive_ && gameUISize_.x > 0.0f && gameUISize_.y > 0.0f;
   const Vector2 local{
      static_cast<float>(pointer.x) - gameUIOrigin_.x, static_cast<float>(pointer.y) - gameUIOrigin_.y};
   frame.pointerInside =
      frame.active && local.x >= 0.0f && local.y >= 0.0f && local.x < gameUISize_.x && local.y < gameUISize_.y;
   if (frame.active) {
      frame.pointer = {local.x * uiViewportSize_.x / gameUISize_.x, local.y * uiViewportSize_.y / gameUISize_.y};
   }
   frame.pointerMoved = frame.pointer.x != previousUIPointer_.x || frame.pointer.y != previousUIPointer_.y;
   previousUIPointer_ = frame.pointer;
   frame.pointerDown = input.IsMousePressed(MouseButton::Left);
   frame.pointerPressed = input.IsMouseTriggered(MouseButton::Left);
   frame.pointerReleased = input.IsMouseReleased(MouseButton::Left);
   frame.confirm = actions.GetActionState("UI", "UI.Confirm").triggered;
   frame.cancel = actions.GetActionState("UI", "UI.Cancel").triggered;
   frame.vertical = actions.GetActionState("UI", "UI.Navigate").value.x;
   frame.horizontal = actions.GetActionState("UI", "UI.NavigateHorizontal").value.x;
   const bool tab = input.IsKeyTriggered(KeyCode::Tab);
   const bool shift = input.IsKeyPressed(KeyCode::LeftShift) || input.IsKeyPressed(KeyCode::RightShift);
   frame.next = tab && !shift;
   frame.previous = tab && shift;
   return frame;
}

void Renderer::DrawManagedUI() {
   auto* scene = EngineContext::GetCurrentScene();
   auto* textures = assetManager_ ? assetManager_->GetTextureManager() : nullptr;
   auto* fonts = assetManager_ ? assetManager_->GetFontManager() : nullptr;
   if (!scene || !textures || !fonts || !textRenderer_) return;
   auto& world = scene->GetSceneWorld();
   auto& ui = world.GetUI();
   bool editing = false;
#ifdef USE_IMGUI
   editing = EngineContext::IsPlayModeEdit();
#endif
   const auto layout = ui.BuildLayout(uiViewportSize_, editing);
   for (const auto& item : layout) {
      if (!item.visible) {
         continue;
      }
      auto* object = world.FindObjectById(item.objectId);
      if (!object) {
         continue;
      }
      if (auto* text = object->GetComponent<UITextComponent>(); text && text->IsEnabled()) {
         auto style = text->GetStyle();
         style.pivot = {};
         style.color.w *= item.opacity;
         const auto commands = textRenderer_->QueueText(text->GetLayout(*fonts), style, Transform{},
            text->GetVisibleGlyphCount(), Window::kUiReferenceWidth, Window::kUiReferenceHeight, uiViewportSize_.x,
            uiViewportSize_.y, &item.matrix);
         for (const auto& command : commands) {
            SubmitDrawCommand(DrawCommand::CreateText(command, RenderPass::Overlay));
         }
         continue;
      }
      auto* widget = GetUIWidget(*object);
      auto* sprite = dynamic_cast<Sprite*>(object);
      if (!sprite && !widget) {
         continue;
      }
      auto* texture = textures->GetTexture("white1x1");
      Vector4 color{1.0f, 1.0f, 1.0f, item.opacity};
      Matrix4x4 uvTransform = MakeIdentity4x4();
      if (const auto* material = object->GetComponent<MaterialComponent>()) {
         if (material->GetMaterial()) {
            color = material->GetMaterial()->GetColor();
            color.w *= item.opacity;
            uvTransform = material->GetMaterial()->GetUVTransform();
         }
         if (!material->GetTextureNames().empty()) {
            if (auto* requested = textures->GetTexture(material->GetTextureNames().front());
               requested && !requested->GetMetadata().IsCubemap()) {
               texture = requested;
            }
         }
      }
      if (!texture) {
         continue;
      }
      const auto visual = widget ? widget->GetVisual(!item.interactable && !editing) : UIWidgetVisual{};
      if (widget) {
         const auto& tint = visual.color;
         color = {color.x * tint[0], color.y * tint[1], color.z * tint[2], color.w * tint[3]};
      }
      Vector2 uvMin{};
      Vector2 uvMax{1.0f, 1.0f};
      if (sprite) {
         const auto origin = sprite->GetTextureLeftTop();
         const auto size = sprite->GetTextureSize();
         const auto& metadata = texture->GetMetadata();
         uvMin = {origin.x / static_cast<float>(metadata.width), origin.y / static_cast<float>(metadata.height)};
         if (size.x > 0.0f && size.y > 0.0f) {
            uvMax = {uvMin.x + size.x / static_cast<float>(metadata.width),
               uvMin.y + size.y / static_cast<float>(metadata.height)};
         }
         if (sprite->IsFlipX()) {
            std::swap(uvMin.x, uvMax.x);
         }
         if (sprite->IsFlipY()) {
            std::swap(uvMin.y, uvMax.y);
         }
      }
      const auto queue = [&](Vector2 size, Vector4 tint) {
         const auto command = textRenderer_->QueueQuad(
            size, item.matrix, uvMin, uvMax, tint, texture->GetTextureSrvHandleGPU(), uiViewportSize_, uvTransform);
         SubmitDrawCommand(DrawCommand::CreateText(command, RenderPass::Overlay));
      };
      queue(item.size, color);
      // 操作部品の値表示は専用の描画色だけを変更し、保存値や共有マテリアルへ書き戻さない。
      if (widget && visual.fillAmount > 0.0f) {
         const auto& tint = visual.fillColor;
         queue({item.size.x * std::clamp(visual.fillAmount, 0.0f, 1.0f), item.size.y},
            {tint[0], tint[1], tint[2], item.opacity * tint[3]});
      }
   }
}
}
