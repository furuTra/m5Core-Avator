#pragma once

#include <Arduino.h>

// 複数の入力から「アバターが追従すべき画面上の座標(点)」を1つ決める調停モジュール。
//
// このモジュールが持つのは **どの点を優先し、いつまで保持するか** という視線の振る舞いだけで、
// 点がどこから来たか（タッチ・IMU・通信など）には関知しない。入力デバイスへのアクセスは各入力
// モジュール(TouchInput / ImuInput など)、点の見せ方は FaceRenderer の責務。
//
// 入力は呼び出し側（main）が毎フレーム設定する。優先度の異なる2系統を受け付ける:
//   - setFocusTarget(x, y) … 一時的に注目させたい点。**優先**され、指定から FOCUS_HOLD_MS の間有効。
//   - setBaseTarget(x, y)  … 既定の追従先。注目の保持が切れているときに使う。
//
// 出力は M5 ディスプレイ座標系(px, 左上原点)の「追従先の点」。入力が何も無ければ中立点(=正面)を返す。
// 中立点は視線の基準（この点を向くと正面）で、通常はアバターの目の中心を渡す。
class EyeTracker {
public:
    // 視線の中立点(screen px)を設定する。入力が何も無いときこの点を返す。
    // 通常は FaceRenderer::gazeCenterX()/gazeCenterY()（目の中心）を渡す。
    void begin(float centerX, float centerY);

    // 一時的に注目させたい点(screen px)を指定する。指定はこの時点から FOCUS_HOLD_MS の間だけ有効で、
    // その間は既定の追従先より優先される（指定が途切れても少しの間その点を見続けるため）。
    void setFocusTarget(float x, float y);

    // 既定の追従先(screen px)を指定する。注目の保持が切れている間はこの点を見る。
    void setBaseTarget(float x, float y);

    // 追従先の座標(screen px)を算出し (x,y) へ返す。
    // 有効な注目点があればその点、無ければ既定の追従先を返す。
    // どちらも無ければ中立点を返して false（呼び出し側で前回値維持等を選べる）。
    bool getTarget(float& x, float& y);

private:
    // setFocusTarget で指定された点が有効期限内なら (x,y) へ返して true。
    bool getFocusTarget(float& x, float& y);

    float centerX = 160.0f;
    float centerY = 120.0f;

    // 直近に指定された注目点と、その有効期限(ms)。指定が途切れても急に視線が戻らないようにする保持。
    float focusX = 0.0f;
    float focusY = 0.0f;
    unsigned long focusHoldUntil = 0;
    bool focusHolding = false;

    // 直近に指定された既定の追従先。hasBase=一度でも指定されたか。
    float baseX = 0.0f;
    float baseY = 0.0f;
    bool hasBase = false;
};
