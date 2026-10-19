# init_avatar.py / build_avatar.py の共通処理（パス解決・定義ファイルの読み書き・既定値）。
# 定義ファイルの仕様は docs/avatar-definition.md を参照。
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFINITION_NAME = "avatar.json"

# config で省略されたキーの既定値（docs/avatar-definition.md の「既定値」列と一致させる）。
DEFAULT_DISPLAY = {"zoom": 1.6, "background": "#0A0F14"}
DEFAULT_GAZE_REACH = [160, 120]
DEFAULT_BLINK = {"intervalMs": [2500, 6000], "closedMs": 180}
DEFAULT_ALPHA_THRESHOLD = 128
DEFAULT_GROUP_MAX = [0, 0]
DEFAULT_GROUP_SMOOTH = 0.2

# 1行に収める最大文字数（整形用）。短い配列・オブジェクトは1行にまとめて読みやすくする。
LINE_WIDTH = 100


def source_dir(avatar):
    return os.path.join(ROOT, "assets", avatar)


def output_dir():
    """デバイスが読むアセットの出力先。アバター名によらず固定で、最後に build したアバターが表示される
    （デバイスは起動時に常に /assets/avatar/avatar.json を読む）。"""
    return os.path.join(ROOT, "data", "assets", "avatar")


def preview_path(avatar):
    return os.path.join(ROOT, "build", "preview", avatar + ".png")


def definition_path(avatar):
    return os.path.join(source_dir(avatar), DEFINITION_NAME)


def list_layer_pngs(avatar):
    """素材ディレクトリ内のPNG名（拡張子なし）を名前順で返す。"""
    return sorted(os.path.splitext(f)[0] for f in os.listdir(source_dir(avatar))
                  if f.lower().endswith(".png"))


def format_json(value, indent=0):
    """json.dumps(indent=2) では縦に長くなりすぎるため、1行に収まる要素は1行にまとめて整形する。"""
    flat = json.dumps(value, ensure_ascii=False)
    if indent + len(flat) <= LINE_WIDTH or not isinstance(value, (dict, list)) or not value:
        return flat
    pad = " " * (indent + 2)
    if isinstance(value, dict):
        items = [f"{pad}{json.dumps(k, ensure_ascii=False)}: {format_json(v, indent + 2)}"
                 for k, v in value.items()]
        return "{\n" + ",\n".join(items) + "\n" + " " * indent + "}"
    items = [pad + format_json(v, indent + 2) for v in value]
    return "[\n" + ",\n".join(items) + "\n" + " " * indent + "]"


def write_definition(path, definition):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(format_json(definition) + "\n")


def read_definition(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)
