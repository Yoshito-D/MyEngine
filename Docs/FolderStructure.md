# フォルダ構成

エンジンの共通機能とゲーム固有の処理を分け、各ファイルを担当する機能の近くに配置する。
ヘッダーと実装ファイルは同じフォルダーに置く。

```text
MyEngine/
├─ Project/
│  ├─ GameEngine/
│  │  ├─ Framework/          起動・終了・メインループ・EngineContext
│  │  ├─ Time/               フレーム時間・FPS計測・タイマー
│  │  ├─ Utility/            小規模な共通補助処理
│  │  │  ├─ Logger.h/.cpp
│  │  │  ├─ StateMachine.h/.cpp
│  │  │  ├─ JsonFile.h/.cpp
│  │  │  ├─ ExportDump.h
│  │  │  └─ D3DResourceLeakChecker.h
│  │  ├─ Math/
│  │  │  ├─ Types/           ベクトル・行列・Quaternion・Transform
│  │  │  └─ Functions/       演算・補間・色・乱数
│  │  ├─ Graphics/
│  │  │  ├─ Device/          DirectX・GPU資源の基盤
│  │  │  ├─ Resources/       Mesh・Texture・Materialなどの描画資源
│  │  │  ├─ Pipeline/        シェーダー・PSO・RootSignature
│  │  │  ├─ Renderer/        描画コマンド・描画パス・ライトのGPU転送
│  │  │  └─ PostProcess/     ポストエフェクト
│  │  ├─ Assets/             読み込んで共有するデータとキャッシュ
│  │  │  ├─ Model/
│  │  │  ├─ Animation/
│  │  │  ├─ Texture/
│  │  │  ├─ Material/
│  │  │  ├─ Audio/
│  │  │  └─ Font/
│  │  ├─ Audio/              音声再生
│  │  ├─ Input/              入力・入力アクション
│  │  ├─ Window/             Windowsウィンドウ
│  │  ├─ Scene/              シーン・カメラ・ライト
│  │  ├─ Object/             シーン上で動く実体
│  │  │  ├─ Component/       基盤・描画・音声・UIなどのコンポーネント
│  │  │  ├─ Model/
│  │  │  ├─ Sprite/
│  │  │  ├─ Skybox/
│  │  │  └─ Text/
│  │  ├─ Effects/Particles/  パーティクルと各種モジュール
│  │  ├─ UI/Text/            ゲーム用テキストのレイアウト
│  │  ├─ Editor/             編集画面・編集操作・ImGui
│  │  └─ Collision/          当たり判定
│  ├─ GameProject/
│  │  ├─ Game.h / Game.cpp   アプリケーション本体
│  │  └─ App/
│  │     ├─ Scene/           ゲームのシーン生成・カタログ
│  │     └─ Component/
│  │        ├─ Camera/RearFollow/
│  │        ├─ Character/
│  │        ├─ Gravity/
│  │        ├─ Race/UI/
│  │        ├─ Title/
│  │        ├─ Tutorial/
│  │        └─ Vehicle/
│  ├─ Docs/                  実装解説・説明資料・計測結果の解説
│  │  ├─ Camera/
│  │  ├─ Rendering/
│  │  └─ Slides/
│  ├─ Externals/             外部ライブラリ
│  ├─ Resources/
│  │  ├─ engine/             エンジン共通のシェーダー・設定・画像
│  │  └─ game/               ゲームのシーン・モデル・画像・音声
│  └─ MyEngine.sln など      Visual Studio設定・エントリーポイント
├─ Docs/FolderStructure.md   フォルダ構成の説明
├─ Tools/                    開発用スクリプト
└─ Generated/                ビルド出力・計測結果・生成資料（Git対象外）
```

## 配置の判断

- `Time`はフレームの時間計測とタイマーをまとめる。現在の`TimeProfiler`はデルタ時間・FPSを計測するため、ここに配置する。
- `Utility`はログ出力・汎用状態管理・JSONファイル保存・クラッシュダンプ・リーク検査など、小規模な共通補助処理をまとめる。
- 時間・数学・描画・音声など、機能としてまとまっている処理はそれぞれの機能フォルダーへ配置する。
- `Assets`は読み込んで共有するデータ、`Object`はシーンに配置して更新する実体を扱う。
- `Graphics/Resources`はGPUで使う描画資源、`Assets`はファイル読み込みとキャッシュを扱う。
- `Scene/Light`はシーン上のライト、`Graphics/Renderer/Light`はGPUへ渡すライト情報を扱う。
- `UI`はゲーム画面の共通UI処理、`Editor`は開発中の編集操作とImGuiを扱う。
- `Object/Component`は`Base`・`Rendering`・`Audio`・`Animation`・`Effects`・`Collision`・`UI`で分ける。
- ゲーム固有の処理は`GameProject`内に置き、車両・重力・レースなどの機能別にまとめる。
- 外部ライブラリと実行時リソースの配置は、各ライブラリや読み込みコードが使うパスを保つ。
- 構成説明はこのファイル、実装・説明・計測資料のMarkdownは`Project/Docs`へ配置する。
- リポジトリの入口である`README.md`、作業指示の`AGENTS.md`、外部ライブラリに同梱されたMarkdownは各用途の場所で管理する。
- 計測結果の解説は`Project/Docs/Camera/Measurements`、元のCSV・画像などのデータは`Generated/Diagnostics`で管理する。

## インクルードとVisual Studio

エンジンとゲームのヘッダーは、`Project`を基準にしたパスを明示する。
各機能のフォルダーをインクルード検索パスへ個別に追加する必要はない。

```cpp
#include "GameEngine/Graphics/Resources/Mesh.h"
#include "GameEngine/Math/Types/Vector3.h"
#include "GameProject/App/Component/Camera/RearFollow/PlayerRearFollowCamera.h"
```

ファイルを追加・移動・削除したら、`Project/MyEngine.vcxproj`の登録を更新する。
続いて次のコマンドを実行するか、`Tools/RunFilterAdjust.bat`を開いてフィルターを更新する。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\FilterAdjust.ps1
```

スクリプトは`vcxproj`に登録されたファイルから`.filters`を生成する。
`vcxproj`自体の登録は変更しない。スクリプトの場所からプロジェクトを解決するため、
起動時の作業フォルダーに依存せず、管理者権限も必要ない。

## JSON保存処理

`Utility/JsonFile.h`は`SaveJsonFileAtomically`を提供する。
シーン保存で使用されていた処理を維持し、参照されていなかった
`JsonDataManager`と`JsonGroup`のクラスは削除した。

## 関連資料

- [プレイヤーのシルエット影](../Project/Docs/Rendering/PlayerShadowPass_Implementation.md)
- [追従カメラの修正履歴](../Project/Docs/Camera/PlayerRearFollowCamera_FixFlow.md)

## エディタの所有と編集経路

`GameEngine/Scene/SceneObjectStore`はランタイムとエディタ共通の生成・所有・復元を担当する。
`GameEngine/Editor`はアセット一覧、パネル、参照欄、編集履歴を担当する。
詳細と操作・制約は[EditorWorkflow](../Project/Docs/Editor/EditorWorkflow.md)を参照。
