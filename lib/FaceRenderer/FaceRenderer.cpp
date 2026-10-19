#include "FaceRenderer.h"
// アセットFS(LittleFS)は FaceRenderer.h 経由で AssetLoader.h → M5Unified.h の順に include 済み。

namespace {
// キャンバス（=ディスプレイ）サイズ。M5Stack Core2 の物理解像度と一致。
constexpr int CANVAS_W = 320;
constexpr int CANVAS_H = 240;
} // namespace

bool FaceRenderer::begin(AssetLoader* loader, const AvatarDefinition* definition) {
    assets = loader;
    def = definition;

    // 視線制御へこのアバターの値（中立点・可動域・各グループの可動量と追従の速さ）を渡す。
    // 以降の視線計算は GazeController/GazeOffset が担い、ここは求まったオフセットで描くだけになる。
    gaze.begin(def->faceOriginX + def->gazeCenterX, def->faceOriginY + def->gazeCenterY,
               def->gazeReachX, def->gazeReachY);
    groupOffsets.assign(def->groups.size(), GazeOffset());
    for (size_t i = 0; i < def->groups.size(); ++i) {
        const AvatarGroup& g = def->groups[i];
        groupOffsets[i].begin(g.maxX, g.maxY, g.smooth);
    }

    // 各スプライトを PSRAM に確保する（内蔵SRAMを圧迫しないため）。全て 16bpp。
    bgFull.setPsram(true);     bgFull.setColorDepth(16);
    faceClosed.setPsram(true); faceClosed.setColorDepth(16);
    if (!bgFull.createSprite(CANVAS_W, CANVAS_H) || !faceClosed.createSprite(CANVAS_W, CANVAS_H)) {
        return false;
    }
    if (def->hasBand) {
        bandScreenX = def->faceOriginX + def->band.x;
        bandScreenY = def->faceOriginY + def->band.y;
        bandCanvas.setPsram(true); bandCanvas.setColorDepth(16);
        bandBelow.setPsram(true);  bandBelow.setColorDepth(16);
        if (!bandCanvas.createSprite(def->band.w, def->band.h) ||
            !bandBelow.createSprite(def->band.w, def->band.h)) {
            return false;
        }
    }

    // 各段の生ARGBをFSから読み込む（毎フレームのPNGデコード・スプライトへのアルファ喪失を避ける）。
    for (const AvatarStage& stage : def->stages) {
        if (stage.kind == AvatarStageKind::Below) continue;
        ArgbStage loaded{&stage, nullptr};
        if (!loadArgbBin(stage.file.c_str(), &loaded.argb, (size_t)stage.rect.w * stage.rect.h)) {
            return false;
        }
        argbStages.push_back(loaded);
    }

    // 開き顔・閉じ顔・バンドの下地を一度だけ合成する（重い PNG デコードは begin 時のみ）。
    composeFace(bgFull, def->openFace);
    composeFace(faceClosed, def->closedFace);
    if (def->hasBand) bakeBandBelow();

    // まばたきのタイミングを実行ごとに散らすため乱数を初期化する。
    randomSeed(micros());
    eyesClosed = false;
    nextToggleAt = millis() + randomBlinkInterval();

    ready = true;
    return true;
}

void FaceRenderer::drawLayer(M5Canvas& target, const char* partName, int x, int y) {
    // PNG は表示サイズへ縮小済み（tools/build_avatar.py）のため等倍で描く（拡大描画は低速）。
    // 透過部分は drawPngFile 側でアルファ合成される。
    target.drawPngFile(assets->fs(), assets->pngPath(partName), x, y);
}

void FaceRenderer::composeFace(M5Canvas& target, const std::vector<AvatarLayer>& layers) {
    target.fillScreen(def->backgroundRgb888);
    for (const AvatarLayer& layer : layers) {
        drawLayer(target, layer.name.c_str(),
                  def->faceOriginX + layer.offsetX, def->faceOriginY + layer.offsetY);
    }
}

void FaceRenderer::bakeBandBelow() {
    // バンド座標系（原点=バンド左上）へ描くため、顔の原点からバンドの位置を引く。
    // 不透明ターゲット(16bpp)への drawPng はアルファ合成が正しく効くため、下地は実行時に焼いてよい。
    bandBelow.fillScreen(def->backgroundRgb888);
    for (const AvatarStage& stage : def->stages) {
        if (stage.kind != AvatarStageKind::Below) continue;
        for (const String& name : stage.layers) {
            drawLayer(bandBelow, name.c_str(), -def->band.x, -def->band.y);
        }
    }
}

bool FaceRenderer::loadArgbBin(const char* fileName, uint32_t** dst, size_t count) {
    // 生ARGB(0xAARRGGBB) をリトルエンディアンで count 画素ぶん並べたバイナリを PSRAM へ読み込む。
    // ESP32 はリトルエンディアンなので、バイト列をそのまま uint32 バッファへ読めば復元できる。
    const size_t bytes = count * sizeof(uint32_t);

    *dst = static_cast<uint32_t*>(ps_malloc(bytes));
    if (*dst == nullptr) *dst = static_cast<uint32_t*>(malloc(bytes));
    if (*dst == nullptr) return false;

    fs::File f = assets->fs().open(assets->binPath(fileName), "r");
    if (!f) return false;
    const size_t got = f.read(reinterpret_cast<uint8_t*>(*dst), bytes);
    f.close();
    return got == bytes;
}

FaceRenderer::Offset FaceRenderer::totalOffset(int group) const {
    Offset total{0, 0};
    for (int g = group; g >= 0; g = def->groups[g].parent) {
        total.x += groupOffsets[g].x();
        total.y += groupOffsets[g].y();
    }
    return total;
}

void FaceRenderer::drawBandInto(M5Canvas& target) {
    // 下地をコピー（バンド全面）。
    bandBelow.pushSprite(&target, 0, 0);

    // 各段を定義の順（奥→手前）に重ねる。段の画像は rect 原寸なので、位置をずらせば平行移動になる。
    for (const ArgbStage& s : argbStages) {
        const AvatarStage& stage = *s.stage;
        int x = stage.rect.x - def->band.x;
        int y = stage.rect.y - def->band.y;
        const auto* argb = reinterpret_cast<const m5gfx::argb8888_t*>(s.argb);

        if (stage.kind == AvatarStageKind::Overlay) {
            // 固定の段。動くパーツの上へ重ね直して正しい重ね順を復元する。
            target.pushAlphaImage(x, y, stage.rect.w, stage.rect.h, argb);
            continue;
        }

        const AvatarGroup& group = def->groups[stage.group];
        const Offset move = totalOffset(stage.group);
        x += move.x;
        y += move.y;
        if (group.clipRects.empty()) {
            target.pushAlphaImage(x, y, stage.rect.w, stage.rect.h, argb);
            continue;
        }
        // クリップ矩形ごとに重ねる（clip.mode: components では領域ごとに複数ある）。矩形は親と一緒に動くため、
        // 親の移動量分ずらす。
        // 矩形どうしは重ならない前提（重なると同じ画素を二重に合成してしまう）。
        const Offset parentMove = group.parent >= 0 ? totalOffset(group.parent) : Offset{0, 0};
        for (const AvatarRect& clip : group.clipRects) {
            target.setClipRect(clip.x + parentMove.x - def->band.x, clip.y + parentMove.y - def->band.y,
                               clip.w, clip.h);
            target.pushAlphaImage(x, y, stage.rect.w, stage.rect.h, argb);
        }
        target.clearClipRect();
    }
}

void FaceRenderer::render() {
    if (eyesClosed) {
        // まばたき中: 閉じ顔をそのまま転送する。
        faceClosed.pushSprite(0, 0);
        return;
    }
    // 開き（全画面更新）: 顔全体を転送してからバンドだけ現在のオフセットで再合成して上描きする。
    bgFull.pushSprite(0, 0);
    if (def->hasBand) renderBand();
}

void FaceRenderer::renderBand() {
    drawBandInto(bandCanvas);
    bandCanvas.pushSprite(bandScreenX, bandScreenY);
}

void FaceRenderer::drawStaticFace() {
    if (!ready) return;
    render();
}

void FaceRenderer::lookAt(float x, float y) {
    // 追従先の点(screen px)をそのまま視線制御へ渡す（正規化も補間も GazeController 側の責務）。
    gaze.lookAt(x, y);
}

float FaceRenderer::gazeCenterX() const { return gaze.centerX(); }
float FaceRenderer::gazeCenterY() const { return gaze.centerY(); }

unsigned long FaceRenderer::randomBlinkInterval() const {
    return def->blinkIntervalMinMs +
           (unsigned long)random(def->blinkIntervalMaxMs - def->blinkIntervalMinMs + 1);
}

bool FaceRenderer::updateBlink(unsigned long now) {
    if ((long)(now - nextToggleAt) < 0) return false; // まだ切り替え時刻でない

    if (eyesClosed) {
        eyesClosed = false;
        nextToggleAt = now + randomBlinkInterval();
    } else {
        eyesClosed = true;
        nextToggleAt = now + def->blinkClosedMs;
    }
    return true;
}

bool FaceRenderer::updateGaze() {
    // 全グループを同じ目標方向へ寄せる（可動量と追従の速さの違いは各 GazeOffset が持つ）。
    // 1つでも動いていれば再描画が要るので、途中で打ち切らず全て進めてから判定する。
    const float dirX = gaze.dirX();
    const float dirY = gaze.dirY();
    bool changed = false;
    for (GazeOffset& offset : groupOffsets) {
        changed |= offset.update(dirX, dirY);
    }
    return changed;
}

void FaceRenderer::update() {
    if (!ready) return;

    const unsigned long now = millis();
    const bool blinkChanged = updateBlink(now);
    // 目を閉じている間は動くパーツが見えないので、目標追従の計算だけ行い再描画は促さない。
    const bool gazeChanged = updateGaze() && !eyesClosed;

    if (blinkChanged) {
        // 開閉の切替は全画面更新（閉じ顔への差し替え／開き顔の再構成）。
        render();
    } else if (gazeChanged && def->hasBand) {
        // 視線の変化はバンドの小領域だけ更新して高速化する。
        renderBand();
    }
}
