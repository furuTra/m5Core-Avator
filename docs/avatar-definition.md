# アバター定義ファイル（avatar.json）

> **状態: 実装済み。** 生成ツールは `tools/init_avatar.py` / `tools/build_avatar.py`、デバイス側は
> `lib/AvatarDefinition/`（読み込み）と `lib/FaceRenderer/`（定義に従った描画）。旧方式の `tools/gen_*.py`
> と生成ヘッダは削除した。

他のアバターにも差し替えられるよう、アバター固有の情報（パーツ構成・Z順・動かし方・計測値）を
アバターごとの定義ファイル `avatar.json` に集約する。デバイス側のコードと生成ツールは、
特定のパーツ名や座標を持たない。

## 作業の流れ

```
1. init_avatar.py を実行        → assets/<アバター名>/avatar.json（config の雛形）を作る
2. ユーザーが config を編集      → 重ね順・動かすパーツとグループ・zoom など
3. build_avatar.py を実行       → 縮小・計測して measured を書き込み、data/assets/avatar/ へデバイス用データを出力
4. uploadfs して実機で確認      → max・smooth を調整して 3 に戻る
```

- `max`（可動量）は雛形では `[0, 0]`（動かない）。適切な値は実機で見ないと決められないため、
  実機で確認しながら少しずつ上げていく。ツールは可動量の妥当性を判定しない。
- `config` を変えたら、必ず `build_avatar.py` を再実行してから uploadfs する。デバイスは `data/` へ
  コピーされた定義ファイルを読むため、`smooth` のように `measured` に影響しない値の変更でも同様。

## ファイル構成

```
assets/<アバター名>/            ← マスター素材（git 管理）
    *.png                        全パーツを同一サイズの正方形キャンバス上に配置した透過PNG
    avatar.json                  定義ファイル（config はユーザー、measured はツールが書く）
data/assets/avatar/             ← build_avatar.py の出力（手編集しない。uploadfs で LittleFS へ）
    avatar.json                  最後に build したアバターの定義ファイルのコピー
    <レイヤー名>.png             表示サイズへ縮小したPNG（起動時の全画面合成・バンド下地に使う）
    stage_<番号>_<グループ>.bin  バンド内で重ねる段の生ARGB（0xAARRGGBB, リトルエンディアン）
build/preview/<アバター名>.png  ← 計測結果を素材に重ねた確認用画像（デバイスへは送らない）
```

- `data/assets/avatar/` はアバター名によらず固定（「デバイス側の読み込み方針」を参照）。
  build のたびに、前回 build したアバターの出力（前回の `avatar.json` に記録されたPNGと
  `stage_*.bin`）を消してから書き出す。それ以外のファイルには触れない。

## 座標と単位

- **画面px**: M5 ディスプレイ座標（320×240、左上原点、Y は下向きが正）。
- **顔座標**: 表示サイズ（`faceSize` = 画面の高さ240 × `zoom`）の正方形の左上を原点とする座標。
  単位は画面pxと同じで、`画面座標 = 顔座標 + faceOrigin`。`measured` の位置はすべて顔座標で持つ。
- `config` に書く px 値（`max`・`gazeReach`）はすべて**画面px**。素材PNGの解像度には依存しない。
  そのため `zoom` を変えると、同じ `max` でも顔の大きさに対する動きの割合が変わる（再調整が必要）。
- 計測は、素材PNGを `faceSize` へ縮小した画像で行う。縮小は `build_avatar.py` の中でメモリ上に行い、
  同じ縮小画像から出力も作るため、計測した座標とデバイスが描く画素は一致する。

## 視線の動きの仕組み

```
追従先の点(x,y)  ──①──▶  目標方向 dir (-1..1)  ──②──▶  グループごとの移動量(px)  ──③──▶  描画
 (タッチ・傾き)        gazeCenter と gazeReach で正規化    max と smooth で決まる          parent と clip を反映
```

1. 中立点（`gazeCenter`）から見た追従先の向きと距離を、X・Y それぞれ -1〜1 に正規化する。
2. 目標方向を、グループごとの移動量（px）へ変換し、毎フレーム滑らかに近づける。
3. 親グループの移動量を足し、クリップ範囲に制限して描く。

## config（ユーザーが書く）

### 全体

| キー | 型 | 既定値 | 説明 |
|---|---|---|---|
| `display.zoom` | 数値 | `1.6` | アバターの拡大率。1.0 で素材全体が画面の高さに収まる。1 を超えると上下左右が画面外へはみ出す。 |
| `display.background` | 文字列 | `"#0A0F14"` | 顔の外側の背景色。 |
| `gazeCenter.from` | 文字列 | — | 視線の中立点にするレイヤー名。そのレイヤーの不透明部分の中心が中立点になる。この点を見ているとき、全グループの移動量は 0（正面）。 |
| `gazeReach` | `[X, Y]` 画面px | `[160, 120]` | 中立点から何px離れた点を見たら目標方向を振り切る（±1）か。小さいほど、少しの入力で大きく動く。 |
| `blink.intervalMs` | `[最小, 最大]` ms | `[2500, 6000]` | まばたきの間隔。この範囲でランダムに決める。 |
| `blink.closedMs` | 数値 ms | `180` | 目を閉じている時間。 |
| `alphaThreshold` | 0〜255 | `128` | 計測で「不透明」とみなすアルファ値。輪郭がぼけた素材で調整する。 |

### `layers`：レイヤー一覧

素材のすべてのレイヤーを、**配列の順番＝重ね順（先頭が一番奥、末尾が一番手前）**で並べる。

| キー | 型 | 既定値 | 説明 |
|---|---|---|---|
| `name` | 文字列 | — | 素材PNGのファイル名（拡張子なし）。`assets/<アバター名>/<name>.png` を指す。 |
| `group` | 文字列 | `"fixed"` | 属する動きグループの名前（`groups` のキー）。`"fixed"` は視線に関係なく動かないことを表す。同じグループのレイヤーは、常に同じ量だけ一緒に動く。 |
| `show` | `"always"` / `"closed"` | `"always"` | `"closed"` は目を閉じた顔を作るときだけ表示する（瞼など）。 |

動くパーツより手前にある `fixed` レイヤー（髪など）は、動くパーツの上に重ね直されるため、重ね順は崩れない。

### `groups`：動きグループ

キー（例: `"eyes"`）がグループ名。名前は自由で、`layers[].group` から参照する。

| キー | 型 | 既定値 | 説明 |
|---|---|---|---|
| `max` | `[X, Y]` 画面px | `[0, 0]` | 目標方向を振り切ったとき（±1）に、中立位置から最大で何pxずれるか。左右・上下とも対称で、中間の方向では比例する。`parent` がある場合は親に対する相対的な移動量。 |
| `smooth` | 0〜1 | `0.2` | 1フレームごとに、目標位置までの残りの距離の何割を詰めるか。大きいほど速く追従し、`1.0` で補間なし。目安（約16ms/フレーム）: 0.25 で約130ms、0.2 で約170ms、0.18 で約190ms で目標の9割に届く。 |
| `parent` | 文字列 | なし | 親グループ名。指定すると「親の移動量＋自分の移動量」の位置に描く（例: 瞳の親を白目のグループにすると、瞳は白目ごと動きつつ白目の中でも動く）。循環（A→B→A）はエラー。 |
| `clip.layer` | 文字列 | なし | はみ出し防止の描画範囲の基準にするレイヤー名。そのレイヤーの不透明部分の形から範囲を求める。省略するとクリップしない。 |
| `clip.mode` | `"bbox"` / `"components"` | `"bbox"` | `"bbox"`: 不透明部分全体を囲む矩形1つ。`"components"`: 透明な隙間で分かれた領域ごとの矩形（左右の白目など）。 |

クリップ範囲は、そのグループの**親の移動量**分だけ一緒にずれる（白目が動けば、瞳のクリップ範囲も追従する）。

### init_avatar.py が作る雛形

パーツの構成・名前はアバターごとに異なるため、ツールはパーツ名から役割（重ね順・グループ・瞼など）を
推測しない。雛形は次の内容だけを持ち、残りはユーザーが編集する。

- `layers`: `assets/<アバター名>/` の全PNGを**名前順**・`group: "fixed"` で列挙する。
  重ね順（奥→手前）への並べ替えはユーザーが行う。
- `groups`: 空。動かすパーツがあれば、ユーザーがグループを定義して `layers[].group` に割り当てる。
  新しく定義したグループの `max` は、実機で確認しながら `[0, 0]` から上げていく。
- `gazeCenter.from`: 空。ユーザーが視線の中立点にするレイヤー名を書く（空のままだと build でエラー）。
- `show: "closed"`: 付けない。目を閉じた顔でだけ表示するレイヤーには、ユーザーが付ける。
- `display` / `gazeReach` / `blink` / `alphaThreshold`: 既定値。
- 既存の `avatar.json` は `--force` を付けたときだけ上書きする。

## measured（build_avatar.py が書く）

ユーザーは編集しない。デバイスは `config` と `measured` を合わせて読むため、描画に必要な値は
すべてここに含める。

| キー | 説明 | 求め方 |
|---|---|---|
| `schemaVersion` | 定義ファイル形式のバージョン | 固定値。形式を変えたら上げる。デバイスは対応外のバージョンを拒否する。 |
| `avatar` | アバター名 | `build_avatar.py` の引数。デバイスが起動ログなどで、どのアバターを読んだかを示すのに使う。 |
| `sourceHash` | 生成に使った入力のハッシュ | 素材PNGと `config` から計算する。素材だけ差し替えて再生成し忘れた状態をツールが警告できる。時刻は含めない（同じ入力からは同じ内容が出る）。 |
| `canvas.sourceSize` | 素材PNGの辺長 | 素材から取得。全PNGが同じサイズの正方形でなければエラー。 |
| `canvas.faceSize` | 表示時の顔の辺長 | 240 × `zoom` |
| `canvas.faceOrigin` | 顔座標の原点の画面上の位置 | `(画面サイズ − faceSize) / 2` |
| `gazeCenter` | 視線の中立点（顔座標） | `config.gazeCenter.from` のレイヤーの、しきい値bboxの中心 |
| `groups.<名前>.totalMax` | 親子関係をたどって合算した最大移動量 | `max` の合計。バンドの余白に使う |
| `groups.<名前>.clipRects` | クリップ矩形の配列（顔座標、移動量0での位置） | `clip.layer` と `clip.mode` から計測。`clip` が無ければ空配列 |
| `band` | 視線の変化時に再合成・転送する矩形（顔座標） | 各 `moving` 段の `rect` を、そのグループの `totalMax` 分だけ上下左右に広げ、すべてを囲む矩形 |
| `stages` | バンド内の描画段（奥→手前） | 下記「段の組み立て」 |
| `faces.open` / `faces.closed` | 起動時に1回だけ合成する全画面の顔 | `open` は `show: "always"` のレイヤー、`closed` は全レイヤーを重ね順に並べる |
| `faces.closed.offsets` | 閉じ顔でずらして描くレイヤーと量 | `show: "closed"` のレイヤーを「目（`gazeCenter.from` のレイヤー）の下端 − そのレイヤーの下端 + 3px」だけ下げる |

### 矩形の取り方

矩形は `{ "x", "y", "w", "h" }`（顔座標）で表す。用途によって不透明部分の取り方を分ける。

- **切り出し用**（`stages[].rect`）: アルファが 1 以上の画素をすべて含む矩形。縮小でできた
  半透明のにじみも欠けないようにする。
- **クリップ・中心用**（`clipRects`・`gazeCenter`・`faces.closed.offsets`）: アルファが
  `alphaThreshold` 以上の画素を囲む矩形。見た目の輪郭に合わせる。
- `components` の分割: しきい値以上の画素が1つも無い列で区切り、区切られた範囲ごとに矩形を取る。

### 段の組み立て

`layers` のうち `show: "always"` のものを重ね順にたどり、次の規則で `stages` を作る。

| kind | 対象 | 実行時の扱い |
|---|---|---|
| `below` | 最初の動くレイヤーより奥の `fixed` レイヤー | 起動時にPNGから不透明の16bppで一度だけ焼き、バンドの下地にする。 |
| `moving` | 同じグループが連続する範囲 | `.bin` を「`rect` の位置＋そのグループの現在の移動量（親の分を含む）」に、`clipRects` でクリップして重ねる。 |
| `overlay` | 最初の動くレイヤー以降にある `fixed` レイヤー | 固定位置で重ね直し、重ね順を元に戻す。`rect` は `band` との共通部分。 |

- **同じグループの段をまとめる最適化**: 同じグループの2つの段のうち、手前の段が間にあるどの段とも
  重ならない（各段の `rect` を `totalMax` 分広げた範囲が交差しない）場合は、手前の段を奥の段の位置まで
  下げて1つの段にまとめる。
  例: 現在の素材では EyeBase と EyeBrow の間に瞳があるが、眉と瞳は重ならないため `eyes` は1段になる。
  `max` を大きくすると重なるようになり、段が分かれることがある（例: `eyes` の max Y を 20 にすると
  EyeBase と EyeBrow が別の段になる。自動で判定する）。
- 動くレイヤーの間に挟まっている `fixed` レイヤーも、その位置のまま `overlay` 段として重ねる
  （`overlay` は末尾に限らず、途中に複数できることがある）。
- 動くグループが1つも無い場合、`band` と `stages` は空になり、まばたきだけを行う。

## 例（現在のアバター）

`measured` の値は、現在の素材を実際に計測したもの。ただし `max` は雛形の `[0, 0]` ではなく、
現行コードの調整済みの値を入れた場合の結果。

```json
{
  "config": {
    "display": { "zoom": 1.6, "background": "#0A0F14" },
    "gazeCenter": { "from": "EyeBase" },
    "gazeReach": [160, 120],
    "blink": { "intervalMs": [2500, 6000], "closedMs": 180 },
    "alphaThreshold": 128,
    "layers": [
      { "name": "Ribbon",   "group": "fixed" },
      { "name": "FaceBase", "group": "fixed" },
      { "name": "EyeBase",  "group": "eyes" },
      { "name": "Eye",      "group": "pupil" },
      { "name": "EyeLid",   "group": "fixed", "show": "closed" },
      { "name": "EyeBrow",  "group": "eyes" },
      { "name": "Nose",     "group": "detail" },
      { "name": "Mouth",    "group": "detail" },
      { "name": "Glasses",  "group": "detail" },
      { "name": "Hair",     "group": "fixed" },
      { "name": "Otaman",   "group": "fixed" },
      { "name": "Aho",      "group": "fixed" }
    ],
    "groups": {
      "eyes":   { "max": [8, 5], "smooth": 0.2,
                  "clip": { "layer": "FaceBase", "mode": "bbox" } },
      "pupil":  { "max": [6, 4], "smooth": 0.25, "parent": "eyes",
                  "clip": { "layer": "EyeBase", "mode": "components" } },
      "detail": { "max": [4, 3], "smooth": 0.18 }
    }
  },
  "measured": {
    "schemaVersion": 1,
    "avatar": "avatar",
    "sourceHash": "（入力から計算した値）",
    "canvas": { "sourceSize": 750, "faceSize": 384, "faceOrigin": { "x": -32, "y": -72 } },
    "gazeCenter": { "x": 193.5, "y": 232.5 },
    "groups": {
      "eyes":   { "totalMax": [8, 5],
                  "clipRects": [ { "x": 115, "y": 138, "w": 161, "h": 155 } ] },
      "pupil":  { "totalMax": [14, 9],
                  "clipRects": [ { "x": 119, "y": 213, "w": 57, "h": 38 },
                                 { "x": 212, "y": 213, "w": 56, "h": 39 } ] },
      "detail": { "totalMax": [4, 3], "clipRects": [] }
    },
    "band": { "x": 106, "y": 183, "w": 178, "h": 102 },
    "stages": [
      { "kind": "below", "layers": ["Ribbon", "FaceBase"] },
      { "kind": "moving", "group": "eyes", "layers": ["EyeBase", "EyeBrow"],
        "file": "stage_1_eyes.bin", "rect": { "x": 115, "y": 188, "w": 157, "h": 67 } },
      { "kind": "moving", "group": "pupil", "layers": ["Eye"],
        "file": "stage_2_pupil.bin", "rect": { "x": 143, "y": 222, "w": 104, "h": 32 } },
      { "kind": "moving", "group": "detail", "layers": ["Nose", "Mouth", "Glasses"],
        "file": "stage_3_detail.bin", "rect": { "x": 110, "y": 226, "w": 170, "h": 56 } },
      { "kind": "overlay", "layers": ["Hair", "Otaman", "Aho"],
        "file": "stage_4_overlay.bin", "rect": { "x": 106, "y": 183, "w": 178, "h": 102 } }
    ],
    "faces": {
      "open":   { "layers": ["Ribbon", "FaceBase", "EyeBase", "Eye", "EyeBrow", "Nose",
                             "Mouth", "Glasses", "Hair", "Otaman", "Aho"] },
      "closed": { "layers": ["Ribbon", "FaceBase", "EyeBase", "Eye", "EyeLid", "EyeBrow", "Nose",
                             "Mouth", "Glasses", "Hair", "Otaman", "Aho"],
                  "offsets": { "EyeLid": { "x": 0, "y": 20 } } }
    }
  }
}
```

## 旧実装との対応と差分

移行前（`FaceRenderer.cpp` の定数と `tools/gen_*.py` で1体のアバターに固定していた実装）との対応。

| 旧実装（`FaceRenderer.cpp` ほか） | 定義ファイル |
|---|---|
| `BASE_LAYERS` / `CLOSED_LAYERS` / `BELOW_LAYERS` | `config.layers` → `measured.faces` / `measured.stages` |
| `GAZE_MAX_*` / `GROUP_MAX_*` / `DETAIL_MAX_*`、`*_SMOOTH` | `config.groups.*.max` / `smooth` |
| `GAZE_REACH_*`、`ZOOM`、`COLOR_BG`、`BLINK_*` | `config.gazeReach` / `display` / `blink` |
| `EYE_CENTER_*`、`FACE_CLIP_*`、`EYE_RECTS`、`EYELID_DROP` | `measured.gazeCenter` / `groups.*.clipRects` / `faces.closed.offsets` |
| `EyeGroupAsset.h`、`PupilAsset.h`（生成ヘッダ） | `measured.band` / `stages[].rect`（ヘッダは廃止し、再ビルド不要にする） |
| `gen_littlefs.py` / `gen_pupil.py` / `gen_eye_group.py` | `build_avatar.py` に統合 |

- `.bin` をバンド全体の大きさではなく各段の範囲だけにするため、PSRAM 使用量は約325KB から約162KB に減る（現在の素材での実測）。
- バンドは全方向一律14pxの余白（198×122）から、グループごとの可動量分の余白（178×102）になる。
- 750pxで測った値を換算・切り捨てていた旧実装の定数に比べ、一部の座標が1〜2pxずれる
  （例: 目の中心が画面上で (161,159) から (161.5,160.5)）。見た目への影響は実機で確認する。

## デバイス側の読み込み方針（決定事項）

- **アバターの選択はビルド時に行う。** 表示するアバターは `build_avatar.py <アバター名>` で `data/` へ
  出力したものになる。出力先はアバター名によらず `data/assets/avatar/` に固定で、デバイスは起動時に
  常に `/assets/avatar/avatar.json` を読む。
  - 差し替えは `build_avatar.py <アバター名>` → `pio run -t uploadfs` だけで、ファーム本体の
    ビルド・書き込みは不要（ファームはアバターに依存しない）。
  - 複数のアバターを同時に LittleFS へ置かないため、使わないアバターのデータでフラッシュを消費しない。
- **定義ファイルは ArduinoJson で起動時に1回だけ読み**、内容を構造体へ写してから JSON のデータは
  解放する。`measured.schemaVersion` が対応外、または必要なキーが欠けている場合は、起動時に画面へ
  エラーを表示する（現行の「FS ERROR」と同様）。
  - build 時に C++ ヘッダを生成する方式は採らない。差し替えのたびにファームの書き込みも必要になり、
    ヘッダ（ファーム）と画像（LittleFS）の組み合わせが食い違うと表示が崩れるため。

## デバイス側のモジュール構成

| モジュール | 責務 |
|---|---|
| `AvatarDefinition`（`lib/AvatarDefinition/`） | `avatar.json` を読み、グループ・段・全画面の顔・まばたき設定などを構造体へ写す。`config` で省略されたキーには、上記「既定値」と同じ値を補う。描画方法には関知しない。 |
| `FaceRenderer`（`lib/FaceRenderer/`） | `AvatarDefinition` の内容に従って、開き顔・閉じ顔・バンドの下地を起動時に合成し、実行時は段の一覧を順に重ねる。グループごとに `GazeOffset` を1つ持ち、親をたどって移動量を合算する。特定のパーツ名や座標は持たない。 |
| `AssetLoader`（`lib/AssetLoader/`） | LittleFS のマウントと、`/assets/avatar/` 以下のパス解決（`definitionPath()` / `pngPath()` / `binPath()`）。 |
| `main.cpp` | 起動時に `AvatarDefinition::load()` を呼び、失敗したら `DEF ERROR` と原因を画面に表示する。 |

関連ドキュメント: [アセット管理](assets.md) / [描画方針](rendering.md) / [アーキテクチャ](architecture.md)
