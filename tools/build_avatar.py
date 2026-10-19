# アバター定義ファイル avatar.json の config を読み、素材PNGを表示サイズへ縮小・計測して measured を
# 書き込み、デバイス用のデータ（縮小PNG・各段の生ARGB・定義ファイルのコピー）を data/ へ出力する。
# 仕様は docs/avatar-definition.md を参照。
#
# 使い方: python tools/build_avatar.py <アバター名> [--check]
#   --check … 生成はせず、measured.sourceHash が現在の素材・config と一致するかだけを確認する
#             （素材や config を変えた後の再生成忘れの検出用。不一致なら終了コード1）。
import argparse
import glob
import hashlib
import json
import os
import sys

from PIL import Image, ImageDraw

import avatar_common as common

SCHEMA_VERSION = 1

# M5Stack Core2 / CoreS3 の画面サイズ（px）。faceSize = SCREEN_H × zoom。
SCREEN_W = 320
SCREEN_H = 240

# 閉じ顔で下げるレイヤー（show: closed）が、gazeCenter.from のレイヤーの下端をこの分だけ越えるようにする。
CLOSED_DROP_PADDING = 3

OVERLAY = "overlay"  # overlay 段のファイル名に使う名前
FIXED = "fixed"


class ConfigError(Exception):
    pass


# ---- 矩形ユーティリティ（(x0, y0, x1, y1)、終端は含まない） ----

def rect_union(a, b):
    if a is None:
        return b
    if b is None:
        return a
    return (min(a[0], b[0]), min(a[1], b[1]), max(a[2], b[2]), max(a[3], b[3]))


def rect_intersect(a, b):
    r = (max(a[0], b[0]), max(a[1], b[1]), min(a[2], b[2]), min(a[3], b[3]))
    return r if r[0] < r[2] and r[1] < r[3] else None


def rect_expand(r, mx, my):
    return (r[0] - mx, r[1] - my, r[2] + mx, r[3] + my)


def rect_json(r):
    return {"x": r[0], "y": r[1], "w": r[2] - r[0], "h": r[3] - r[1]}


# ---- 計測 ----

def opaque_mask(img, threshold):
    """アルファが threshold 以上の画素を 255、それ以外を 0 にしたマスク。"""
    return img.getchannel("A").point(lambda v: 255 if v >= threshold else 0)


def bbox_any(img):
    """切り出し用: アルファが1以上の画素をすべて含む矩形（縮小でできたにじみも欠けないように）。"""
    return img.getchannel("A").getbbox()


def bbox_threshold(img, threshold):
    """クリップ・中心用: アルファが threshold 以上の画素を囲む矩形（見た目の輪郭）。"""
    return opaque_mask(img, threshold).getbbox()


def component_rects(img, threshold):
    """不透明画素が1つも無い列で区切り、区切られた範囲ごとの矩形を左から順に返す。"""
    mask = opaque_mask(img, threshold)
    width, height = mask.size
    rects = []
    start = None
    for x in range(width + 1):
        filled = x < width and mask.crop((x, 0, x + 1, height)).getbbox() is not None
        if filled and start is None:
            start = x
        elif not filled and start is not None:
            box = mask.crop((start, 0, x, height)).getbbox()
            rects.append((start, box[1], x, box[3]))
            start = None
    return rects


def compose(images, names, size):
    base = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    for name in names:
        base = Image.alpha_composite(base, images[name])
    return base


def to_argb_bytes(img, rect):
    """rect を切り出し、ESP32（リトルエンディアン）がそのまま uint32 0xAARRGGBB として読める並び
    （B, G, R, A）のバイト列にする。"""
    r, g, b, a = img.crop(rect).split()
    return Image.merge("RGBA", (b, g, r, a)).tobytes()


# ---- config の読み込みと検証 ----

def resolve_config(config, png_names):
    """既定値を補い、矛盾があれば ConfigError を投げる。ファイルへは書き戻さない。"""
    cfg = {
        "display": {**common.DEFAULT_DISPLAY, **config.get("display", {})},
        "gazeCenter": config.get("gazeCenter", {}),
        "gazeReach": config.get("gazeReach", common.DEFAULT_GAZE_REACH),
        "blink": {**common.DEFAULT_BLINK, **config.get("blink", {})},
        "alphaThreshold": config.get("alphaThreshold", common.DEFAULT_ALPHA_THRESHOLD),
        "layers": [],
        "groups": {},
    }

    groups = config.get("groups", {})
    for name, group in groups.items():
        if name in (FIXED, OVERLAY):
            raise ConfigError(f"グループ名 '{name}' は予約語のため使えない")
        g = {
            "max": group.get("max", common.DEFAULT_GROUP_MAX),
            "smooth": group.get("smooth", common.DEFAULT_GROUP_SMOOTH),
            "parent": group.get("parent"),
            "clip": group.get("clip"),
        }
        if len(g["max"]) != 2 or any(v < 0 for v in g["max"]):
            raise ConfigError(f"groups.{name}.max は 0 以上の [X, Y]")
        if not 0 < g["smooth"] <= 1:
            raise ConfigError(f"groups.{name}.smooth は 0 より大きく 1 以下")
        if g["parent"] is not None and g["parent"] not in groups:
            raise ConfigError(f"groups.{name}.parent '{g['parent']}' は未定義のグループ")
        if g["clip"] is not None:
            clip = {"mode": "bbox", **g["clip"]}
            if clip.get("layer") not in png_names:
                raise ConfigError(f"groups.{name}.clip.layer '{clip.get('layer')}' の素材PNGが無い")
            if clip["mode"] not in ("bbox", "components"):
                raise ConfigError(f"groups.{name}.clip.mode は bbox か components")
            g["clip"] = clip
        cfg["groups"][name] = g

    # 親子の循環を検出する。
    for name in groups:
        seen = [name]
        parent = cfg["groups"][name]["parent"]
        while parent is not None:
            if parent in seen:
                raise ConfigError(f"parent が循環している: {' → '.join(seen + [parent])}")
            seen.append(parent)
            parent = cfg["groups"][parent]["parent"]

    layers = config.get("layers", [])
    if not layers:
        raise ConfigError("layers が空")
    seen_names = set()
    for layer in layers:
        name = layer.get("name")
        if name not in png_names:
            raise ConfigError(f"layers の '{name}' の素材PNGが無い")
        if name in seen_names:
            raise ConfigError(f"layers に '{name}' が重複している")
        seen_names.add(name)
        group = layer.get("group", FIXED)
        if group != FIXED and group not in groups:
            raise ConfigError(f"layers の '{name}' の group '{group}' は未定義のグループ")
        show = layer.get("show", "always")
        if show not in ("always", "closed"):
            raise ConfigError(f"layers の '{name}' の show は always か closed")
        cfg["layers"].append({"name": name, "group": group, "show": show})

    center_layer = cfg["gazeCenter"].get("from")
    if not center_layer:
        raise ConfigError("gazeCenter.from が未指定（視線の中立点にするレイヤー名を書く）")
    if center_layer not in seen_names:
        raise ConfigError(f"gazeCenter.from '{center_layer}' は layers に無い")

    unused = set(groups) - {layer["group"] for layer in cfg["layers"]}
    for name in sorted(unused):
        print(f"注意: グループ '{name}' はどのレイヤーからも使われていない")
    return cfg


def source_hash(avatar, config):
    """config と、layers が参照する素材PNGの内容から計算するハッシュ（時刻は含めない）。"""
    h = hashlib.sha256()
    h.update(json.dumps(config, sort_keys=True, ensure_ascii=False).encode("utf-8"))
    for name in sorted(layer["name"] for layer in config.get("layers", [])):
        h.update(name.encode("utf-8"))
        with open(os.path.join(common.source_dir(avatar), name + ".png"), "rb") as f:
            h.update(f.read())
    return h.hexdigest()[:16]


# ---- 段の組み立て ----

def total_max(cfg, group):
    mx = my = 0
    while group is not None:
        g = cfg["groups"][group]
        mx += g["max"][0]
        my += g["max"][1]
        group = g["parent"]
    return mx, my


def build_stages(cfg, images, size, totals):
    """表示するレイヤーを重ね順にたどり、同じグループが連続する範囲を1段にまとめる。"""
    runs = []
    for layer in cfg["layers"]:
        if layer["show"] != "always":
            continue
        if runs and runs[-1]["group"] == layer["group"]:
            runs[-1]["layers"].append(layer["name"])
        else:
            runs.append({"group": layer["group"], "layers": [layer["name"]]})

    # 先頭の fixed の並びは下地。動くグループが無ければ段は作らない（まばたきだけ）。
    below = runs.pop(0) if runs and runs[0]["group"] == FIXED else None
    if not any(r["group"] != FIXED for r in runs):
        return below, []

    def reach(run):
        # 動いたときに届きうる範囲（重なり判定に使う）。
        mx, my = totals.get(run["group"], (0, 0))
        return rect_expand(run["rect"], mx, my) if run["rect"] else None

    def overlaps(a, b):
        ra, rb = reach(a), reach(b)
        return ra is not None and rb is not None and rect_intersect(ra, rb) is not None

    for run in runs:
        run["rect"] = bbox_any(compose(images, run["layers"], size))

    # 同じグループの段をまとめる最適化: 後ろの段を、同じグループの直前の段の位置まで奥へ下げられる
    # （間にある段のどれとも重ならない）なら1段にまとめる。まとめた結果隣り合った同じグループも結合する。
    merged = True
    while merged:
        merged = False
        for j in range(len(runs)):
            if runs[j]["group"] == FIXED:
                continue
            i = next((k for k in range(j - 1, -1, -1) if runs[k]["group"] == runs[j]["group"]), None)
            if i is None or any(overlaps(runs[j], runs[k]) for k in range(i + 1, j)):
                continue
            runs[i]["layers"] += runs[j]["layers"]
            runs[i]["rect"] = rect_union(runs[i]["rect"], runs[j]["rect"])
            del runs[j]
            k = 1
            while k < len(runs):
                if runs[k]["group"] == runs[k - 1]["group"]:
                    runs[k - 1]["layers"] += runs[k]["layers"]
                    runs[k - 1]["rect"] = rect_union(runs[k - 1]["rect"], runs[k]["rect"])
                    del runs[k]
                else:
                    k += 1
            merged = True
            break
    return below, runs


def compute_band(runs, totals, size, face_origin):
    band = None
    for run in runs:
        if run["group"] != FIXED and run["rect"]:
            mx, my = totals[run["group"]]
            band = rect_union(band, rect_expand(run["rect"], mx, my))
    # 顔のキャンバス内かつ画面に映る範囲へ制限する（画面外を合成・転送しても無駄なため）。
    visible = (-face_origin[0], -face_origin[1],
               -face_origin[0] + SCREEN_W, -face_origin[1] + SCREEN_H)
    return rect_intersect(rect_intersect(band, (0, 0, size, size)), visible)


# ---- プレビュー ----

def write_preview(path, face, measured, scale=2):
    """開き顔に計測結果を重ねた確認用画像。白=画面の範囲、黄=バンド、各色=段とクリップ矩形。"""
    img = Image.new("RGBA", face.size, (40, 40, 40, 255))
    img = Image.alpha_composite(img, face).resize(
        (face.width * scale, face.height * scale), Image.NEAREST)
    draw = ImageDraw.Draw(img)

    def box(r, color, width=1):
        draw.rectangle((r["x"] * scale, r["y"] * scale,
                        (r["x"] + r["w"]) * scale - 1, (r["y"] + r["h"]) * scale - 1),
                       outline=color, width=width)

    origin = measured["canvas"]["faceOrigin"]
    box({"x": -origin["x"], "y": -origin["y"], "w": SCREEN_W, "h": SCREEN_H}, (255, 255, 255))
    if measured["band"]:
        box(measured["band"], (255, 220, 0), 2)
    palette = [(0, 200, 255), (255, 80, 200), (80, 255, 120), (255, 140, 0), (160, 120, 255)]
    group_colors = {name: palette[i % len(palette)]
                    for i, name in enumerate(measured["groups"])}
    for stage in measured["stages"]:
        if stage["kind"] == "moving":
            box(stage["rect"], group_colors[stage["group"]])
    for name, group in measured["groups"].items():
        for r in group["clipRects"]:
            draw.rectangle((r["x"] * scale, r["y"] * scale,
                            (r["x"] + r["w"]) * scale - 1, (r["y"] + r["h"]) * scale - 1),
                           outline=group_colors[name])
            draw.text((r["x"] * scale + 2, r["y"] * scale + 2), f"clip:{name}",
                      fill=group_colors[name])
    cx, cy = measured["gazeCenter"]["x"] * scale, measured["gazeCenter"]["y"] * scale
    draw.line((cx - 8, cy, cx + 8, cy), fill=(255, 0, 0), width=2)
    draw.line((cx, cy - 8, cx, cy + 8), fill=(255, 0, 0), width=2)

    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)


# ---- 出力 ----

def remove_previous_output(out_dir):
    """前回 build したアバターの出力を消す（別のアバターへ差し替えたとき、不要なPNGを LittleFS へ
    書き込まないため）。対象は前回の avatar.json に記録されたファイルと stage_*.bin だけで、
    それ以外のファイル（手で置いたファイルなど）には触れない。"""
    previous = os.path.join(out_dir, common.DEFINITION_NAME)
    targets = set(glob.glob(os.path.join(out_dir, "stage_*.bin")))
    if os.path.exists(previous):
        definition = common.read_definition(previous)
        for layer in definition.get("config", {}).get("layers", []):
            targets.add(os.path.join(out_dir, layer["name"] + ".png"))
        targets.add(previous)
    for path in targets:
        if os.path.exists(path):
            os.remove(path)


# ---- メイン ----

def build(avatar):
    path = common.definition_path(avatar)
    definition = common.read_definition(path)
    config = definition.get("config", {})
    png_names = set(common.list_layer_pngs(avatar))
    cfg = resolve_config(config, png_names)
    threshold = cfg["alphaThreshold"]

    # 素材を読み込み、表示サイズへ縮小する（計測と出力に同じ縮小画像を使う）。
    sources = {}
    for layer in cfg["layers"]:
        sources[layer["name"]] = Image.open(
            os.path.join(common.source_dir(avatar), layer["name"] + ".png")).convert("RGBA")
    sizes = {img.size for img in sources.values()}
    if len(sizes) != 1 or next(iter(sizes))[0] != next(iter(sizes))[1]:
        raise ConfigError(f"素材PNGは全て同じサイズの正方形である必要がある: {sorted(sizes)}")
    source_size = next(iter(sizes))[0]
    face_size = round(SCREEN_H * cfg["display"]["zoom"])
    # デバイス側（C++ の整数除算）と同じく 0 方向へ丸める。
    face_origin = (int((SCREEN_W - face_size) / 2), int((SCREEN_H - face_size) / 2))
    images = {name: img.resize((face_size, face_size), Image.LANCZOS)
              for name, img in sources.items()}

    totals = {name: total_max(cfg, name) for name in cfg["groups"]}

    center_box = bbox_threshold(images[cfg["gazeCenter"]["from"]], threshold)
    if center_box is None:
        raise ConfigError(f"gazeCenter.from '{cfg['gazeCenter']['from']}' に不透明な画素が無い")

    measured_groups = {}
    for name, group in cfg["groups"].items():
        clip_rects = []
        if group["clip"]:
            clip_img = images[group["clip"]["layer"]]
            if group["clip"]["mode"] == "bbox":
                r = bbox_threshold(clip_img, threshold)
                clip_rects = [r] if r else []
            else:
                clip_rects = component_rects(clip_img, threshold)
            if not clip_rects:
                raise ConfigError(f"groups.{name}.clip.layer に不透明な画素が無い")
        measured_groups[name] = {"totalMax": list(totals[name]),
                                 "clipRects": [rect_json(r) for r in clip_rects]}

    below, runs = build_stages(cfg, images, face_size, totals)
    band = compute_band(runs, totals, face_size, face_origin) if runs else None

    stages = []
    if below:
        stages.append({"kind": "below", "layers": below["layers"]})
    stage_images = []  # (ファイル名, 合成画像, 切り出し矩形)
    for run in runs:
        moving = run["group"] != FIXED
        rect = run["rect"] if moving else (rect_intersect(run["rect"], band) if run["rect"] else None)
        if rect is None:
            # 不透明な画素が無い、またはバンドに掛からない固定段は描く必要がない。
            continue
        label = run["group"] if moving else OVERLAY
        file_name = f"stage_{len(stages)}_{label}.bin"
        stage = {"kind": "moving" if moving else "overlay"}
        if moving:
            stage["group"] = run["group"]
        stage.update({"layers": run["layers"], "file": file_name, "rect": rect_json(rect)})
        stages.append(stage)
        stage_images.append((file_name, compose(images, run["layers"], face_size), rect))

    # 閉じ顔でだけ表示するレイヤーは、目（gazeCenter.from）の下端を越える位置まで下げて描く。
    offsets = {}
    for layer in cfg["layers"]:
        if layer["show"] == "closed":
            r = bbox_threshold(images[layer["name"]], threshold)
            if r:
                drop = max(0, center_box[3] - r[3] + CLOSED_DROP_PADDING)
                offsets[layer["name"]] = {"x": 0, "y": drop}
    closed = {"layers": [layer["name"] for layer in cfg["layers"]]}
    if offsets:
        closed["offsets"] = offsets

    measured = {
        "schemaVersion": SCHEMA_VERSION,
        "avatar": avatar,
        "sourceHash": source_hash(avatar, config),
        "canvas": {"sourceSize": source_size, "faceSize": face_size,
                   "faceOrigin": {"x": face_origin[0], "y": face_origin[1]}},
        "gazeCenter": {"x": (center_box[0] + center_box[2]) / 2,
                       "y": (center_box[1] + center_box[3]) / 2},
        "groups": measured_groups,
        "band": rect_json(band) if band else None,
        "stages": stages,
        "faces": {
            "open": {"layers": [layer["name"] for layer in cfg["layers"]
                                if layer["show"] == "always"]},
            "closed": closed,
        },
    }

    # ---- 出力 ----
    out_dir = common.output_dir()
    os.makedirs(out_dir, exist_ok=True)
    remove_previous_output(out_dir)

    total = 0
    for name, img in images.items():
        dest = os.path.join(out_dir, name + ".png")
        img.save(dest, format="PNG", optimize=True)
        total += os.path.getsize(dest)
    print(f"PNG {len(images)} 枚: {total / 1024:.1f} KB")

    ram = 0
    for file_name, img, rect in stage_images:
        data = to_argb_bytes(img, rect)
        with open(os.path.join(out_dir, file_name), "wb") as f:
            f.write(data)
        ram += len(data)
        print(f"  {file_name}: {rect[2] - rect[0]}x{rect[3] - rect[1]} ({len(data) / 1024:.1f} KB)")
    print(f"段の生ARGB 合計: {ram / 1024:.1f} KB（実行時に PSRAM へ読み込む量）")

    definition["measured"] = measured
    common.write_definition(path, definition)
    common.write_definition(os.path.join(out_dir, common.DEFINITION_NAME), definition)

    face = compose(images, measured["faces"]["open"]["layers"], face_size)
    write_preview(common.preview_path(avatar), face, measured)

    b = measured["band"]
    if b is None:
        print("バンド: なし（動くグループが無い）")
    else:
        print(f"バンド: ({b['x']},{b['y']}) {b['w']}x{b['h']}")
    print(f"更新: {os.path.relpath(path, common.ROOT)}")
    print(f"出力: {os.path.relpath(out_dir, common.ROOT)}")
    print(f"確認用: {os.path.relpath(common.preview_path(avatar), common.ROOT)}")


def check(avatar):
    definition = common.read_definition(common.definition_path(avatar))
    recorded = definition.get("measured", {}).get("sourceHash")
    current = source_hash(avatar, definition.get("config", {}))
    if recorded == current:
        print("最新: measured は現在の素材・config から生成されている")
        return 0
    print("要再生成: 素材か config が measured の生成後に変更されている"
          "（python tools/build_avatar.py で再生成する）")
    return 1


def main():
    parser = argparse.ArgumentParser(description="avatar.json から計測・デバイス用データを生成する")
    parser.add_argument("avatar", help="アバター名（assets/<アバター名>/avatar.json）")
    parser.add_argument("--check", action="store_true", help="再生成が必要かだけを確認する")
    args = parser.parse_args()

    if not os.path.exists(common.definition_path(args.avatar)):
        sys.exit(f"定義ファイルがありません（先に init_avatar.py を実行）: "
                 f"{os.path.relpath(common.definition_path(args.avatar), common.ROOT)}")
    try:
        if args.check:
            sys.exit(check(args.avatar))
        build(args.avatar)
    except ConfigError as e:
        sys.exit(f"config エラー: {e}")


if __name__ == "__main__":
    main()
