#include "ImuInput.h"

namespace {
// ---- IMU傾き → 画面上の点(screen px) への写像 ----
// 傾き(重力加速度, 単位g)を、基準点からの画面上の変位(px)へ線形変換する感度。
// 値は現アバターの可動域(avatar.json の gazeReach / groups.*.max)に合わせて調整してあり、
// 従来の瞳の追従量(左右 dx=-ax*45 / 上下 dy=az*12 相当)を再現する。感度の変更はここだけで完結する。
constexpr float IMU_GAIN_X = 1200.0f;
constexpr float IMU_GAIN_Y = 360.0f;

// 左右(画面X): IMU軸と画面軸が90°ずれているため ax の符号を反転して割り当てる。
constexpr float IMU_SIGN_X = -1.0f;
// 上下(画面Y): 画面法線方向の重力成分 az を使う（縦に立てる=正面 / 横に寝かせる=下 / 逆さ=上）。
// 画面平面内の ay は上下に使わない。実機で上下が逆なら符号を反転する。
constexpr float IMU_SIGN_Y = 1.0f;
} // namespace

void ImuInput::begin(float nx, float ny) {
    neutralX = nx;
    neutralY = ny;
}

bool ImuInput::getPoint(float& x, float& y) const {
    float ax, ay, az;
    if (!M5.Imu.isEnabled() || !M5.Imu.getAccel(&ax, &ay, &az)) return false;

    // 基準点(傾き0=正面)から、傾き方向へずらした点を返す。
    x = neutralX + IMU_SIGN_X * ax * IMU_GAIN_X;
    y = neutralY + IMU_SIGN_Y * az * IMU_GAIN_Y;
    return true;
}
