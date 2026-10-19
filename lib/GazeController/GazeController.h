#pragma once

// 「追従先の点(x,y) を受け取り、視線として動かす各パーツのオフセット(px)を決める」視線制御モジュール。
//
// 入力デバイス（IMU・タッチ・通信など）にも描画方法（画像合成・図形描画）にも依存しない純粋な計算だけを
// 持つため、他プロジェクトへそのまま流用できる。依存は標準ライブラリのみで、M5Unified も Arduino も
// include しない。
//
// 使い方は 2 段階:
//   1. GazeController … 追従先の点を「中立点からの目標方向(-1..1)」へ正規化する（座標系の吸収）。
//   2. GazeOffset     … 目標方向を、動かしたいパーツごとの変位(px)へ写像して滑らかに追従させる。
//                       パーツごとに可動量と追従の速さを変えられる（瞳は速く大きく、顔の細部は緩やかに等）。
//
//   GazeController gaze;
//   GazeOffset pupil;
//   gaze.begin(eyeCenterX, eyeCenterY, reachX, reachY);
//   pupil.begin(6.0f, 4.0f, 0.25f);
//   // 入力側から追従先の点が来たとき
//   gaze.lookAt(x, y);
//   // 毎フレーム
//   if (pupil.update(gaze.dirX(), gaze.dirY())) redraw(pupil.x(), pupil.y());

// 視線に合わせて動く 1 パーツぶんのオフセット(px)。目標方向へ比例配分した値を滑らかに追う。
class GazeOffset {
public:
    // maxX/maxY … 目標方向が振り切った(±1)ときの変位(px)。smooth … 目標へ近づける補間係数(0..1)。
    // 値が大きいほど速く追従する。1.0 で補間なし（即座に目標値）。
    void begin(float maxX, float maxY, float smooth);

    // 目標方向(-1..1)を受けて現在値を1フレーム進める。
    // 描画に使う整数px が変化したときだけ true を返す（1px未満の揺れで再描画させないため）。
    bool update(float dirX, float dirY);

    // 現在のオフセット(px, 整数)。update が返した「変化あり」はこの値の変化を指す。
    int x() const { return drawnX; }
    int y() const { return drawnY; }

private:
    float maxX = 0.0f;
    float maxY = 0.0f;
    float smooth = 1.0f;

    // 補間中の実数値と、そこから丸めた描画用の整数値（変化検出に使う）。
    float curX = 0.0f;
    float curY = 0.0f;
    int drawnX = 0;
    int drawnY = 0;
};

// 追従先の点(px)を、視線の目標方向(-1..1)へ変換する。
class GazeController {
public:
    // centerX/centerY … 視線の中立点(px)。この点を向くと正面（目標方向 0,0）。通常は目の中心。
    // reachX/reachY   … 目標方向が振り切る(±1)までの、中立点からの距離(px)。
    //                   小さいほど少しの変位で大きく目を動かす。
    void begin(float centerX, float centerY, float reachX, float reachY);

    // 追従先の点(px)を指定する。視線制御へ点を渡す唯一の入口で、点がどこから来たか（タッチ・IMU・通信）は
    // 問わない。可動域の外の点（画面外も可）は正規化時に ±1 へクランプされる。
    void lookAt(float x, float y);

    // 直近の lookAt から求めた目標方向（各軸 -1..1）。±1 = その向きへ振り切った状態。
    float dirX() const { return targetDirX; }
    float dirY() const { return targetDirY; }

    // 視線の中立点(px)。入力側（傾きの基準点、入力が無いときの既定値など）へ渡すために公開する。
    float centerX() const { return neutralX; }
    float centerY() const { return neutralY; }

private:
    float neutralX = 0.0f;
    float neutralY = 0.0f;
    float reachX = 1.0f;
    float reachY = 1.0f;

    float targetDirX = 0.0f;
    float targetDirY = 0.0f;
};
