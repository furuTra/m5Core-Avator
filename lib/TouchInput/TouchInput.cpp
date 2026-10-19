#include "TouchInput.h"

bool TouchInput::getPoint(int& x, int& y) const {
    // getCount() が 0 のときは getDetail() の内容が有効でないため、先に件数で弾く。
    if (!M5.Touch.isEnabled() || M5.Touch.getCount() == 0) return false;

    // 複数点タッチは扱わず、1点目だけを見る。
    const auto& touch = M5.Touch.getDetail();
    if (!touch.isPressed()) return false; // 離した直後のフレームなど（押下中ではない）

    x = touch.x;
    y = touch.y;
    return true;
}
