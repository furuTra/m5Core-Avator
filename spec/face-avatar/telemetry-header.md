# テレメトリヘッダー仕様（SYS.UI_REF）

出典: [reference/microcontroller-face-ui-spec.md](reference/microcontroller-face-ui-spec.md) 3章、
[reference/spec-canvas.pdf](reference/spec-canvas.pdf) 03_UI CONTROLS & TELEMETRY

## 描画方式

ヘッダーは文字列とアイコンのみで構成されるため、M5GFXのテキスト描画・プリミティブ描画で実装し、
画像アセット化はしない（[README.md](README.md) の使い分け方針）。

## 位置・スタイル

- 位置: x=12〜308、y=12〜32（キャンバス座標）。
- フォント: 等幅（monospace）、サイズ12px、字間はやや広め（目安 0.5px。M5GFXでは
  `setTextSize`/カスタムフォントの字間調整、または1文字ずつ描画してオフセットを加える方法で近似する）。
- 色: `COLOR_ACCENT`（[layout.md](layout.md) 参照）。

## 左側: システムステータス文字列

- `SYS.OK_` = 正常動作を示す。末尾のアンダースコアも文字列の一部。
- カーソルのように点滅する可能性がある（明記なし、[open-questions.md](open-questions.md) 参照）。
  初期実装では常時表示（点滅なし）とし、点滅要否は後日確認する。
- `SYS.OK_` 以外のステータスコードの有無は元資料に明記がない。実装は `SYS.<CODE>_` という
  文字列フォーマットの表示関数（例: `drawSystemStatus(const char* code)`）として汎用化しておき、
  `OK` 以外のコードが決まり次第差し替えられるようにする。

## 右側: バッテリー表示

- 数値パーセンテージ（`0`〜`100` の整数）＋ バッテリーアイコン。
- M5Unified の `M5.Power.getBatteryLevel()` から取得した値を表示する。
- アイコンの見た目は「満充電（フル）」のみ元資料に明記されている。低下・充電中などの
  バリエーションは [open-questions.md](open-questions.md) の確認事項とする。
- アイコンは単純な線画（矩形＋端子の小突起）のため、`src/main.cpp` の既存 `drawPowerIcon`
  と同様にプリミティブ図形で描画する。
