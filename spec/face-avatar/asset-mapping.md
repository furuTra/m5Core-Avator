# 画像アセット構成（詳細）

[docs/assets.md](../../docs/assets.md) で定めた一般方針（LittleFS・PNG・`AssetLoader`
モジュールに集約）を、[expressions.md](expressions.md) / [eye-tracking.md](eye-tracking.md) の
仕様に基づいて具体化したファイル構成。

## ディレクトリ構成

```
/assets/faces/
  neutral/
    eye_socket.png     # 落ち着いた目の形（瞳は含まない。pupil_dot.png を重ねる）
    mouth.png           # 水平な直線
  happy/
    eye_socket.png     # 目の形＋上向きの弧（スクイント）。瞳は pupil_dot.png を重ねる
    mouth.png           # 上向きの曲線（笑顔）
  alert/
    eye_socket.png     # 大きく見開いた目の形（アウトライン）。瞳は pupil_ring.png を重ねる
    mouth.png           # 「O」型（円形アウトライン）
  sleep/
    eye_socket.png     # 水平な直線1本（瞳なし）
    # mouth.png は無し（口を描画しない）
  common/
    pupil_dot.png       # neutral/happy 用の塗りつぶし瞳（小サイズスプライト）
    pupil_ring.png       # alert 用のリング状（アウトライン）瞳（小サイズスプライト）
```

## 合成順序（Zオーダー）

1. 背景（`COLOR_BG` で塗りつぶし）
2. 外側/内側リング（プリミティブ描画。[expressions.md](expressions.md) 参照）
3. `eye_socket.png`（現在の表情状態、またはクロスフェード中は新旧2枚）
4. 瞳スプライト（`pupil_dot.png` / `pupil_ring.png`。`sleep` では描画しない）を
   [eye-tracking.md](eye-tracking.md) の変位ベクトル分オフセットして描画
5. `mouth.png`（現在の表情状態、またはクロスフェード中は新旧2枚。`sleep` では描画しない）
6. テレメトリヘッダー・区切り線・下部ボタン（すべてプリミティブ/テキスト描画。
   [telemetry-header.md](telemetry-header.md) / [control-buttons.md](control-buttons.md)）

## 読み込みポリシー

- `AssetLoader` は起動時に4状態分の `eye_socket.png` / `mouth.png` と、共通の瞳スプライト2種を
  まとめて読み込み、メモリ上に保持する（表情切り替えのたびにFSへアクセスすると遅延の原因になるため）。
- 画像サイズはリング内径（半径38px、直径76px）に収まる範囲で作成する。
