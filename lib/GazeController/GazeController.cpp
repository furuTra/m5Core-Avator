#include "GazeController.h"

#include <math.h> // lroundf

namespace {
float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
} // namespace

void GazeOffset::begin(float mx, float my, float sm) {
    maxX = mx;
    maxY = my;
    smooth = sm;
    curX = 0.0f;
    curY = 0.0f;
    drawnX = 0;
    drawnY = 0;
}

bool GazeOffset::update(float dirX, float dirY) {
    // 目標方向(-1..1)を可動量(px)へ比例配分し、現在値をそこへ滑らかに近づける。
    curX += (dirX * maxX - curX) * smooth;
    curY += (dirY * maxY - curY) * smooth;

    // 変化の判定は補間中の実数値ではなく、実際に描画へ使う整数px で行う。
    const int nx = (int)lroundf(curX);
    const int ny = (int)lroundf(curY);
    if (nx == drawnX && ny == drawnY) return false;

    drawnX = nx;
    drawnY = ny;
    return true;
}

void GazeController::begin(float centerX, float centerY, float rx, float ry) {
    neutralX = centerX;
    neutralY = centerY;
    reachX = rx;
    reachY = ry;
}

void GazeController::lookAt(float x, float y) {
    // 追従先の点を中立点からの変位として捉え、可動域 reach で正規化して目標方向(-1..1)にする。
    // 実際の追従（補間）は GazeOffset::update が毎フレーム行う。
    targetDirX = clampf((x - neutralX) / reachX, -1.0f, 1.0f);
    targetDirY = clampf((y - neutralY) / reachY, -1.0f, 1.0f);
}
