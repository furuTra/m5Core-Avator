#include "EyeTracker.h"

namespace {
// ---- 注目点の保持時間 ----
// 最後に setFocusTarget で指定された点を、この時間だけ見続けてから既定の追従先へ戻る。
// 0 にすると指定が途切れた瞬間に戻ってしまい、「見ていた」印象が残らないため余韻を持たせる。
constexpr unsigned long FOCUS_HOLD_MS = 1500;
} // namespace

void EyeTracker::begin(float cx, float cy) {
    centerX = cx;
    centerY = cy;
}

void EyeTracker::setFocusTarget(float x, float y) {
    // 指定された点をそのまま追従先にする。画面外の値でも構わない（可動域を超える分は
    // FaceRenderer::lookAt が正規化時にクランプする）。
    focusX = x;
    focusY = y;
    focusHoldUntil = millis() + FOCUS_HOLD_MS;
    focusHolding = true;
}

void EyeTracker::setBaseTarget(float x, float y) {
    baseX = x;
    baseY = y;
    hasBase = true;
}

bool EyeTracker::getFocusTarget(float& x, float& y) {
    if (!focusHolding) return false;

    // 指定から一定時間はその点を見続け、期限が切れたら既定の追従先へフォールバックする。
    if ((long)(millis() - focusHoldUntil) < 0) {
        x = focusX;
        y = focusY;
        return true;
    }
    focusHolding = false;
    return false;
}

bool EyeTracker::getTarget(float& x, float& y) {
    // 注目点が優先。保持が切れていれば既定の追従先へフォールバックする。
    if (getFocusTarget(x, y)) return true;
    if (hasBase) {
        x = baseX;
        y = baseY;
        return true;
    }

    // どちらの入力も無いときは中立点(正面)を返す。
    x = centerX;
    y = centerY;
    return false;
}
