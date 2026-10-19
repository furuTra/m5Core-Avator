#pragma once

#include <Arduino.h>

// 画像アセットを置くファイルシステム(LittleFS)のヘッダを M5Unified.h より前に include する。
// M5GFX は drawPngFile のファイルシステム特殊化を、対応ヘッダのマクロ検出時のみ有効化するため
// （LittleFS.h → _LITTLEFS_H_）。AssetLoader.h を M5Unified.h より前に include すること。
#include <LittleFS.h>
#include <FS.h>

// 画像アセットへのアクセスを集約するユーティリティ。アセットはフラッシュの LittleFS 領域に置き、
// パーツ名からフルパスを解決する。他モジュールにパス組み立てやFS参照をさせない
// （docs/assets.md: 「ファイルパスを直接扱わせない」方針）。
class AssetLoader {
public:
    // LittleFS をマウントする。成功したら true。自動フォーマットはしない（失敗＝イメージ未書込みを
    // 検知したいため）。失敗時は false を返し、呼び出し側でフォールバック表示を行う。
    bool begin();

    // 描画に使うファイルシステム(LittleFS)への参照。drawPngFile へそのまま渡す。
    fs::FS& fs();

    // レイヤー名（avatar.json の layers[].name）からPNGのフルパス（"/assets/avatar/<名前>.png"）を返す。
    // 戻り値は次回呼び出しまで有効な内部バッファを指す（即 drawPngFile へ渡す想定）。
    const char* pngPath(const char* partName);

    // アバター定義ファイルのフルパス（"/assets/avatar/avatar.json"）。
    const char* definitionPath();

    // アセットディレクトリ内の任意ファイル（例: "stage_1_eyes.bin"）のフルパスを返す。
    // 戻り値は次回呼び出しまで有効な内部バッファを指す（即 open/drawPngFile へ渡す想定）。
    const char* binPath(const char* fileName);

    bool isReady() const { return ready; }

private:
    bool ready = false;
    char pathBuffer[64];
};
