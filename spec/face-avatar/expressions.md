# 表情状態仕様（SYS.EXPR_DB）

出典: [reference/microcontroller-face-ui-spec.md](reference/microcontroller-face-ui-spec.md) 4〜5章、
[reference/spec-canvas.pdf](reference/spec-canvas.pdf) 01_EXPRESSION STATES

## アバターエリア共通仕様

- 中心座標: `(160, 108)`（キャンバス座標系）。
- 二重の同心円リング（[layout.md](layout.md) のカラートークン参照）:
  - 外側リング: `COLOR_ACCENT_DIM`、半径 `48px`
  - 内側リング: `COLOR_ACCENT`、半径 `38px`
  - これらのリングはM5GFXのプリミティブ（`drawCircle`）で描画し、画像アセット化しない
    （[README.md](README.md) の使い分け方針を参照）。
- 顔グラフィック（目・口）は内側リングの中に描画する。目の中心 y=100、口の位置 y=118 目安。

## 4つの表情状態

状態は常に1つだけアクティブ。`FaceExpression` enum として
`Neutral | Happy | Alert | Sleep` の4値を定義し、外部（`AnimationController` や将来の
API/コマンド受信処理）から設定可能にする。

| 状態 | 目 | 口 | 必要な画像パーツ |
|---|---|---|---|
| `Neutral` | 落ち着いた塗りつぶしの点状瞳2つ | 水平な直線 | `eye_socket_neutral.png`（瞳は共通パーツ `pupil_dot.png` を重ねる） / `mouth_neutral.png` |
| `Happy` | 同じ点状瞳 ＋ 各目の上に上向きの弧（スクイント） | 上向きの曲線（笑顔） | `eye_socket_happy.png`（瞳は `pupil_dot.png` を重ねる） / `mouth_happy.png` |
| `Alert` | 大きく見開いた状態。瞳は大きめの円形アウトラインの中に表示 | 「O」型（円形アウトライン） | `eye_socket_alert.png`（瞳は `pupil_ring.png` を重ねる） / `mouth_alert.png` |
| `Sleep` | 閉じた状態。各目を水平な直線1本で表現（瞳なし） | 描画しない（非表示） | `eye_socket_sleep.png`（瞳パーツは使わない） / なし |

パーツの具体的なファイル配置は [asset-mapping.md](asset-mapping.md) を参照。

## 状態遷移

- 遷移トリガー（タイマー/センサー/外部コマンドのどれか）は元資料に明記がないため、
  `FaceState` は外部から `setExpression(FaceExpression)` を呼べる形にし、トリガーの実装は
  呼び出し側（`main.cpp` や将来のコマンド受信モジュール）に委ねる。
- 状態が変化したら、[README.md](README.md) に記載の **クロスフェード方式** で120msかけて
  旧パーツ（アルファ 100%→0%）と新パーツ（アルファ 0%→100%）を重ねて描画する。
  - `AnimationController` が経過時間から現在のアルファ値を計算し、`FaceRenderer` に渡す。
  - 120ms を1フレームあたり約20msで描画すると仮定すると、遷移中は約6フレームの中間描画が発生する。
    ESP32での描画負荷を見て、フレーム数・間隔は実装時に調整してよい（本仕様が固定するのは
    「120ms」という遷移時間の目安のみ）。
- 瞳（`pupil_dot.png` / `pupil_ring.png`）は状態遷移のクロスフェードとは独立して、
  [eye-tracking.md](eye-tracking.md) の視線トラッキングに従って常時位置更新される。
