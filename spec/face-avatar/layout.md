# キャンバス・レイアウト仕様

出典: [reference/microcontroller-face-ui-spec.md](reference/microcontroller-face-ui-spec.md) 1〜2章・7章、
[reference/wireframe-annotated.svg](reference/wireframe-annotated.svg)

## キャンバスサイズ

- `320 x 240` px。M5Stack Core2 の物理ディスプレイ解像度と一致するため、スケーリング不要でそのまま
  ピクセル座標を利用できる。
- 全体パディング: 上下左右 `12px`。

## 縦方向の構成（上から下）

1. テレメトリヘッダーバー（[telemetry-header.md](telemetry-header.md)） — y=12〜32
2. アバターエリア（[expressions.md](expressions.md) / [eye-tracking.md](eye-tracking.md)） — 中心 y=108 付近、可変領域
3. 区切り線（本ページ下記） — y=184
4. 下部コントロールバー（[control-buttons.md](control-buttons.md)） — y=194〜228

`space-between` 相当のレイアウトのため、実装上は各セクションのY座標を固定値としてハードコードして
問題ない（可変長テキストなどによる再計算は不要）。

## カラートークン

M5GFX で使用する色は `RGB565` の `uint16_t` として定義する。`M5.Display.color565(r, g, b)` で
生成するか、定数化して `AssetLoader` または専用の `ColorPalette.h` にまとめる。

| トークン名 | 値（RGB888） | 用途 |
|---|---|---|
| `COLOR_BG` | `#0A0F14` | 画面背景 |
| `COLOR_BG_BORDER` | `#1C2830` | 外枠（ベゼル）の境界線 |
| `COLOR_ACCENT` | `#4DD9EC` | 全ての線・アイコン・文字（シアン単色UI） |
| `COLOR_ACCENT_DIM` | `#2A5A63` | アバター外側リング（副次的な強調） |
| `COLOR_DIVIDER` | `#2A3A42` | 点線区切り |

- デザイン言語: シアン単色 × ほぼ黒背景。グラデーション・影なしのフラットな線画。
- フォント: 等幅（monospace）。M5GFX の `setFont` で等幅フォントを指定する。

## 区切り線

- 水平の点線。x=12〜308、y=184、色は `COLOR_DIVIDER`。
- アバターエリアと下部コントロールバーの間を区切る。
- M5GFX にはネイティブの破線APIがないため、短い線分（例: 4px描画+4px空け）をループで描画する
  ヘルパーを用意する（`drawDashedLine(x0, y0, x1, y1, color)` 相当）。
