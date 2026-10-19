# tools/ — アセット生成スクリプト

マスター素材（`assets/<アバター名>/*.png`）とアバター定義ファイル `assets/<アバター名>/avatar.json` から、
デバイスが読む形（LittleFSに焼くファイル）を生成するビルド前ツール群。素材や定義ファイルを変えたとき、
表示するアバターを差し替えるときに実行する。仕様・各キーの説明は
[../docs/avatar-definition.md](../docs/avatar-definition.md)。

いずれも [Pillow](https://python-pillow.org/) が必要（`pip install pillow`）。
パスはスクリプト位置からの相対で解決するため、どのカレントディレクトリからでも実行できる。

| スクリプト | 出力 | 役割 |
| --- | --- | --- |
| `init_avatar.py <アバター名> [--force]` | `assets/<アバター名>/avatar.json` | 素材PNGの一覧から config の雛形を作る（全レイヤーを名前順・`fixed` で列挙）。パーツ名から役割は推測しないので、重ね順・動きグループ・視線の中立点はユーザーが編集する。 |
| `build_avatar.py <アバター名>` | `assets/<アバター名>/avatar.json` の measured<br>`data/assets/avatar/`（出力先は固定。縮小PNG・`stage_*.bin`・`avatar.json`）<br>`build/preview/<アバター名>.png` | config を検証し、素材を表示サイズへ縮小・計測して measured を書き込み、デバイス用データと確認用プレビューを出力する。 |
| `build_avatar.py <アバター名> --check` | なし | 素材や config が measured の生成後に変更されていないかを確認する（要再生成なら終了コード1）。 |
| `avatar_common.py` | — | 上記2本の共通処理（パス・定義ファイルの読み書き・既定値）。 |

画像データを**C++ソースにはしない**。デバイスは起動時に LittleFS 上の `avatar.json` を読み、
そこに書かれたPNG・`stage_*.bin` を使う。ファーム本体はアバターに依存しない。

## build_avatar.py の処理

1. `assets/<アバター名>/` の全PNGを読み、全て**同じサイズの正方形**であることを検証する。
2. 表示サイズ（画面の高さ × `zoom`）へ `LANCZOS` で縮小する。以降の計測・出力はすべてこの縮小画像から作る
   ため、計測した座標とデバイスが描く画素は一致する。
3. 視線の中立点・クリップ矩形・バンド・段を**計測**して `measured` に書く。
4. 動くグループごとに、同じグループのレイヤーを重ね順に1枚へ合成し（`Image.alpha_composite`）、
   不透明画素を含む矩形だけを切り出して、`stage_*.bin`（生ARGB）として書く。
5. 縮小したPNGと `avatar.json` のコピーを `data/assets/avatar/` へ書く。
6. 計測結果を素材に重ねた確認用プレビューを `build/preview/` へ書く（デバイスには送らない）。

生ARGBを作るのに必要な入力は、`assets/<アバター名>/` の**素材PNG群**と `avatar.json` の **config** の2つだけ。
config の `layers`（重ね順とグループ）と `groups`（動かし方）が、どのレイヤーを合成して1つの
`stage_*.bin` にするかを決める。

## avatar.json の項目

`avatar.json` は `config`（人が書く）と `measured`（`build_avatar.py` が書く。編集しない）からなる。
正本は [../docs/avatar-definition.md](../docs/avatar-definition.md)。ここでは要点だけを載せる。

座標の単位: `config` の px 値（`max`・`gazeReach`）は**画面px**（320×240、左上原点、Y は下向きが正）。
`measured` の位置はすべて**顔座標**（表示サイズの正方形の左上が原点。`画面座標 = 顔座標 + faceOrigin`）。

### config

| キー | 型 | 既定値 | 説明 |
| --- | --- | --- | --- |
| `display.zoom` | 数値 | `1.6` | 拡大率。1.0 で素材全体が画面の高さに収まる。1 を超えると上下左右が画面外へはみ出す。 |
| `display.background` | 文字列 | `"#0A0F14"` | 顔の外側の背景色。 |
| `gazeCenter.from` | 文字列 | — | 視線の中立点にするレイヤー名（不透明部分の中心）。この点を見ているとき全グループの移動量は 0。空だと build でエラー。 |
| `gazeReach` | `[X, Y]` 画面px | `[160, 120]` | 中立点から何px離れた点で目標方向を振り切る（±1）か。小さいほど少しの入力で大きく動く。 |
| `blink.intervalMs` | `[最小, 最大]` ms | `[2500, 6000]` | まばたきの間隔（この範囲でランダム）。 |
| `blink.closedMs` | 数値 ms | `180` | 目を閉じている時間。 |
| `alphaThreshold` | 0〜255 | `128` | 計測で「不透明」とみなすアルファ値。 |
| `layers[]` | 配列 | — | 全レイヤーを**配列順＝重ね順（先頭が最奥）**で並べる。 |
| `layers[].name` | 文字列 | — | 素材PNGのファイル名（拡張子なし）。 |
| `layers[].group` | 文字列 | `"fixed"` | 属する動きグループ名。`fixed` は動かない。同じグループは常に一緒に動く。 |
| `layers[].show` | `"always"` / `"closed"` | `"always"` | `closed` は目を閉じた顔を作るときだけ表示する（瞼など）。 |
| `groups.<名前>.max` | `[X, Y]` 画面px | `[0, 0]` | 目標方向を振り切ったときの最大移動量。`parent` がある場合は親に対する相対量。 |
| `groups.<名前>.smooth` | 0〜1 | `0.2` | 1フレームで目標までの残りの何割を詰めるか。大きいほど速く追従し、`1.0` で補間なし。 |
| `groups.<名前>.parent` | 文字列 | なし | 親グループ名。「親の移動量＋自分の移動量」の位置に描く。循環はエラー。 |
| `groups.<名前>.clip.layer` | 文字列 | なし | はみ出し防止の描画範囲の基準にするレイヤー名。省略するとクリップしない。 |
| `groups.<名前>.clip.mode` | `"bbox"` / `"components"` | `"bbox"` | `bbox`: 不透明部分全体を囲む矩形1つ。`components`: 透明な隙間で分かれた領域ごとの矩形（左右の白目など）。 |

### measured

| キー | 説明 | 求め方 |
| --- | --- | --- |
| `schemaVersion` | 形式のバージョン。デバイスは対応外を拒否する。 | 固定値 |
| `avatar` | アバター名 | `build_avatar.py` の引数 |
| `sourceHash` | 生成に使った入力のハッシュ（`--check` が比較する） | 素材PNGと config から計算 |
| `canvas.sourceSize` | 素材PNGの辺長 | 素材から取得 |
| `canvas.faceSize` | 表示時の顔の辺長 | 240 × `zoom` |
| `canvas.faceOrigin` | 顔座標の原点の画面上の位置 | `(画面サイズ − faceSize) / 2` |
| `gazeCenter` | 視線の中立点（顔座標） | `gazeCenter.from` のレイヤーの不透明bboxの中心 |
| `groups.<名前>.totalMax` | 親をたどって合算した最大移動量 | `max` の合計 |
| `groups.<名前>.clipRects` | クリップ矩形の配列（顔座標、移動量0での位置） | `clip.layer` と `clip.mode` から計測 |
| `band` | 視線変化時に再合成・転送する矩形 | 各 `moving` 段の `rect` を `totalMax` 分広げ、すべてを囲む矩形 |
| `stages[]` | バンド内の描画段（奥→手前）。`kind` は `below` / `moving` / `overlay` | 重ね順とグループから自動生成 |
| `stages[].file` / `rect` | 段の生ARGBファイル名と、その切り出し矩形（顔座標） | `moving` / `overlay` のみ |
| `faces.open` / `faces.closed` | 起動時に1回合成する全画面の顔 | `open` は `show: "always"`、`closed` は全レイヤー |
| `faces.closed.offsets` | 閉じ顔でずらして描くレイヤーと量 | `show: "closed"` のレイヤーを目の下端まで下げる |

- 動かすグループが無い場合、`band` と `stages` は空になり、まばたきだけを行う。
- `max` は雛形では `[0, 0]`（動かない）。適切な値は実機で見ないと決められないため、少しずつ上げる。
  ツールは可動量の妥当性を判定しない。

## ディレクトリ構造

```
Avator/
├── assets/
│   └── <アバター名>/               マスター素材（git管理）
│       ├── <レイヤー名>.png         全パーツを同一サイズの正方形キャンバスに配置した透過PNG（1パーツ1ファイル）
│       └── avatar.json             定義ファイル（config は人が書き、measured はツールが書く）
├── tools/
│   ├── init_avatar.py              雛形を作る
│   ├── build_avatar.py             縮小・計測・生ARGB生成
│   └── avatar_common.py
├── data/                           build_avatar.py の出力（手編集しない）
│   └── assets/avatar/              アバター名によらず固定。uploadfs で LittleFS へ書く
│       ├── avatar.json             定義ファイルのコピー（デバイスが起動時に読む）
│       ├── <レイヤー名>.png         表示サイズへ縮小したPNG
│       └── stage_<番号>_<グループ>.bin   動くグループの生ARGB
├── build/preview/<アバター名>.png   確認用画像（git管理外）
└── src/ , lib/                     ファームウェア（アバターに依存しない）
```

- `data/assets/avatar/` へ書く際、前回 build した出力（前回の `avatar.json` に記録されたPNGと `stage_*.bin`）
  を先に消す。それ以外のファイルには触れない。

## 作業の流れ

```
init_avatar.py <名前>        assets/<名前>/avatar.json（config の雛形）を作る
        ↓
config を編集                 重ね順・動かすグループ・max・zoom などを人が書く
        ↓
build_avatar.py <名前>       縮小・計測して measured を書き、data/ へ生ARGBなどを出力
        ↓
pio run -t uploadfs          data/ を LittleFS へ書き込む
        ↓
実機で確認して max を調整     build_avatar.py からやり直す
```

**config を変えたら必ず `build_avatar.py` を再実行する。** `measured`（バンドの大きさや `.bin` の内容）は
config から導出されるため、再実行しないとデバイスのデータと食い違う。`smooth` のように `measured` に
影響しない値でも、デバイスは `data/` へコピーされた定義ファイルを読むため同様。
再生成し忘れは `build_avatar.py <アバター名> --check` で検出できる（素材PNGと config から計算した
`sourceHash` を比較する）。

## 実行例

```sh
# プロジェクトルートで
python tools/init_avatar.py avatar    # 雛形を作る → avatar.json の config を編集
python tools/build_avatar.py avatar   # 計測・生成 → uploadfs して実機で確認し、max・smooth を調整
```

## LittleFSへの書き込み

生成した `data/` をフラッシュのLittleFS領域へ書き込む（ファーム本体とは別手順）。
アバターの差し替え・調整だけならファーム本体の書き込みは不要。

```sh
pio run -t uploadfs --upload-port <ポート>  # data/ をLittleFS領域へ（アセット・アバター変更時）
pio run -t upload   --upload-port <ポート>  # ファーム本体（コード変更時）
```

生成物の使われ方と描画アーキテクチャは [../docs/rendering.md](../docs/rendering.md)、
ビルド手順は [../docs/build.md](../docs/build.md) を参照。
