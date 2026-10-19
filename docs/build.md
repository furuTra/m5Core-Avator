# ビルド・実行

- ビルド/書き込みは PlatformIO を使用する。環境は `platformio.ini` の `[env:m5stack-core2]` 一つ。
- シリアルモニタ速度は 115200bps。

## アセットはLittleFSに置く

画像アセットはデバイス内蔵フラッシュの LittleFS 領域から読む（SDカードは使わない）。
ファーム本体とは別に、一度 LittleFS イメージを書き込む必要がある。

```sh
# 1. アセット生成（素材・avatar.json を変えたとき、表示するアバターを差し替えるときに実行）
python tools/build_avatar.py avatar   # assets/avatar/ → data/assets/avatar/

# 2. LittleFSイメージをフラッシュへ書き込む（アセット・アバター変更時のみ）
pio run -t uploadfs --upload-port <ポート>

# 3. ファーム本体をビルド＆書き込み（コード変更時のみ）
pio run -t upload --upload-port <ポート>
```

ファーム本体はアバターに依存しないため、アバターの差し替えや可動量の調整は 1・2 だけで反映できる。

- LittleFSイメージ未書き込みのまま起動すると、マウント失敗として画面に `FS ERROR / Run uploadfs`
  を表示する。
- 定義ファイル `avatar.json` が無い・形式が対応外などで読めないと、画面に `DEF ERROR` と原因を表示する
  （`build_avatar.py` の再実行と uploadfs で直る）。
- 依存ライブラリは M5Unified と ArduinoJson（定義ファイルの読み込み用）。`platformio.ini` の `lib_deps`
  から自動で取得される。

生成ツールの詳細は [tools/README.md](../tools/README.md)、アセットは [assets.md](assets.md) を参照。
