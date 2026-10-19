# 顔アバター 実装仕様（spec/face-avatar）

> ## ⚠️ 移行ステータス（2026-09-03）
>
> 本ディレクトリの仕様は **旧デザイン（シアン単色・フラットな線画のロボット風UI／表情4状態
> neutral・happy・alert・sleep／テレメトリヘッダー／下部3ボタン）** を前提に書かれている。
> プロジェクトは **フルカラーのパーツ分割キャラクターアバターへ全面移行** したため、以下は現状の
> 実装と一致しない。
>
> - **現行の実装済み仕様**: 静止顔の重ね合成。→ [docs/rendering.md](../../docs/rendering.md) /
>   [docs/assets.md](../../docs/assets.md) / `lib/FaceRenderer`・`lib/AssetLoader`。
> - **再設計待ち（本ディレクトリ）**: 新アバターに対する表情状態の定義（そもそも何状態にするか、
>   各状態のパーツをどう用意するか）、まばたき、視線トラッキング、テレメトリ／ボタンUIの要否。
>   新アバター用の表情パーツ（happy/alert/sleep相当）はまだ存在しないため、これらは未確定。
>
> 以降の記述は参考（旧仕様）として残す。

---

このディレクトリは、顔アバターUIの実装レベルの詳細仕様をまとめたものです。
プロジェクト全体の方針は [docs/](../../docs/) 側（[CLAUDE.md](../../CLAUDE.md) から辿れる）に
記載されており、本ディレクトリはその方針の上に立つ「詳細設計」に位置づけられます。

## 元資料

本仕様は以下の元資料をもとに、本プロジェクトの技術方針（M5Unified/M5GFX・画像アセット方式）
向けに翻訳・整理したものです。元資料はそのまま [reference/](reference/) に保存している。

- [reference/microcontroller-face-ui-spec.md](reference/microcontroller-face-ui-spec.md) — 元の実装仕様書
- [reference/wireframe-annotated.svg](reference/wireframe-annotated.svg) — 座標つきワイヤーフレーム
- [reference/spec-canvas.pdf](reference/spec-canvas.pdf) — デザインキャンバス（状態一覧・IMU/タッチ図解）

## 元資料との主な差分（本プロジェクト向けの翻訳方針）

元資料は LVGL-lite を用いたベクター図形描画（円・線・弧をリアルタイム描画し状態間を120ms補間）を
前提としているが、本プロジェクトは [顔グラフィックは画像アセット方式](../../docs/rendering.md) という
既定方針を維持する。そのため、以下の使い分けで見た目を再現する。

| 要素 | 実現方法 | 理由 |
|---|---|---|
| 目・口（表情で変化する部分） | 画像アセット（PNGパーツ） | 顔の表現力に直結するため既定方針どおり画像アセット化 |
| 瞳（視線トラッキングで動く部分） | 画像アセット（小さいスプライトを座標移動） | 形状は固定のまま位置だけ動かせばよく、画像移動で十分再現できる |
| 外側/内側リング、区切り線、ボタン枠、アイコン、ヘッダー文字 | M5GFXのプリミティブ描画（円・線・矩形・テキスト） | 補助UI・チロームであり [docs/rendering.md](../../docs/rendering.md) の許容範囲。既存の電源アイコン実装（`src/main.cpp` の `drawPowerIcon`）と同じ考え方 |

状態間の「120ms補間」は、元資料ではベクター形状そのものの補間（モーフィング）を意味するが、
画像パーツ方式ではモーフィングできない。本プロジェクトでは **パーツのクロスフェード
（旧状態のパーツをフェードアウトしつつ新状態のパーツをフェードインする、120ms間のアルファブレンド）**
で近似する。この近似方法自体が妥当かは実機での見え方次第であり、[open-questions.md](open-questions.md) に
論点として記載する。

## ドキュメント一覧

| ドキュメント | 内容 |
| --- | --- |
| [layout.md](layout.md) | キャンバス全体レイアウト・カラートークン・区切り線 |
| [expressions.md](expressions.md) | 表情4状態（neutral/happy/alert/sleep）とパーツ構成・遷移 |
| [eye-tracking.md](eye-tracking.md) | IMU傾き・タッチ入力による瞳の視線トラッキング |
| [telemetry-header.md](telemetry-header.md) | 上部テレメトリヘッダー（システムステータス・バッテリー） |
| [control-buttons.md](control-buttons.md) | 下部3ボタン（電源・Wi-Fi・停止） |
| [asset-mapping.md](asset-mapping.md) | LittleFS上の画像アセット構成（[docs/assets.md](../../docs/assets.md) の詳細版） |
| [open-questions.md](open-questions.md) | 元資料・本翻訳作業で未確定の論点一覧 |

## スコープ

前回までの憲章では視線トラッキング・テレメトリヘッダー・下部ボタンは将来拡張扱いだったが、
本仕様一式を **初期実装スコープに含める**（[docs/requirements.md](../../docs/requirements.md) 参照）。
