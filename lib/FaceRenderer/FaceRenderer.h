#pragma once

#include <Arduino.h>
// AssetLoader.h はアセットFS(LittleFS.h)を include する。M5GFX は drawPngFile のファイルシステム
// 特殊化を対応ヘッダのマクロ検出時のみ有効化するため、AssetLoader.h（＝FSヘッダ）を M5Unified.h より
// 前に include する必要がある。
#include "AssetLoader.h"
#include <M5Unified.h>

#include <vector>

#include "AvatarDefinition.h" // パーツ構成・重ね順・動かし方・計測値（avatar.json）
#include "GazeController.h"   // 追従先の点 → 目標方向 → 各パーツのオフセット(px)（描画・入力に非依存）

// アバター定義（AvatarDefinition）に従って顔パーツを合成し、画面へ転送するモジュール。
// 特定のパーツ名や座標は持たず、どのパーツをどう重ね・どう動かすかはすべて定義ファイルから得る。
//
// 描画コスト対策として、毎フレームの全レイヤー再合成は行わない（PNGデコードは重い）。起動時に
// 「開き顔（bgFull）」「閉じ顔（faceClosed）」を一度だけ合成しておき、実行時は状態変化時のみ更新する。
//
// 視線に合わせて動くパーツは、動きグループごとに同じ量だけ平行移動する。可動域はすべて「バンド」
// （定義ファイルの band）に収まるため、視線の変化時はバンドだけを再合成して転送する:
//   下地(below 段) → 動く段(moving: グループの移動量分ずらす) / 固定の段(overlay) を定義の順に重ねる。
// 半透明を含む段は、ツールが正しくアルファ合成した生ARGB（stage_*.bin）を pushAlphaImage で重ねる
// （drawPng はスプライト宛に描くと半透明のアルファを保持しないため）。
class FaceRenderer {
public:
    // 定義に従って各キャンバスを確保し、開き顔・閉じ顔・バンドの下地を一度だけ合成し、各段の生ARGBを
    // 読み込む。definition は FaceRenderer より長く生存すること。PSRAM 確保・読み込みの成否を返す。
    bool begin(AssetLoader* loader, const AvatarDefinition* definition);

    // 現在の状態（開き・視線中央）の顔を画面へ転送する。begin 直後の初期表示に使う。
    void drawStaticFace();

    // まばたきと視線トラッキングの状態を1フレーム進める。loop() から毎フレーム呼ぶ。
    // 状態（開閉・各グループの位置）が変化したときだけ再描画する。
    void update();

    // 追従先の座標(画面px, M5ディスプレイ座標系)を指定し、そこへ視線を向ける目標を設定する。
    // 座標→目標方向→各グループのオフセット(px) の計算は GazeController が持ち、このクラスは
    // 求まったオフセットで描くことだけを担う。実際の追従（滑らかな補間）は update() が行う。
    // IMU・タッチ・通信など入力源を問わず、このメソッドだけで視線を制御できる（流用のための境界）。
    void lookAt(float x, float y);

    // 視線の中立点(=この点を向くと正面)の画面座標。入力アダプタ(EyeTracker等)へ中立点を渡すために公開する。
    float gazeCenterX() const;
    float gazeCenterY() const;

private:
    // 画面上の移動量(px)。
    struct Offset {
        int x;
        int y;
    };

    // バンド内の生ARGBを持つ段（moving / overlay）。
    struct ArgbStage {
        const AvatarStage* stage;
        uint32_t* argb; // stage->rect 原寸の生ARGB(0xAARRGGBB)。PSRAM 上
    };

    // 1パーツ（表示サイズへ縮小済みPNG）を指定した原点(x,y)へ等倍で重ねる（透過部はアルファ合成）。
    void drawLayer(M5Canvas& target, const char* partName, int x, int y);

    // レイヤー一覧を重ね順に target へ合成する（開き顔・閉じ顔。起動時に一度だけ）。
    void composeFace(M5Canvas& target, const std::vector<AvatarLayer>& layers);

    // バンドの下地（below 段, 不透明16bpp）を起動時に一度だけ焼く。
    void bakeBandBelow();

    // 生ARGB(0xAARRGGBB)のバイナリを PSRAM バッファへ読み込む。成功で true。
    bool loadArgbBin(const char* fileName, uint32_t** dst, size_t count);

    // 顔全体を描画して画面へ転送する（初期表示・まばたきの開閉切替時に使う全画面更新）。
    void render();

    // バンドだけを合成して転送する（視線の変化のみのとき。全画面転送を避ける）。
    void renderBand();

    // バンドへ各段を重ねる（render/renderBand 共通処理）。target はバンド座標系のキャンバス。
    void drawBandInto(M5Canvas& target);

    // グループの現在の移動量（親をたどって合算したもの）。
    Offset totalOffset(int group) const;

    // まばたきの状態機械を進める。切り替えが起きたら true を返す。
    bool updateBlink(unsigned long now);

    // 各グループのオフセットを1フレーム進める。描画上の整数オフセットのいずれかが変化したら true。
    bool updateGaze();

    unsigned long randomBlinkInterval() const;

    AssetLoader* assets = nullptr;
    const AvatarDefinition* def = nullptr;

    M5Canvas bgFull{&M5.Display};     // 開き顔（中立位置）。全画面更新時にバンド外を埋める（不変）
    M5Canvas faceClosed{&M5.Display}; // 閉じ顔（不変）
    M5Canvas bandCanvas{&M5.Display}; // バンドの作業用（合成 → 転送）
    M5Canvas bandBelow{&M5.Display};  // バンドの下地（below 段, 16bpp。不変）
    std::vector<ArgbStage> argbStages;
    bool ready = false;

    // バンド左上の画面座標。
    int bandScreenX = 0;
    int bandScreenY = 0;

    // まばたき状態。eyesClosed=現在閉じているか、nextToggleAt=次に開閉を切り替える時刻(ms)。
    bool eyesClosed = false;
    unsigned long nextToggleAt = 0;

    // 追従先の点を目標方向(-1..1)へ正規化する視線制御本体（中立点・可動域は定義ファイルの値）。
    GazeController gaze;

    // 動きグループごとの移動量（def->groups と同じ並び）。親がある場合は親に対する相対量。
    std::vector<GazeOffset> groupOffsets;
};
