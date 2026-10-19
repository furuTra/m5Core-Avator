# m5Core-Avator

M5Stack Core2 の画面に、フルカラーのキャラクターの顔を表示するプロジェクト。
パーツごとに分けた透過PNGから顔を組み立て、**まばたき**と**視線**（傾けた方向・タッチした方向を見る）を付ける。

- 対象: M5Stack Core2（ESP32, 320x240）/ PlatformIO（Arduino framework）
- 主なライブラリ: M5Unified（M5GFX を含む）, ArduinoJson
- 言語: C++11（`gnu++11`）

## 素材は同梱していない

キャラクターの素材（PNG）は、このリポジトリに含めていない。キャラクターは著作物であるため、
自分の素材（または利用を許諾された素材）を用意して使う。
`assets/` は空で、利用者の素材は `.gitignore` で追跡対象から外してある（誤ってコミット・公開しないため）。

素材は、次の条件を満たす必要がある。

- 全パーツが、**同じサイズの正方形キャンバス**に、位置を保ったまま配置された透過PNGである。
- 視線で動かしたいパーツ（瞳・白目・眉など）が、**別々のPNG**に分かれている。
- まばたきをするなら、**閉じた目を表す瞼のパーツ**がある。
- 動かすパーツの動きは**平行移動だけ**で足りる（回転・拡大縮小は扱わない）。

## 使い方

```sh
# 1. 素材を assets/<アバター名>/ に置き、定義ファイル avatar.json の雛形を作る
python tools/init_avatar.py <アバター名>

# 2. assets/<アバター名>/avatar.json の config を編集する（重ね順・動かすグループ・視線の中立点など）

# 3. 縮小・計測して、デバイス用のデータを data/ に生成する
python tools/build_avatar.py <アバター名>

# 4. data/ をフラッシュの LittleFS 領域へ書き込む（素材や定義を変えたときだけ）
pio run -t uploadfs --upload-port <ポート>

# 5. ファーム本体を書き込む（コードを変えたときだけ）
pio run -t upload --upload-port <ポート>
```

`config` を変えたら、必ず `build_avatar.py` を再実行してから `uploadfs` する。
動かすパーツの可動量（`max`）は 0（動かない）から始まる。実機で見ながら、少しずつ上げる。

Python ツールには [Pillow](https://python-pillow.org/) が必要（`pip install pillow`）。

## ドキュメント

| ドキュメント | 内容 |
| --- | --- |
| [docs/overview.md](docs/overview.md) | プロジェクト概要 |
| [docs/rendering.md](docs/rendering.md) | 描画方針（画像アセット方式、表情切り替え・アニメーション） |
| [docs/processing-flow.md](docs/processing-flow.md) | 処理フロー（起動・毎フレーム処理・視線/描画の流れ） |
| [docs/avatar-definition.md](docs/avatar-definition.md) | アバター定義ファイル（avatar.json）の仕様 |
| [docs/build.md](docs/build.md) | ビルド・実行方法 |
| [tools/README.md](tools/README.md) | 素材の変換ツールの使い方 |
| [docs/coding-standards.md](docs/coding-standards.md) | C++コーディング規約 |

その他の設計メモは `docs/` と `spec/` を参照。`CLAUDE.md` は、AI コーディングアシスタント向けの作業指針。

## ライセンス

このリポジトリのソースコードとドキュメントは [MIT License](LICENSE) で公開している。
キャラクターの素材は含まれておらず、このライセンスの対象外である。
