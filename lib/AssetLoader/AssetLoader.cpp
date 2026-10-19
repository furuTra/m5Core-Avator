#include "AssetLoader.h"

#include <M5Unified.h>

namespace {
// アバターのアセット（定義ファイル・パーツPNG・段の生ARGB）の配置ディレクトリ（LittleFSルートからの
// 絶対パス）。tools/build_avatar.py はアバター名によらずここへ出力する。
constexpr const char* ASSET_DIR = "/assets/avatar";
} // namespace

bool AssetLoader::begin() {
    // フラッシュのLittleFS領域をマウントする（`pio run -t uploadfs` で書き込み済み前提）。
    ready = LittleFS.begin(false);
    return ready;
}

fs::FS& AssetLoader::fs() {
    return LittleFS;
}

const char* AssetLoader::pngPath(const char* partName) {
    snprintf(pathBuffer, sizeof(pathBuffer), "%s/%s.png", ASSET_DIR, partName);
    return pathBuffer;
}

const char* AssetLoader::definitionPath() {
    snprintf(pathBuffer, sizeof(pathBuffer), "%s/avatar.json", ASSET_DIR);
    return pathBuffer;
}

const char* AssetLoader::binPath(const char* fileName) {
    snprintf(pathBuffer, sizeof(pathBuffer), "%s/%s", ASSET_DIR, fileName);
    return pathBuffer;
}
