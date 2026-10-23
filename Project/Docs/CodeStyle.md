# コードの書式とコメント

エンジン・ゲーム・シェーダーの自作コードでは、`Project/.clang-format`と
`Project/.editorconfig`を共通の書式として使う。外部ライブラリと生成コードは対象外とする。

## インデントと改行

- インデントはスペース3文字とし、タブを混在させない。
- 開き波括弧は宣言や条件と同じ行に置く。
- `else`のない`if`で処理が`return;`だけの場合は、120文字以内なら`if (!camera) return;`のように1行で書く。
- それ以外の`if`・`else`・`for`・`while`・`do`の処理は必ず`{}`で囲み、次の行へ書く。
  `return false;`など値を返す処理も`{}`で囲む。`else if`は通常の連結した形で書く。
- 処理に説明コメントがある場合は、`return;`だけでも`{}`を残してコメントを保持する。
- 実装ファイルの関数は複数行に展開する。
- 通常関数は原則としてヘッダーで宣言し、対応する`.cpp`に実装する。
- `enum`・`enum class`・`struct`・`class`の定義は原則として対応するヘッダーに置く。クラス内部だけで使う型は`private`に置き、関数の局所状態に依存するガードなどは`.cpp`に残してよい。
- `.cpp`の`#include`はすべて先頭にまとめ、宣言・定義の途中に置かない。条件付きインクルードの`#if`なども先頭のインクルード群に置き、必要な条件を維持する。
- 一行で書ける短いアクセサーやメンバー関数はヘッダーに定義してよい。複数の処理や長いロジックは`.cpp`へ分離する。
- テンプレートと、定数式での評価に定義が必要な`constexpr`／`consteval`関数はヘッダーに実装を置く。
- 自明な特殊メンバーの`= default`／`= delete`と純粋仮想関数の`= 0`はヘッダーに残す。
- 行の長さは120文字を目安とし、長い引数・条件・式は意味のまとまりで改行する。
  文字列と既存コメントの内容は自動整形で書き換えない。
- 改行コードはCRLF、文字コードはUTF-8とし、末尾に改行を1つ置く。行末の空白は残さない。

```cpp
if (!camera) return;

if (!resource) {
   return false;
}

if (camera) {
   camera->Update();
}

for (auto& item : items) {
   item.Update();
}
```

## 空行

- 関数・クラスの定義の間には空行を1行置く。空行を2行以上連続させない。
- 公開API同士の間には空行を1行置き、説明コメントと宣言は離さない。
- 関数内では、入力確認、準備、計算、結果の反映など、処理の目的が変わる位置に空行を置く。
  同じ目的の連続した代入や計算はまとめる。
- `if`・`for`・`while`・`do`・`switch`の前後は、前後の処理と空行1行で区切る。
  1行の早期リターンも同じ扱いとし、連続する制御文の間にも空行を置く。
- ブロックの先頭・末尾には制御文のための空行を追加しない。
  `if`・`else if`・`else`の連結部分や、`do`の末尾の`while`は分けない。
- 制御文の直前に説明コメントがある場合は、コメントと制御文を離さず、コメントの前に空行を置く。
- `#pragma once`の後と、インクルード群の後には空行を1行置く。

```cpp
auto* camera = GetCamera();

if (!camera) return;

const auto deltaTime = GetDeltaTime();

// 有効な項目だけを更新する。
for (auto& item : items) {
   if (!item.IsActive()) {
      continue;
   }

   item.Update(deltaTime);
}

camera->Update();
```

## コメント

- 公開APIは`/// @brief`で役割を説明し、必要に応じて`@param`・`@return`・`@details`を添える。
  オーバーライドが同じ契約を引き継ぐ場合は`/// @copydoc`を使う。
- 実装内の説明は`// `で始め、該当する処理の直前に置く。
- 非自明な処理では、採用した理由、座標系、単位、寿命、実行順序などの制約を説明する。
  コードをそのまま言い換えるコメントは増やさない。
- 既存のコメントは正しい限り保持する。処理を変更するときは説明との整合も確認する。

```cpp
/// @brief 指定された設定を検証し、有効な場合だけ反映する。
bool Configure(const Settings& settings);

/// @brief 現在の設定を取得する。
const Settings& GetSettings() const;
```

## 数値型とリンク

- 整数の幅は`<cstdint>`の型で明示する。通常の符号付き整数は`std::int32_t`、パック済みの色などは`std::uint32_t`を使う。
- メンバー変数や保存・GPU転送するデータは、特に整数のメモリ幅を明示する。
- 要素数と添字には用途に応じて`size_t`を使い、Windows／DirectXなどの境界ではAPIが要求する型を使う。
- `main`／`WinMain`など規定のシグネチャを持つ入口や外部APIのコールバックは、規定どおり`int`を使ってよい。
- 浮動小数点は`float`を基本とし、浮動小数点リテラルには`f`を付ける。
- JSONの保持値が`float`の表現範囲に入るか検証する処理など、狭める前の精度が必要な箇所だけ`double`を使い、理由をコメントする。
- 通常関数へ明示的な`inline`を付けない。共有の可変な静的メンバーはヘッダーで宣言し、`.cpp`で一度だけ定義する。
- ヘッダーの名前空間定数には`constexpr`を使う。

## 整形

Visual Studioでは`.clang-format`による書式設定を有効にする。
短い`if`の1行化は`AllowShortIfStatementsOnASingleLine: WithoutElse`で指定する。
`clang-format`には`return;`だけを例外にして波括弧を挿入する設定がないため、
`InsertBraces`は無効にし、それ以外の処理の`{}`はコード側で必ず付ける。
コマンドラインでは対象ファイルを指定して実行する。

```powershell
clang-format -i Project/GameEngine/Audio/Audio.cpp
clang-format --dry-run --Werror Project/GameEngine/Audio/Audio.cpp
```

整形後は差分を確認し、処理・文字列・コメントの意味が変わっていないことを確かめる。
