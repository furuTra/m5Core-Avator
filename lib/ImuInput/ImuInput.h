#pragma once

#include <M5Unified.h>

// 本体の傾き(6軸IMU)を「画面上の点(px)」として取り出す入力モジュール。
//
// IMU固有の事情（どの加速度軸を使うか・画面とIMUの軸ずれ・符号・感度）をここに閉じ込め、
// 利用側へは TouchInput と同じく「画面上の点」だけを渡す。点をどう使うか（視線を向ける等）は
// このモジュールの責務ではないため、視線やアバターに関する知識は持たない。
class ImuInput {
public:
    // 傾き0のときに返す基準点(screen px)を設定する。
    // 視線に使う場合はアバターの目の中心（FaceRenderer::gazeCenterX()/gazeCenterY()）を渡す。
    void begin(float neutralX, float neutralY);

    // 傾きから求めた画面座標(px, 左上原点)を (x,y) へ返して true。
    // IMUが読めなければ false を返し、(x,y) は変更しない。
    // M5.Imu の値は M5.update() が更新するため、必ずその後に呼ぶこと。
    //
    // 注: 傾きが大きいと基準点から大きく離れた（画面外の）点になりうる。範囲の扱いは用途によるので、
    // ここではクランプせずそのまま返す。
    bool getPoint(float& x, float& y) const;

private:
    float neutralX = 160.0f;
    float neutralY = 120.0f;
};
