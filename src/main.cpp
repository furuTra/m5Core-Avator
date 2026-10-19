#include <Arduino.h>
#include <M5Unified.h>

#include "AssetLoader.h"
#include "AvatarDefinition.h"
#include "EyeTracker.h"
#include "FaceRenderer.h"
#include "ImuInput.h"
#include "TouchInput.h"

// main.cpp は初期化と loop からの呼び出しに徹する（docs/architecture.md）。
// 描画・合成ロジックは FaceRenderer、LittleFS マウント/パス解決は AssetLoader、アバター固有の情報
// （パーツ構成・動かし方。avatar.json）の読み込みは AvatarDefinition、入力から画面上の点を
// 取り出すのは TouchInput（タッチ）と ImuInput（傾き）、その点の調停（優先度と保持）は EyeTracker。
// main はそれらの橋渡し: 入力の位置(x,y) → EyeTracker → 追従先(x,y) → FaceRenderer。
// どの入力を「注目(優先)」「既定」に割り当てるかも main が決める（EyeTracker は入力源を知らない）。

AssetLoader assetLoader;
AvatarDefinition avatarDefinition;
FaceRenderer faceRenderer;
EyeTracker eyeTracker;
TouchInput touchInput;
ImuInput imuInput;

// LittleFS マウント・定義ファイルの読み込み・キャンバス確保に失敗したときの画面表示
// （顔を出せない原因を実機で確認できるように）。
void showFatal(const char* message) {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextColor(WHITE);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(8, 8);
    M5.Display.println(message);
}

void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5.begin(cfg);

    // アセットFS（フラッシュのLittleFS領域）をマウントする。
    if (!assetLoader.begin()) {
        Serial.println("LittleFS mount failed");
        showFatal("FS ERROR\nRun uploadfs");
        return;
    }

    // 表示するアバターの定義ファイルを読む（どのアバターかは data/ へ出力したもので決まる）。
    if (!avatarDefinition.load(assetLoader.fs(), assetLoader.definitionPath())) {
        Serial.printf("Avatar definition error: %s\n", avatarDefinition.errorMessage());
        showFatal((String("DEF ERROR\n") + avatarDefinition.errorMessage()).c_str());
        return;
    }
    Serial.printf("Avatar: %s\n", avatarDefinition.avatarName.c_str());

    if (!faceRenderer.begin(&assetLoader, &avatarDefinition)) {
        Serial.println("Canvas alloc / asset load failed");
        showFatal("MEM ERROR");
        return;
    }

    // 視線の中立点(=正面)としてアバターの目の中心を渡す。EyeTracker には入力が無いときの既定値、
    // ImuInput には傾き0のときの基準点として、同じ点を使う。
    eyeTracker.begin(faceRenderer.gazeCenterX(), faceRenderer.gazeCenterY());
    imuInput.begin(faceRenderer.gazeCenterX(), faceRenderer.gazeCenterY());

    faceRenderer.drawStaticFace();
}

void loop() {
    M5.update();

    // 1) 画面がタッチされていれば、その位置(x,y)を取得して「注目点」として渡す（傾きより優先される）。
    int touchX, touchY;
    if (touchInput.getPoint(touchX, touchY)) {
        eyeTracker.setFocusTarget(touchX, touchY);
    }

    // 2) IMUの傾きから位置(x,y)を取得して「既定の追従先」として渡す（タッチが無いときに見る点）。
    float tiltX, tiltY;
    if (imuInput.getPoint(tiltX, tiltY)) {
        eyeTracker.setBaseTarget(tiltX, tiltY);
    }

    // 3) 追従先の位置(x,y)を取得する。直近の注目点(タッチ)があればそこ、無ければ既定の追従先(傾き)が
    //    返る（優先度と指定が途切れた後の保持は EyeTracker 側の責務）。
    float targetX, targetY;
    const bool hasTarget = eyeTracker.getTarget(targetX, targetY);

    // 4) 取得した位置へ視線を向ける。入力が何も無い場合は前回の目標を維持する
    //    （getTarget は中立点を返すが lookAt は呼ばない）。
    if (hasTarget) {
        faceRenderer.lookAt(targetX, targetY);
    }

    // まばたき・視線追従の状態を進める（状態変化時のみ再描画される）。
    faceRenderer.update();
    delay(16);
}
