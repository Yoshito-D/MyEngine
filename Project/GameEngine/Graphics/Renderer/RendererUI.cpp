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
#include "GameEngine/Graphics/Renderer/Pass/SphereMapRenderer.h"
#include "GameEngine/Object/Component/UI/UITextComponent.h"
#include "GameEngine/Object/Component/Rendering/MaterialComponent.h"
#include <algorithm>

namespace GameEngine {

void Renderer::QueueSphereMap() {
   const auto* frame = sphereMapRenderer_ ? sphereMapRenderer_->GetFrame() : nullptr;

   if (!frame) return;

   const auto texture = sphereMapRenderer_->Render(*device_, *psoManager_, offscreenRenderTarget_->GetFormat());
   // 球体マップの描画先から、描画途中のシーンの色・深度へ戻す。
   offscreenRenderTarget_->BindPreservingContents(true);
   InvalidatePipelineBinding();

   // 合成時だけ同じ倍率を使い、投影と画面上の円形背景を揃える。
   const auto size = Vector2{static_cast<float>(frame->width), static_cast<float>(frame->height)} * frame->displayScale;
   const Vector2 bottomRight{(uiViewportSize_.x + Window::kUiReferenceWidth) * 0.5f,
      (uiViewportSize_.y + Window::kUiReferenceHeight) * 0.5f};
   const Vector2 origin{bottomRight.x - frame->margin - size.x, bottomRight.y - frame->margin - size.y};
   const auto identity = MakeIdentity4x4();
   const float radius = frame->apertureRadius * frame->displayScale;
   const auto center = origin + frame->projectionCenter * frame->displayScale;
   const auto background = textRenderer_->QueueQuad({2.f * radius, 2.f * radius},
      MakeTranslateMatrix({center.x - radius, center.y - radius, 0.f}), {}, {1.f, 1.f},
      frame->backgroundColor, texture, uiViewportSize_, identity, TextRenderer::QuadFill::SolidCircle);
   SubmitDrawCommand(DrawCommand(RenderPass::PostProcess, background));

   // 背景だけを通常合成し、その上の線と光点は発光を加算する。
   const auto matrix = MakeTranslateMatrix({origin.x, origin.y, 0.f});
   auto command = textRenderer_->QueueQuad(size, matrix, {}, {1.f, 1.f}, {1.f, 1.f, 1.f, 1.f},
      texture, uiViewportSize_, identity);
   command.blendMode = BlendMode::kBlendModeAdd;
   SubmitDrawCommand(DrawCommand(RenderPass::PostProcess, command));

   auto style = frame->captionStyle;
   style.screenAnchor = UIAnchor::BottomRight;
   style.pivot = {0.5f, 1.f};
   DrawUIText(frame->caption, frame->GetCaptionOffset(), style);
}

UIInputFrame Renderer::ReadUIInput(Input& input, InputActionService& actions, bool mouseEnabled) {
   UIInputFrame frame;
   frame.gamepadConnected = input.GetConnectedGamePadCount() != 0;
   bool standalone = true;
#ifdef USE_IMGUI
   standalone = !imGuiManager_ || !imGuiManager_->IsDockSpaceVisible();
#endif
   if (standalone) {
      RECT rectangle{};
      const auto hwnd = GetActiveWindow();
      GetClientRect(hwnd, &rectangle);
      if (mouseEnabled) {
         POINT origin{};
         ClientToScreen(hwnd, &origin);
         gameUIOrigin_ = {static_cast<float>(origin.x), static_cast<float>(origin.y)};
      }

      gameUISize_ = {static_cast<float>(rectangle.right), static_cast<float>(rectangle.bottom)};
      gameUIActive_ = hwnd && GetForegroundWindow() == hwnd;
   }

   DWORD foregroundProcess = 0;
   GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
   frame.active =
      foregroundProcess == GetCurrentProcessId() && gameUIActive_ && gameUISize_.x > 0.0f && gameUISize_.y > 0.0f;

   // 無効時はポインターの全フィールドを空に保ち、ホバーによるフォーカス移動や方向入力の遮断も防ぐ。
   if (mouseEnabled) {
      POINT pointer{};
      GetCursorPos(&pointer);
#ifdef USE_IMGUI
      // Viewports無効時のImGui座標はクライアント座標であり、デスクトップ座標を直接引かない。
      if (!standalone && !imGuiManager_->IsMultiViewportEnabled()) {
         const auto client = input.GetMouseScreenPosition();
         pointer = {static_cast<LONG>(client.x), static_cast<LONG>(client.y)};
      }
#endif
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
   }

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

void Renderer::DrawManagedUI(SceneWorld& world) {
   auto* textures = assetManager_ ? assetManager_->GetTextureManager() : nullptr;
   auto* fonts = assetManager_ ? assetManager_->GetFontManager() : nullptr;
   if (!textures || !fonts || !textRenderer_) return;
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
            SubmitDrawCommand(DrawCommand(RenderPass::Overlay, command));
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
         SubmitDrawCommand(DrawCommand(RenderPass::Overlay, command));
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

void Renderer::DrawCameraLetterbox() {
   if (letterboxProgress_ <= 0.0f || !assetManager_ || !textRenderer_) return;

   auto* textures = assetManager_->GetTextureManager();
   auto* white = textures ? textures->GetTexture("white1x1") : nullptr;

   if (!white) return;

   constexpr float kLetterboxHeightRatio = 0.12f; ///< 画面の高さに対する片側の黒帯の最大比率。
   const float t = letterboxProgress_;
   const float opacity = t * t * (3.0f - 2.0f * t);
   const float height = uiViewportSize_.y * kLetterboxHeightRatio * opacity;
   const auto identity = MakeIdentity4x4();
   const auto drawBar = [&](float y) {
      const auto command = textRenderer_->QueueQuad({uiViewportSize_.x, height}, MakeTranslateMatrix({0.0f, y, 0.0f}),
         {}, {1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, opacity}, white->GetTextureSrvHandleGPU(), uiViewportSize_, identity);
      SubmitDrawCommand(DrawCommand(RenderPass::Overlay, command));
   };

   // 通常UIの上に黒帯と案内を順に重ね、カット切り替え時はまとめて暗転させる。
   drawBar(0.0f);
   drawBar(uiViewportSize_.y - height);

   auto* fonts = assetManager_->GetFontManager();

   if (!fonts || letterboxCaption_.empty() || letterboxCaptionStyle_.color.w <= 0.0f) return;

   constexpr float kCaptionMargin = 32.0f; ///< 案内と右端の間隔（基準UIのピクセル）。
   auto style = letterboxCaptionStyle_;
   style.pivot = {};
   const auto layout = fonts->LayoutText(letterboxCaption_, style);
   const auto matrix = MakeTranslateMatrix({uiViewportSize_.x - layout.size.x - kCaptionMargin,
      uiViewportSize_.y - (height + layout.size.y) * 0.5f, 0.0f});
   const auto commands = textRenderer_->QueueText(layout, style, Transform{}, UITextComponent::kShowAllGlyphs,
      Window::kUiReferenceWidth, Window::kUiReferenceHeight, uiViewportSize_.x, uiViewportSize_.y, &matrix);

   for (const auto& command : commands) {
      SubmitDrawCommand(DrawCommand(RenderPass::Overlay, command));
   }
}
}
