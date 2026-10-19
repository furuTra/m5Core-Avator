# 視線トラッキング仕様（SYS.IMU_TRACK）

出典: [reference/microcontroller-face-ui-spec.md](reference/microcontroller-face-ui-spec.md) 4.1章、
[reference/spec-canvas.pdf](reference/spec-canvas.pdf) 02_EYE TRACKING RESPONSE

## 概要

瞳（`pupil_dot.png` / `pupil_ring.png`。[expressions.md](expressions.md) 参照）は、
基準位置からの変位ベクトル `(dx, dy)` に応じて画像スプライトごと座標移動させる。
形状そのものは変えず位置だけ動かすため、画像アセット方式でも元資料の見た目をそのまま再現できる。

## 入力ソース

### 6軸IMU（本体の傾き）

元資料は左右方向の3段階サンプルのみを提示していた:

| 状態 | 瞳の変位 |
|---|---|
| 左に傾ける | `(-4px, 0px)` |
| 中央（水平） | `(0px, 0px)` |
| 右に傾ける | `(+4px, 0px)` |

M5Unified の `M5.Imu.getAccel(&ax, &ay, &az)` から重力加速度（単位g）を取得し、
角度に応じて瞳オフセットへ線形マッピングする（連続値化を採用。3段階に丸めない）。

#### 現在の実装：IMU軸と画面／目線の対応

実機（M5Stack Core2, `setRotation` はデフォルト＝横向き）で目視確認済みの対応関係。
現在のコードは [`lib/ImuInput/ImuInput.cpp`](../../lib/ImuInput/ImuInput.cpp)（傾き→画面上の点）。
以下の `GAZE_GAIN_*` / `GAZE_TILT_SIGN` は分離前の名称で、現在はそれぞれ `IMU_GAIN_X/Y`・
`IMU_SIGN_X/Y` に相当する。

- **左右（画面X, 右が＋）← `ax`**：本体の左右の傾き。IMU軸と画面軸が90°ずれていたため
  符号を反転して `dx = -ax * GAZE_GAIN_X` で割り当てる。
- **上下（画面Y, 下が＋）← `az`**：本体の前後の倒し（＝画面法線方向の重力成分）。
  `dy = GAZE_TILT_SIGN * az * GAZE_GAIN_TILT`。**画面平面内の `ay` は上下には使わない。**
- **`ay`（画面Y平面内の傾き成分）**：視線には未使用。

前後の倒しと目線の対応（`az` は 0→±1 の広い範囲で振れる）:

| 本体の姿勢 | `az` | 目線 |
|---|---|---|
| 縦に立てる（画面が正面を向く） | ≈ 0 | 中央（正面） |
| 横に寝かせる（画面が上を向く） | ≈ +1 | 下 |
| 逆さ（画面が下を向く） | ≈ -1 | 上 |

調整用の定数（同ファイル冒頭の匿名 namespace 内）:

- `GAZE_GAIN_X` / `GAZE_GAIN_TILT`：左右／上下の感度。`GAZE_TILT_SIGN`：上下の符号
  （実機で上下が逆なら `-1.0f`。現状は `+1.0f` で正しいことを確認済み）。
- 瞳オフセットの上限(px)：アバター定義ファイル `avatar.json` の `config.groups.pupil.max`。白目内に収める
  （[docs/avatar-definition.md](../../docs/avatar-definition.md)）。

なお元資料の左右3段階（`±4px`）はあくまでサンプル値で、実装の上限は上記の定義ファイルの値に従う。

### 静電容量式タッチ（`M5.Touch`）

- タッチ座標を目標ベクトルに変換する。例: 左上をタッチ → 目標 `(+4, -2)`。
- タッチなしの場合は中央・フォーカス済みのデフォルト状態 `(0, 0)`。
- 右下をタッチした場合は対角方向へのオフセット（例: `(-4, +2)` 相当）も想定される。
- 具体的な換算式（タッチ座標→変位ベクトルの写像関数）は元資料に明記がないため、
  アバターエリア中心 `(160, 108)` からのタッチ座標の相対位置を正規化し、
  水平±4px・垂直±2px の範囲にスケーリングする実装を基本方針とする。

## 合成ロジック

- IMUとタッチの両方が同一の目標ベクトル `(dx, dy)` に書き込み、瞳はそこへ向けて補間する。
  両方の入力が同時に発生した場合の優先度（タッチ優先か、加算合成か）は元資料に明記がないため
  [open-questions.md](open-questions.md) に記載する。実装の初期方針としては
  **タッチ入力がある間はタッチを優先し、タッチが離れたらIMUの値にフォールバックする**。
- 瞳の位置は目標ベクトルへ [expressions.md](expressions.md) の状態遷移と同じ **120ms補間** で
  近づける。画像スプライトの座標移動なので、状態遷移のクロスフェードと異なり単純な
  線形補間（イージング）で実装できる。
- 仕様上確認できる最大変位は水平±4px。タッチ入力では垂直方向・斜め方向のオフセットも発生しうる
  （±2px程度を目安とする）。
- `Alert` 状態のリング状（アウトライン）の瞳 `pupil_ring.png` も、塗りつぶし円の瞳
  `pupil_dot.png` と同様に移動対象とする。`Sleep` 状態は瞳を描画しないため対象外。

## 実装モジュールの構成（実装済み）

視線処理は「入力アダプタ」と「視線制御」に分離してある（[docs/architecture.md](../../docs/architecture.md)
の視線制御の分離を参照）。

**入力モジュール**（デバイス → 画面上の点）。どちらも `getPoint(x, y)` で「いまの点」を返すだけで、
座標の用途（視線・ボタン判定）は持たない。`M5.update()` の後に呼ぶ。

- `TouchInput`（`lib/TouchInput/`）: **タッチの有無と画面座標の取得だけ**を担う（実装済み）。
  タッチ中のみ `true` と座標を返す。`M5.Touch.getDetail()` の押下判定・座標の取り出しをここへ
  閉じ込める。表示領域の下（ボタン帯）のタッチは y が 240 以上になるが、クランプせずそのまま返す。
- `ImuInput`（`lib/ImuInput/`）: **傾き → 画面上の点**の換算だけを担う（実装済み）。IMU固有の事情
  （使用する加速度軸・画面との軸ずれ・符号・感度）をここへ隔離する。左右 ← `-ax`、上下 ← `az`
  （前後の倒し）。感度は `ImuInput.cpp` の `IMU_GAIN_X/Y`、符号は `IMU_SIGN_X/Y`（実機で上下/左右が
  逆なら符号を反転）。基準点（傾き0＝正面）はアバターの目の中心を `begin()` で渡す。

**調停**:

- `EyeTracker`（`lib/EyeTracker/`）: 入力された点から **追従先の座標(画面px)を1つ決める**。点がどこから
  来たか（タッチ・IMU）には関知せず、どの点を優先していつまで保持するかだけを持つ（デバイスのヘッダも
  include しない）。中立点(=正面)はアバターの目の中心
  （`FaceRenderer::gazeCenterX()/gazeCenterY()`）を渡す。
  - `setFocusTarget(x, y)`（注目点＝優先）/ `setBaseTarget(x, y)`（既定の追従先）で受け取った座標を
    **そのまま追従先の点にする**。可動域を超える分は `lookAt` 側の正規化でクランプされるため、
    座標→変位ベクトルの換算式は不要。ボタン帯のタッチ（y≧240）もそのまま扱う＝下を見る。
  - **優先度**（上記「合成ロジック」の初期方針どおり）: `main` がタッチを注目点、傾きを既定の追従先へ
    割り当てているため、タッチが優先される。指を離した後も `FOCUS_HOLD_MS`（現在1500ms）はその点を
    見続け、期限が切れたら傾きの点へフォールバックする。
    「離した瞬間に傾き方向へ戻る」と見ていた印象が残らないための余韻。
**視線の計算**（デバイス非依存・描画非依存。他プロジェクトへ流用できる単位）:

- `GazeController` / `GazeOffset`（`lib/GazeController/`）: 追従先の座標から実際の動きを決める本体。
  `GazeController::lookAt(x, y)` が中立点（目の中心）からの方向を可動域 `reach`（本プロジェクトでは
  定義ファイルの `gazeReach`）で正規化し、`GazeOffset::update()` が各動きグループの可動量
  （`groups.*.max`）へ比例配分して `groups.*.smooth` 係数で線形補間する。整数px が変化したフレームだけ
  `true` を返し、それが再描画の判定になる。標準ライブラリ以外に依存しない
  （[lib/GazeController/README.md](../../lib/GazeController/README.md)）。
- `FaceRenderer`（`lib/FaceRenderer/`）: `lookAt(x, y)` を `GazeController` へそのまま渡し、返ってきた
  オフセット(px)で瞳・白目/眉・鼻口眼鏡を描く。アバター固有の値（中立点・可動域・可動量・追従係数）は
  `FaceRenderer.cpp` 冒頭の定数として持ち、`begin()` で視線制御へ渡す。
- `src/main.cpp`: 毎フレーム 1) `touchInput.getPoint()` → `eyeTracker.setFocusTarget()`（タッチ時のみ）、
  2) `imuInput.getPoint()` → `eyeTracker.setBaseTarget()`、3) `eyeTracker.getTarget()` で追従先の位置を
  取得、4) `faceRenderer.lookAt()` でその位置へ視線を向ける、の順で橋渡しする。
  タッチとIMUのどちらを優先するかは、この割り当て（注目／既定）を変えるだけで切り替えられる。
- 入力源を通信などへ増やす場合も、同じ「画面上の点を返す」入力モジュールを用意して `EyeTracker` へ
  渡すだけでよい（`FaceRenderer` の変更は不要）。タッチがまさにその例で、画面座標をそのまま追従先と
  して渡すだけで実装できている。

> **注**: 下部コントロールバー（[control-buttons.md](control-buttons.md)）を実装する際は、ボタン矩形への
> タッチを視線追従と取り合わないよう、`main` でボタン判定を先に行って消費する（`setFocusTarget` を
> 呼ばない）。座標の取得と用途が分かれているため、`TouchInput` / `EyeTracker` 側の変更は不要。
> 現状はコントロールバー未実装のため、全てのタッチを視線追従に使っている。
