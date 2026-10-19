# アバター定義ファイル avatar.json の雛形（config）を、素材PNGの一覧から作る。
# 作成後、ユーザーが重ね順・動きグループ・視線の中立点・zoom などを編集し、build_avatar.py を実行する。
# 仕様は docs/avatar-definition.md を参照。
#
# 使い方: python tools/init_avatar.py <アバター名> [--force]
#   素材は assets/<アバター名>/*.png。既存の avatar.json は --force を付けたときだけ上書きする。
#
# パーツの構成・名前はアバターごとに異なるため、パーツ名から役割を推測することはしない。
# 雛形は全レイヤーを名前順・動かない（fixed）で並べるだけで、重ね順・グループ・中立点はユーザーが決める。
import argparse
import os
import sys

import avatar_common as common


def main():
    parser = argparse.ArgumentParser(description="avatar.json の雛形を作る")
    parser.add_argument("avatar", help="アバター名（assets/<アバター名>/ に素材PNGを置く）")
    parser.add_argument("--force", action="store_true", help="既存の avatar.json を上書きする")
    args = parser.parse_args()

    if not os.path.isdir(common.source_dir(args.avatar)):
        sys.exit(f"素材ディレクトリがありません: {common.source_dir(args.avatar)}")
    path = common.definition_path(args.avatar)
    if os.path.exists(path) and not args.force:
        sys.exit(f"既に存在します（上書きするには --force）: {os.path.relpath(path, common.ROOT)}")

    names = common.list_layer_pngs(args.avatar)
    if not names:
        sys.exit("素材PNGがありません")

    config = {
        "display": dict(common.DEFAULT_DISPLAY),
        "gazeCenter": {"from": ""},
        "gazeReach": list(common.DEFAULT_GAZE_REACH),
        "blink": dict(common.DEFAULT_BLINK),
        "alphaThreshold": common.DEFAULT_ALPHA_THRESHOLD,
        "layers": [{"name": name, "group": "fixed"} for name in names],
        "groups": {},
    }
    common.write_definition(path, {"config": config, "measured": {}})

    print(f"作成: {os.path.relpath(path, common.ROOT)}（レイヤー {len(names)} 枚）")
    print("  build_avatar.py の前に config を編集すること:")
    print("  - layers を重ね順（奥→手前）に並べ替える（雛形は名前順）")
    print("  - gazeCenter.from に視線の中立点にするレイヤー名を書く（必須）")
    print("  - 動かすパーツがあれば groups を定義し、layers の group に割り当てる")
    print("  - 目を閉じた顔でだけ表示するレイヤーには \"show\": \"closed\" を付ける")


if __name__ == "__main__":
    main()
