# アーキテクチャ方針（ディレクトリ構成）

機能ごとに `lib/` 以下へモジュール（クラス）として分割する。`src/main.cpp` は初期化処理と
`loop()` からの呼び出しのみを担い、描画・状態管理のロジックを直接書かない。

```
lib/
  AssetLoader/       LittleFS(フラッシュ)から画像アセットを読み込むユーティリティ
  AvatarDefinition/   アバター定義ファイル(avatar.json)を起動時に読み、パーツ構成・動かし方を保持する
  FaceRenderer/       定義に従って画像パーツを合成描画する（まばたき含む）。視線の計算は GazeController に委譲
  GazeController/     追従先座標(x,y) → 目標方向 → 各パーツのオフセット(px)。描画・入力に非依存（流用可）
  TouchInput/         画面タッチの有無と座標(画面px)を取り出すだけの入力モジュール
  ImuInput/           本体の傾き(6軸IMU)を座標(画面px)へ換算するだけの入力モジュール
  EyeTracker/         入力された点から追従先の座標(画面px)を1つ決める調停モジュール（優先度と保持）
  (将来) AnimationController/ 表情の状態遷移など、時間経過に基づく状態変化を管理する
  (将来) WifiScanner/  既存のWiFiスキャン機能を切り出す場合はここに配置
src/
  main.cpp            setup()/loop() のみ。各モジュールの初期化と、
                      入力位置(TouchInput / ImuInput) → 追従先(EyeTracker) → 視線(FaceRenderer) の橋渡し
```

## 視線制御の分離（入力アダプタと視線の境界）

視線処理は次の3つに分離する。**視線の計算部（`GazeController`）は入力源にも描画方法にも依存しない**
ため、フォルダごと他プロジェクトへ流用できる（[lib/GazeController/README.md](../lib/GazeController/README.md)）。

1. **点を作る**（`TouchInput` / `ImuInput` などの入力モジュール）— デバイス固有
2. **点を調停する**（`EyeTracker`）— デバイス非依存
3. **点を視線の動きへ変換する**（`GazeController`）— デバイス非依存・描画非依存
   → 求まったオフセット(px)で描くのが `FaceRenderer`

- `GazeController` / `GazeOffset`（`lib/GazeController/`）: 視線計算の本体。標準ライブラリ以外に依存
  しない（Arduino / M5Unified も include しない）。
  - `GazeController::lookAt(x, y)` … 追従先の点を、中立点からの目標方向(-1..1)へ正規化する。
    中立点・可動域(`reach`)は `begin()` で与える（このプロジェクトでは定義ファイルの `gazeCenter` と
    `gazeReach`）。
  - `GazeOffset::update(dirX, dirY)` … 目標方向を1パーツぶんの変位(px)へ比例配分し、滑らかに追従
    させる。可動量と追従の速さはパーツごとに `begin()` で与える。描画に使う**整数px**が変化した
    フレームだけ `true` を返すので、これを再描画の判定に使う。
- `FaceRenderer::lookAt(x, y)`: 追従先の座標(画面px)を受け取り、そのまま `GazeController` へ渡すだけ。
  このクラスの責務は、求まったオフセットで各動きグループを描くこと（値の計算は持たない）。
  アバター固有の値（中立点、可動域、グループごとの可動量と追従係数）は `AvatarDefinition` が定義ファイル
  から読んだものを `begin()` で視線制御へ渡す。実際の追従は `update()` が毎フレーム進める。
- `AvatarDefinition`: 定義ファイル `avatar.json` を起動時に ArduinoJson で一度だけ読み、構造体へ写す。
  描画方法には関知しない。読み込み・検証に失敗すると `main` が `DEF ERROR` を表示する
  （[avatar-definition.md](avatar-definition.md)）。
- **入力モジュール**（デバイス → 画面上の点）: どちらも `getPoint(x, y)` で「いまの点」を返すだけで、
  座標の用途（視線・ボタン判定など）は知らない。視線以外の用途にも使える。
  - `TouchInput`: `M5.Touch` の押下判定と座標の取り出し。タッチ中のみ `true`。
  - `ImuInput`: `M5.Imu` の傾きを基準点まわりの点へ換算する。IMU固有の事情（使用する加速度軸・
    画面との軸ずれ・符号・感度）をこのモジュールに隔離する。基準点(傾き0=正面)は目の中心を渡す。
- `EyeTracker`: 入力された点から追従先を1つ決める調停役。**点がどこから来たか（タッチ・IMU・通信）には
  関知せず**、どの点を優先していつまで保持するかという視線の振る舞いだけを持つ。デバイスのヘッダも
  include しない。中立点(=正面)は `FaceRenderer` の目の中心を渡す。
  - `setFocusTarget(x, y)` … 一時的に注目させたい点。**優先**され、指定が途切れた後も一定時間
    （`FOCUS_HOLD_MS`）その点を見続ける。
  - `setBaseTarget(x, y)` … 既定の追従先。注目の保持が切れている間はこの点を見る。
  - 入力が何も無ければ中立点を返して `false`。
  - どの入力を注目／既定に割り当てるかは `main` が決める（現在はタッチ＝注目、傾き＝既定）。
- `main.cpp`: 毎フレーム以下の順に橋渡しする。
  1. `touchInput.getPoint(x, y)` → `eyeTracker.setFocusTarget(x, y)` … タッチ位置を注目点として渡す
  2. `imuInput.getPoint(x, y)` → `eyeTracker.setBaseTarget(x, y)` … 傾きの位置を既定の追従先として渡す
  3. `eyeTracker.getTarget(x, y)` … 追従先の位置を取得する
  4. `faceRenderer.lookAt(x, y)` … 取得した位置へ視線を向ける

  「位置を決める」(3) と「その位置へ視線を向ける」(4) は担当モジュールが変わるため、処理も分けて書く。
- 入力源を通信などへ増やす場合も、同じ「座標を返す」入力モジュールを用意して `EyeTracker` へ渡すだけでよい
  （`GazeController` / `FaceRenderer` の変更は不要）。
- 逆に他プロジェクトへ視線の動きだけを持っていく場合は `lib/GazeController/`（必要なら `lib/EyeTracker/`）
  をコピーし、点の作り方と描き方だけをそのプロジェクトで用意する。

- 各モジュールは `ライブラリ名/ライブラリ名` のヘッダ + `.cpp` の構成とする（PlatformIO の慣例に従う）。
  モジュール名・ファイル名は PascalCase で、`lib/<ModuleName>/<ModuleName>.h` とする（[コーディング規約](coding-standards.md) 1章・2章）。
- モジュール間は明確なインターフェース（クラスの public メソッド）を介してのみやり取りし、
  内部状態を直接参照しない。
- `EyeTracker` の入出力仕様（IMU傾き→追従先座標の算出方法）は
  [spec/face-avatar/eye-tracking.md](../spec/face-avatar/eye-tracking.md) を参照。

関連ドキュメント: [処理フロー解説](processing-flow.md) / [描画方針](rendering.md) / [アセット管理](assets.md) / [既存コードの扱い](legacy-code.md) /
[顔アバター実装仕様](../spec/face-avatar/README.md)
