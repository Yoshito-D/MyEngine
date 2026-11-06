# パラメータの単位

自作エンジン・ゲーム・シェーダー・保存JSONでは、次の単位を共通で使う。
外部ライブラリ固有の単位は呼び出し境界で変換する。

| 量 | 内部計算・保存JSON | Inspector・デバッグ表示 |
| --- | --- | --- |
| ワールド位置・距離・半径 | m（1ワールド単位 = 1 m） | m |
| 時間・更新間隔・寿命 | s | s |
| 移動速度・ジャンプ初速・速度インパルス | m/s | m/s（レースHUDのみkm/h） |
| 加速度・重力・質量を持たない粒子のForce | m/s² | m/s² |
| 角度・Euler角・FOV・UV回転 | rad | degree（deg） |
| 角速度 | rad/s | deg/s |
| 指数補間・減衰・速度増幅の係数 | 1/s | 1/s |
| 単位質量のカメラばね剛性 | 1/s² | 1/s² |
| 質量 | kg | kg |
| UI位置・寸法 | pixel | pixel |
| スケール・割合・入力・速度倍率 | 無次元 | 無次元 |

`speed`という名前でも、補間係数（1/s）、角速度（rad/s）、マウス感度（rad/pixel）は別の量として扱う。
OrbitalBodyのパン感度は1/pixelであり、距離（m）とマウス移動量（pixel）を掛けて移動量（m）を得る。
ホイール感度はm/ホイール入力値、UVスクロール速度はUV/sを使う。
粒子ノイズの空間周波数はrad/m、位相速度はrad/sで保持し、画面ではdeg/m・deg/sへ換算する。

角度の表示・編集では`ImGuiHelper::DragRadiansAsDegrees`、`DrawEulerDegreesControl`などを通す。
内部のrad値へdegreeの値を直接書き込まない。未編集時は往復変換せず、保持値を維持する。
角度の変換係数とm/s→km/hの換算係数は`MathConstants.h`を参照する。速度計ごとの換算倍率は持たない。

速度は`position += velocity * deltaTime`、加速度は`velocity += acceleration * deltaTime`で適用する。
指数応答には`1 - exp(-rate * deltaTime)`、速度の指数増幅には`exp(growthRate * deltaTime)`を使う。
毎フレーム固定の倍率を適用しない。瞬間的な速度変化（m/s）にはdeltaTimeを掛けない。

粒子の`linearAcceleration`はm/s²、`speedGrowthRate`は1/s（0で増幅なし）、
`dampingRate`は1/s（0で減衰なし）である。CPUとGPUは同じ単位と式を使う。
保存済みのフレーム基準値は60 FPS時の応答を基準に換算済み。減衰割合が1以上だった設定は、
速度反転を防ぐため約829 1/sの即時制限相当へ正規化している。旧形式の読込分岐は保持しない。
