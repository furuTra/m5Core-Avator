# アセット管理

- 画像アセットはデバイス内蔵フラッシュの **LittleFS 領域**から実ファイルとして読み込む
  （画素をC++ソースへ配列化しない／SDカードは使わない）。読み込み・パス解決は `AssetLoader` に集約する。
- フォーマットは **PNG（透過対応）を基本**とする。パーツ画像を背景に自然に重ねるため、
  アルファチャンネルを活用する。加えて、視線に合わせて動く段は生ARGBバイナリ `stage_*.bin`
  （PNGデコードを避けるための事前展開データ。詳細は [rendering.md](rendering.md)）。
- アバター固有の情報（パーツ構成・重ね順・動かし方・計測値）はアバター定義ファイル `avatar.json` に
  集約する。仕様は [avatar-definition.md](avatar-definition.md)。
- LittleFS ルートからのディレクトリ構成（表示するアバターによらず固定）:

```
/assets/avatar/
    avatar.json                    ← アバター定義ファイル（起動時に読む）
    <レイヤー名>.png               ← 表示サイズへ縮小したパーツPNG（現在のアバター: Ribbon, FaceBase,
                                     EyeBase, Eye, EyeLid, EyeBrow, Nose, Mouth, Glasses, Hair, Otaman, Aho）
    stage_<番号>_<グループ>.bin    ← バンド内で重ねる段の生ARGB(0xAARRGGBB)
```

- **マスター素材**は `assets/<アバター名>/` に置いた、全パーツが **同一サイズの正方形キャンバス**上に
  配置された透過PNG（全パーツを同じ倍率・同じ原点で重ねると位置が揃う。現在のアバターは 750x750）。
  同じディレクトリに定義ファイル `avatar.json` を置く。これを源泉に、`tools/build_avatar.py` が
  書き込み用アセットを `data/assets/avatar/` へ生成する。
- 生成した `data/` を `pio run -t uploadfs` でフラッシュのLittleFS領域へ書き込む。`data/` 配下は
  自動生成物のため手編集しない（[tools/README.md](../tools/README.md)）。
- アセットの読み込み・パス解決は `AssetLoader` モジュールに集約し、他のモジュールから
  ファイルパスを直接扱わせない。

合成順序（Zオーダー）は各アバターの `avatar.json` の `config.layers` を参照。

> **注**: 旧仕様の `/assets/faces/<表情名>/`（シアン単色UIの表情別パーツ）は、フルカラー
> アバターへの全面移行に伴い廃止した。表情別パーツの再設計は
> [spec/face-avatar/](../spec/face-avatar/README.md) の再設計待ち項目。

関連ドキュメント: [描画方針](rendering.md) / [アーキテクチャ](architecture.md) / [アバター定義ファイル](avatar-definition.md)
