# 下部コントロールバー仕様

出典: [reference/microcontroller-face-ui-spec.md](reference/microcontroller-face-ui-spec.md) 6章、
[reference/spec-canvas.pdf](reference/spec-canvas.pdf) 03_UI CONTROLS & TELEMETRY

## 描画方式

ボタン枠・アイコンともに単純な線画のため、M5GFXのプリミティブ描画（角丸矩形＋線画アイコン）で
実装し、画像アセット化しない（[README.md](README.md) の使い分け方針。既存の `drawPowerIcon`
実装と同じ考え方）。

## 共通仕様

- 正方形のアイコンボタン3個、均等配置（`space-around` 相当）。
- サイズ: 各34×34px、角丸、シアン色（`COLOR_ACCENT`）の枠線。デフォルトは背景透過。
- 配置範囲: y=194〜228（高さ34px）。

| ボタン | 中心x座標 | アイコン | 機能 |
|---|---|---|---|
| 電源 | x=61 | 電源マーク | デバイスのON/OFF切り替え |
| Wi-Fi | x=160 | Wi-Fiマーク | 無線接続のON/OFF切り替え |
| 停止 | x=259 | 停止/ターゲットマーク | 緊急停止・動作の一時停止 |

## 状態表現

- 通常（非アクティブ）状態: 背景透過、シアン色の枠線＋アイコン。
- アクティブ/押下時: シアン色で塗りつぶし、アイコンは反転（暗色）。
  - この見た目は元資料でも推測扱い（「トグル」という表現からの推測）であり確定仕様ではない。
- 電源・Wi-Fiは状態を保持する「トグル」、停止は押下中のみ有効な「モーメンタリ」動作の
  ニュアンスがあるが、いずれも確定していない（[open-questions.md](open-questions.md) 参照）。
  実装は以下の暫定方針とする。
  - 電源: トグル。押下でOFF処理（`M5.Power.powerOff()` 等、既存 `src/main.cpp` の実装を参考）。
  - Wi-Fi: トグル。押下でWiFi ON/OFF（[docs/legacy-code.md](../../docs/legacy-code.md) に記載の
    `WifiScanner` モジュール切り出し時に接続する）。
  - 停止: モーメンタリ。押下中のみ「停止」状態を示し、離すと解除する。

## タッチ判定

- `M5.Touch.getDetail()` の座標と各ボタンの矩形範囲（中心x座標±17px、y=194〜228）を比較する。
- 既存 `src/main.cpp` の `isPowerIconHit` と同様のパターンで、ボタンごとに
  `isButtonHit(x, y, buttonCenterX)` 相当の判定関数を用意する。
