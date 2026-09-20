# PlayerRearFollowCameraの構成

各処理は`RearCameraComponent`を共通基底として`ICinemachineComponent`を実装します。`VirtualCamera`が各インスタンスを所有し、`CalculateState`からそれぞれの`MutateCameraState`を呼び出します。

| コンポーネント | Stage / 順序 | 役割 |
| --- | --- | --- |
| PlayerRearFollowCamera | Body / -1000 | 外部入力、既存設定、旧シーンとの互換性 |
| RearCameraTransition | Body / -900 | 離着陸イベント、着地予測と構図のブレンド |
| RearCameraGravityUp | Body / -800 | 重力Upの補間、後方方向へのUpデルタ回転の通知 |
| RearCameraPlanetGuide | Body / -700 | 空中の惑星方向ガイド、離陸後の待機と復帰 |
| RearCameraDirectionTracker | Body / -600 | 後方方向の追従、着地後の方向復帰 |
| RearCameraSpeedEffects | Body / -500 | FOV、速度変化によるばね演出、距離ブースト |
| RearCameraPositionSolver | Body / -400 | ピボット相対位置、最小距離と角速度の制約 |
| RearCameraAimSolver | Aim / -1000 | 注視点、姿勢、View行列と表示軸 |
| RearCameraMeasurementRecorder | Aim / -900 | Noise適用前の軸計測、集計、ファイル出力 |
| RearCameraDebugView | Aim / -800 | Inspectorの設定編集、検証画面、計測オーバーレイ |

`GetExecutionOrder`は同一Stage内の順序です。Stage自体は引き続きBody → Aim → Noiseの順で、同じ順序値のコンポーネントは追加順を維持します。空中・画面構図ブレンドの更新は重力Upの計算と独立しているため、離着陸コンポーネントで先にまとめて更新します。着地予測ブレンドは従来どおりUpの計算前に確定させています。分割前との出力比較で一致を確認しています。

旧シーンの`PlayerRearFollowCamera`エントリを読み込むと、不足する9部品を自動で追加します。以後の保存では各部品も独立したエントリとして保存され、有効状態を個別に復元できます。設定キーと`camera.distance`などの既存APIを保つため、設定値は引き続き`PlayerRearFollowCamera`の`RearCameraSettings`に集約しています。補間履歴は各計算部品が個別に所有します。

`PlayerRearFollowCamera::MutateCameraState`自体では追従計算を行いません。更新には、部品を追加した`VirtualCamera::Update`または`CalculateState`を使用します。個別に無効化した計算部品は更新されず、他の部品は最後の計算結果を参照します。親となる入力部品の無効化は全体を停止します。

兄弟部品へのポインターは更新ごとに解決します。必須の計算部品が削除された場合は、部分的な姿勢更新を避けて全体を停止します。`AddComponentByName("RearCameraGravityUp")`などで再追加できます。計測・表示部品は必須ではなく、削除しても追従計算は継続します。

回帰テストの実行方法と検証範囲は`Tests/Camera/README.md`に記載しています。
