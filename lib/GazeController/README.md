# GazeController（視線制御パッケージ）

**追従先の点 `(x, y)` を受け取り、視線として動かすパーツのオフセット(px)を返す**だけのモジュール。
入力デバイス（IMU・タッチ・通信）にも描画方法にも依存しないので、このフォルダごと他プロジェクトへ
コピーすれば流用できる。依存は標準ライブラリのみ（Arduino / M5Unified を include しない）。

## 責務の境界

```
[入力] 点(x,y) を作る          →  [このパッケージ] 点 → 方向 → オフセット(px)  →  [描画] オフセットで描く
ImuInput / TouchInput / 通信       GazeController + GazeOffset                     FaceRenderer など
                                   （必要なら EyeTracker で点を調停してから渡す）
```

- **入力側**は「いまの点」を返すだけで、視線のことを知らない。
- **このパッケージ**は点がどこから来たかを知らず、描画方法も知らない。
- **描画側**は `GazeOffset::x()/y()` を自分のパーツ位置へ足すだけ。

## 使い方

```cpp
#include "GazeController.h"

GazeController gaze;
GazeOffset pupil;   // 動かしたいパーツごとに1つ持つ

void setup() {
    // 中立点(この点を向くと正面)と、方向が振り切る(±1)までの距離(px)
    gaze.begin(eyeCenterX, eyeCenterY, 160.0f, 120.0f);
    // 振り切ったときの変位(px) と 追従の速さ(0..1)
    pupil.begin(6.0f, 4.0f, 0.25f);
}

void loop() {
    // 入力側から追従先の点が来たときだけ呼ぶ（可動域の外・画面外の点でもクランプされる）
    gaze.lookAt(targetX, targetY);

    // 毎フレーム進める。描画に使う整数px が変わったときだけ true
    if (pupil.update(gaze.dirX(), gaze.dirY())) {
        drawPupil(baseX + pupil.x(), baseY + pupil.y());
    }
}
```

パーツを複数動かす場合は `GazeOffset` を並べ、同じ `dirX()/dirY()` を渡して可動量と追従の速さだけ
変える（本プロジェクトは瞳・目グループ・鼻口眼鏡の3つ。値は `FaceRenderer.cpp` 冒頭の定数）。
1つでも `update()` が `true` を返したら再描画する（短絡評価で更新を飛ばさないこと）。

## 組み合わせて使うモジュール（いずれも任意）

- `lib/EyeTracker/`: 複数の入力から追従先の点を1つ決める調停（タッチ優先・指を離した後の保持など）。
  これもデバイス非依存なので、入力が複数ある場合は一緒に持っていける。
- `lib/TouchInput/` `lib/ImuInput/`: M5Unified 依存。点の作り方はプロジェクトごとに差し替える前提。
