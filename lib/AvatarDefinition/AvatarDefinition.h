#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>

#include <vector>

// アバター定義ファイル（avatar.json。tools/build_avatar.py が生成）の内容を保持するモジュール。
//
// 起動時に load() で一度だけ JSON を読み、描画に必要な値を構造体へ写す（JSON のデータは読み終えたら
// 解放する）。アバター固有の情報（パーツ構成・重ね順・動かし方・計測値）はすべてここから得るため、
// 描画側（FaceRenderer）は特定のパーツ名や座標を持たない。描画方法には関知しない。
// 各値の意味は docs/avatar-definition.md を参照。座標はすべて「顔座標」（表示サイズの顔の正方形の
// 左上原点, 画面px単位）で、画面座標 = 顔座標 + faceOrigin。

// 矩形（顔座標, px）。
struct AvatarRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

// 描画するレイヤー1枚と、その描画位置のずらし量（px）。
struct AvatarLayer {
    String name;
    int offsetX = 0;
    int offsetY = 0;
};

// 視線に合わせて一緒に動くレイヤーのまとまり（動きグループ）。
struct AvatarGroup {
    String name;
    float maxX = 0.0f;  // 目標方向を振り切ったときの移動量(px)。親がある場合は親に対する相対量
    float maxY = 0.0f;
    float smooth = 0.2f; // 1フレームごとに目標までの残りを詰める割合(0..1)
    int parent = -1;     // 親グループの添字（-1 = 親なし）
    // はみ出し防止の描画範囲（移動量0での位置）。実行時は親の移動量分ずらして使う。空ならクリップしない。
    std::vector<AvatarRect> clipRects;
};

// バンド内の描画段の種類。
enum class AvatarStageKind {
    Below,   // 動くパーツより奥の固定レイヤー。起動時にPNGから一度だけ焼いて下地にする
    Moving,  // 1つの動きグループ。生ARGBをグループの移動量分ずらして重ねる
    Overlay, // 動くパーツより手前の固定レイヤー。生ARGBを固定位置で重ね直す
};

// バンド内の描画段（奥→手前の順に重ねる）。
struct AvatarStage {
    AvatarStageKind kind = AvatarStageKind::Below;
    int group = -1;                  // Moving のときのグループ添字
    std::vector<String> layers;      // Below のときに焼くレイヤー名
    String file;                     // Moving / Overlay の生ARGBファイル名
    AvatarRect rect;                 // Moving / Overlay の生ARGBの切り出し矩形
};

class AvatarDefinition {
public:
    // 定義ファイルを読み込む。成功で true。失敗時は errorMessage() に原因が入る。
    bool load(fs::FS& fs, const char* path);

    // load() が失敗した原因（画面表示・シリアル出力用）。
    const char* errorMessage() const { return error.c_str(); }

    String avatarName;

    // 顔の表示サイズ（正方形の辺, px）と、顔座標の原点の画面上の位置。
    int faceSize = 0;
    int faceOriginX = 0;
    int faceOriginY = 0;

    uint32_t backgroundRgb888 = 0; // 顔の外側の背景色（0xRRGGBB）

    // 視線の中立点（顔座標）と、目標方向を振り切るまでの距離（画面px）。
    float gazeCenterX = 0.0f;
    float gazeCenterY = 0.0f;
    float gazeReachX = 160.0f;
    float gazeReachY = 120.0f;

    // まばたき。間隔は [min, max] の範囲でランダム。
    unsigned long blinkIntervalMinMs = 2500;
    unsigned long blinkIntervalMaxMs = 6000;
    unsigned long blinkClosedMs = 180;

    std::vector<AvatarGroup> groups;

    // 視線の変化時に再合成・転送する矩形（顔座標）。動くグループが無ければ hasBand = false。
    bool hasBand = false;
    AvatarRect band;
    std::vector<AvatarStage> stages;

    // 起動時に一度だけ合成する全画面の顔（開き顔・閉じ顔）のレイヤー（奥→手前）。
    std::vector<AvatarLayer> openFace;
    std::vector<AvatarLayer> closedFace;

private:
    bool fail(const String& message);

    // load() の各セクションの読み込み（load() を1関数1セクションに分けたもの）。
    // 失敗時は fail() で原因を記録して false を返す。
    bool loadDisplay(const JsonObjectConst config, const JsonObjectConst measured);
    bool loadGaze(const JsonObjectConst config, const JsonObjectConst measured);
    bool loadGroups(const JsonObjectConst config, const JsonObjectConst measured);
    bool resolveGroupParents(const JsonObjectConst configGroups);
    bool loadStages(const JsonObjectConst measured);
    bool loadFaces(const JsonObjectConst measured);

    // 描画段1つを読む。成功時のみ stage へ書き込む（失敗時は fail() で原因を記録し stage は変更しない）。
    bool tryReadStage(const JsonObjectConst source, AvatarStage& stage);

    String error;
};
