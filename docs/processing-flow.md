# 処理フロー（現行実装の解説）

`src/main.cpp` + `lib/` の現行実装が、起動から毎フレームの描画まで何をしているかを図で示す。
方針の「なぜ」は [architecture.md](architecture.md) / [rendering.md](rendering.md)、
チューニング定数の意味は [spec/face-avatar/eye-tracking.md](../spec/face-avatar/eye-tracking.md) を参照。

---

## 1. モジュール構成と依存の向き

`main.cpp` は「入力(TouchInput / ImuInput) → 調停(EyeTracker) → 視線(FaceRenderer)」の橋渡しに徹する。
`FaceRenderer` はタッチもIMUも知らず、追従先の**座標**だけを受け取る（入力源を差し替えられる境界）。
さらに視線の計算そのものは `GazeController` が持ち、`FaceRenderer` は求まったオフセットで描くだけ。

```mermaid
flowchart LR
    subgraph HW["ハードウェア / M5Unified"]
        TOUCH["M5.Touch<br>静電容量タッチ"]
        IMU["M5.Imu<br>6軸IMU"]
        DISP["M5.Display<br>320x240"]
        FS["LittleFS<br>フラッシュ"]
    end

    subgraph APP["アプリケーション"]
        MAIN["src/main.cpp<br>setup / loop"]
        TI["TouchInput<br>タッチ座標の取得"]
        II["ImuInput<br>傾き → 座標"]
        ET["EyeTracker<br>追従先の調停"]
        FR["FaceRenderer<br>合成・まばたき・描画"]
        GC["GazeController<br>座標 → 方向 → px"]
        AD["AvatarDefinition<br>avatar.json の読み込み"]
        AL["AssetLoader<br>マウント・パス解決"]
    end

    TOUCH -->|"getDetail x,y"| TI
    IMU -->|"getAccel ax,ay,az"| II
    TI -->|"getPoint x,y"| MAIN
    II -->|"getPoint x,y"| MAIN
    MAIN -->|"setFocusTarget x,y 注目"| ET
    MAIN -->|"setBaseTarget x,y 既定"| ET
    ET -->|"getTarget x,y"| MAIN
    MAIN -->|"lookAt x,y"| FR
    MAIN -->|"update"| FR
    FR -->|"lookAt x,y / dirX,dirY"| GC
    GC -->|"オフセット px"| FR
    FR -->|"pushSprite"| DISP
    FR -->|"drawPngFile / open"| AL
    MAIN -->|"load"| AD
    AD -.->|"パーツ構成・動かし方"| FR
    AD -->|"open avatar.json"| FS
    AL --> FS
    FR -.->|"gazeCenterX/Y 中立点"| ET
    FR -.->|"gazeCenterX/Y 基準点"| II
```

- `TouchInput` / `ImuInput` はどちらも「いまの点(x,y)」を返すだけの入力モジュール。デバイス固有の事情
  （押下判定、加速度の軸・符号・感度）はそれぞれの中に閉じている。
- `EyeTracker` は点がどこから来たかを知らず、受け取った点の**優先度と保持**だけを決める。どの入力を
  注目(優先)／既定に割り当てるかを決めるのは `main` の役目（現在はタッチ＝注目、傾き＝既定）。
- `FaceRenderer` の入口は `lookAt(x, y)` と `update()` の 2 つだけ。受け取った点はそのまま
  `GazeController` へ渡し、返ってきたオフセット(px)で描く。
- `GazeController`（+ `GazeOffset`）は**標準ライブラリだけに依存する純粋な計算**で、他プロジェクトへ
  そのまま流用できる（[lib/GazeController/README.md](../lib/GazeController/README.md)）。
- アバター固有の情報（パーツ構成・重ね順・動かし方・計測値）は `AvatarDefinition` が起動時に
  `avatar.json` から読む。`FaceRenderer` は特定のパーツ名や座標を持たず、その内容に従って描く
  （[avatar-definition.md](avatar-definition.md)）。
- ファイルパスは `AssetLoader` に集約し、他モジュールは持たない。

---

## 2. 起動処理 `setup()`

重い処理（PNGデコード・生ARGBの読み込み）は**すべて起動時に済ませる**。実行時は合成済みの
ビットマップを動かすだけにするのが基本設計。

```mermaid
sequenceDiagram
    participant M as main.cpp
    participant AL as AssetLoader
    participant AD as AvatarDefinition
    participant FR as FaceRenderer
    participant ET as EyeTracker
    participant II as ImuInput
    participant D as M5.Display

    M->>M: M5.begin
    M->>AL: begin
    AL->>AL: LittleFS.begin false
    alt マウント失敗
        AL-->>M: false
        M->>D: showFatal "FS ERROR / Run uploadfs"
        Note over M: setup 終了（顔は出さない）
    else 成功
        AL-->>M: true
        M->>AD: load /assets/avatar/avatar.json
        alt 読み込み・検証に失敗
            AD-->>M: false
            M->>D: showFatal "DEF ERROR / 原因"
            Note over M: setup 終了（顔は出さない）
        else 成功
            AD-->>M: true
            M->>FR: begin AssetLoader, AvatarDefinition
            FR->>FR: PSRAMへスプライト確保
            Note right of FR: bgFull / faceClosed<br>bandCanvas / bandBelow（バンドは定義の大きさ）
            FR->>AL: 各段の stage_*.bin
            AL-->>FR: 生ARGB を PSRAM へ
            FR->>FR: composeFace 開き顔・閉じ顔を1回だけ合成
            FR->>FR: bakeBandBelow バンドの下地を1回だけ合成
            FR-->>M: true
            M->>FR: gazeCenterX / gazeCenterY
            FR-->>M: 定義の中立点（画面座標）
            M->>ET: begin centerX, centerY
            M->>II: begin centerX, centerY
            Note over II: 傾き0のときの基準点
            M->>FR: drawStaticFace
            FR->>D: 初期表示
        end
    end
```

定義ファイルの読み込みに失敗した場合（ファイルが無い・`schemaVersion` が対応外・必要なキーの欠落）は
`DEF ERROR` と原因を表示する。確保に失敗した場合（PSRAM不足・binの欠損）は `MEM ERROR` を表示して `ready` を false のままにし、
以後 `update()` は何もしない。

---

## 3. 毎フレーム `loop()`

`delay(16)` でおおよそ 60fps のペースで回すが、**描画は状態が変化したフレームだけ**行う。

```mermaid
sequenceDiagram
    autonumber
    participant M as main.cpp
    participant TI as TouchInput
    participant II as ImuInput
    participant ET as EyeTracker
    participant FR as FaceRenderer
    participant D as M5.Display

    loop 約16msごと
        M->>M: M5.update
        M->>TI: getPoint x, y
        opt タッチされている
            TI-->>M: true + タッチ座標
            M->>ET: setFocusTarget x, y 注目点
        end
        M->>II: getPoint x, y
        opt IMUが読めた
            II-->>M: true + 傾きから求めた座標
            M->>ET: setBaseTarget x, y 既定の追従先
        end
        M->>ET: getTarget x, y
        alt 直近のタッチ点が保持期間内
            ET-->>M: true + タッチ点
            M->>FR: lookAt x, y
        else 傾きの点がある
            ET-->>M: true + 傾きの点
            M->>FR: lookAt x, y
        else どちらの入力も無い
            ET-->>M: false
            Note over M: lookAt を呼ばず前回の目標を維持
        end
        Note over FR: lookAt は GazeController の目標方向を更新するだけ
        M->>FR: update
        FR->>FR: updateBlink / updateGaze
        opt 変化あり
            FR->>D: 全画面 または 目バンドを転送
        end
        M->>M: delay 16
    end
```

---

## 4. `FaceRenderer::update()` の分岐

まばたきの開閉が切り替わったときだけ全画面を転送し、視線の動きだけなら目バンド（大きさは
定義ファイルの `band`。現在のアバターで 178x102）の小領域で済ませる。全画面転送 約55ms に対しバンド転送は約11ms（[rendering.md](rendering.md) の実測）。

```mermaid
flowchart TD
    A["update"] --> B{"ready?"}
    B -- いいえ --> Z["何もしない"]
    B -- はい --> C["updateBlink now"]
    C --> D["updateGaze"]
    D --> E{"開閉が切り替わった?"}
    E -- はい --> F["render 全画面更新"]
    E -- いいえ --> G{"視線オフセットが<br>整数px単位で変化?<br>かつ 目が開いている"}
    G -- はい --> H["renderBand 目バンドのみ更新"]
    G -- いいえ --> Z2["転送しない"]

    F --> F1{"eyesClosed?"}
    F1 -- はい --> F2["faceClosed をそのまま転送"]
    F1 -- いいえ --> F3["bgFull 転送 → drawBandInto → バンド転送"]
    H --> H1["drawBandInto → バンド転送"]
```

- 動くグループが無いアバター（`band` が無い）では視線による再描画はせず、まばたきだけを行う。
- 目を閉じている間は瞳も白目も見えないため、`updateGaze()` の計算だけ進めて再描画はしない
  （`gazeChanged = updateGaze() && !eyesClosed`）。
- 変化の判定は **float の現在値ではなく `lroundf` した整数px**で行う。1px 未満の揺れでは転送しない。

---

## 5. まばたきの状態遷移

```mermaid
stateDiagram-v2
    [*] --> Open: begin 開き状態で開始
    Open --> Closed: now >= nextToggleAt<br>次回 = now + closedMs
    Closed --> Open: now >= nextToggleAt<br>次回 = now + intervalMs の範囲でランダム
    note right of Open
        開き: bgFull + 目バンド合成
    end note
    note right of Closed
        閉じ: faceClosed を丸ごと転送
        show closed のレイヤー（瞼）を
        定義の重ね順で焼き込み済み
    end note
```

閉じ顔を事前合成してあるので、まばたきは「別スプライトへの差し替え」だけで済む。
時間は定義ファイルの `config.blink`（現在のアバターは closedMs 180ms / intervalMs 2500〜6000ms）。

---

## 6. 視線の値の流れ（タッチ・IMU → 画面上のpx）

入力から描画オフセットまでの変換は 3 段階。段ごとに責務が分かれている。

```mermaid
flowchart LR
    subgraph S1["1. 入力 → 追従先の座標"]
        A0["TouchInput<br>タッチ点 x, y 注目"] --> A2
        A1["ImuInput<br>ax, az 単位g"] --> A3["x = neutralX - ax * 1200<br>y = neutralY + az * 360"]
        A3 --> A2["EyeTracker: 追従先の点<br>注目点が優先"]
    end
    subgraph S2["2. GazeController::lookAt: 座標 → 目標方向"]
        B1["追従先 x, y"] --> B2["dir = x - 中立点 / reach<br>-1..1 にクランプ"]
    end
    subgraph S3["3. GazeOffset::update: 方向 → 各グループのpx"]
        C1["dirX, dirY"] --> C2["pupil 6 / 4 px<br>補間 0.25"]
        C1 --> C3["eyes 白目+眉 8 / 5 px<br>補間 0.20"]
        C1 --> C4["detail 鼻口眼鏡 4 / 3 px<br>補間 0.18"]
    end
    A2 --> B1
    B2 --> C1
    C2 --> D1["描画位置 = eyes + pupil<br>親の移動量を足す"]
    C3 --> D2["描画位置 = eyes"]
    C4 --> D3["描画位置 = detail"]
```

- 段 2・3 は `lib/GazeController/` に閉じており、段 1（点の作り方）と描画のどちらも知らない。
  中立点・`reach`・グループごとの可動量と追従係数はアバター固有の値で、定義ファイルから
  `FaceRenderer::begin()` が渡す。図中の値は現在のアバター（`assets/avatar/avatar.json`）のもの。

- **タッチが最優先**。`TouchInput` が取り出した画面座標は、変換せずそのまま追従先の点になる
  （可動域を超える分は `lookAt` がクランプする）。指を離した後も `FOCUS_HOLD_MS`（1500ms）はその点を
  見続け、その後 `ImuInput` の点へ戻る。
- `ImuInput` は左右が `ax`（IMU軸と画面軸が90°ずれるため符号反転）、上下が `az`（画面法線方向の重力成分）。
  縦に立てる＝正面 / 横に寝かせる＝下 / 逆さ＝上。
- 全グループが同じ向きへ寄るが、可動量（横 6 / 8 / 4 px）と補間係数（0.25 / 0.20 / 0.18）を変えてある。
  瞳がいちばん速く追従し、顔まわりのパーツはより緩やかに遅れて寄るため、顔全体が傾きへ引っぱられて
  見える。
- 瞳（`pupil`）は `parent: eyes` のため、描画位置は **白目グループの移動 + 瞳自身の移動** の合成
  （白目ごと動いた上で、その中を瞳が動く）。

---

## 7. 目バンドの合成（`drawBandInto`）

バンドは「動く段をすべて、可動量分の余白込みで内包する矩形」（`tools/build_avatar.py` が決定し、
定義ファイルの `band` に書く）。毎回バンド全面を下地から作り直すため、移動元に前フレームの残像は残らない。
`drawBandInto` は定義ファイルの `stages` を順に重ねるだけのループで、下図は現在のアバターの段構成。

```mermaid
flowchart TB
    L1["1. below: bandBelow をコピー<br>Ribbon + FaceBase 不透明16bpp"] --> L2
    L2["2. moving eyes: stage_1_eyes.bin 白目+眉<br>eyes の移動量分ずらす<br>FaceBase矩形でクリップ"] --> L3
    L3["3. moving pupil: stage_2_pupil.bin 瞳<br>eyes + pupil の移動量分ずらす<br>左右の白目矩形それぞれでクリップ"] --> L4
    L4["4. moving detail: stage_3_detail.bin 鼻+口+眼鏡<br>detail の移動量分ずらす クリップなし"] --> L5
    L5["5. overlay: stage_4_overlay.bin 髪+お団子+アホ毛<br>固定位置で重ね直す"] --> L6
    L6["bandCanvas をバンドの画面位置へ転送"]
```

- 段の位置 = 段の `rect` の位置 + グループの移動量（親の分を含む）。段の画像は `rect` 原寸なので、
  ずらして描けばそのまま平行移動になる。
- クリップの目的: 白目・眉は `FaceBase` からはみ出さないように、瞳は**左右それぞれの白目矩形**へ。
  左右は鼻筋で分かれているため単一矩形だと隙間へはみ出す（`clip.mode: components`）。
  クリップ矩形は親グループの移動量分ずらすため、白目が寄れば瞳のクリップ範囲も追従する。
- 最上位レイヤーをバンド内で重ね直すのは、`Hair → Glasses → Eye → FaceBase` のZ順を保つため
  （眉の大半・眼鏡の一部は前髪の裏に隠れる）。
- 半透明を含むレイヤーを**生ARGB**で持つ理由: M5GFX の `drawPng` はスプライト宛だと半透明画素の
  アルファを保持しない（読み戻して不透明色で書き戻す）。`pushAlphaImage` なら破綻しない。

---

## 8. 座標系の対応

```mermaid
flowchart LR
    A["マスター素材<br>750x750 px<br>assets/avatar/*.png"] -->|"build_avatar.py で縮小<br>240 × zoom"| B["顔座標<br>384x384 px<br>data/assets/avatar/"]
    B -->|"等倍描画<br>原点 faceOrigin = -32, -72"| C["画面<br>320x240 px"]
```

`zoom`（現在のアバターは 1.6）はアセット側へ焼き込み済みなので実行時の倍率は 1.0。拡大デコード（低速）を
避けるための設計。計測も縮小後の画像で行うため、定義ファイルの座標（`band`・`rect`・`clipRects`・
`gazeCenter`）はすべて顔座標で、画面座標 = 顔座標 + `faceOrigin`。

---

## 9. アセット生成からデバイスまで

```mermaid
flowchart LR
    M["assets/名前/*.png<br>マスター素材"]
    J["assets/名前/avatar.json<br>config はユーザーが編集"]
    M --> G["tools/build_avatar.py 名前"]
    J --> G
    G -->|"measured を書き込む"| J
    G --> D1["data/assets/avatar/<br>avatar.json・*.png・stage_*.bin"]
    G --> P["build/preview/名前.png<br>確認用"]
    D1 --> U["pio run -t uploadfs<br>LittleFS領域へ"]
    FW["src/ ・ lib/"] --> B["pio run -t upload<br>ファーム本体"]
    U --> DEV["M5Stack Core2"]
    B --> DEV
```

ファーム本体はアバターに依存しない（生成ヘッダは無い）。素材・定義ファイルの変更やアバターの差し替えは
`build_avatar.py` → `uploadfs` だけで反映でき、ファームの書き込みはコードを変えたときだけ要る。

---

関連ドキュメント: [アーキテクチャ](architecture.md) / [描画方針](rendering.md) /
[アセット管理](assets.md) / [ビルド・実行](build.md) / [顔アバター実装仕様](../spec/face-avatar/README.md)
