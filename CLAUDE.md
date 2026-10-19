# Avator プロジェクト憲章 (CLAUDE.md)

M5Stack Core2 にキャラクターの顔を表示するプロジェクト「Avator」の要件・アーキテクチャ方針・
コーディング規約は `docs/` ディレクトリに項目ごとに分けて記載している。
このリポジトリで作業する際は、常に以下のドキュメントに従うこと。

C++コードの生成・修正時は [docs/coding-standards.md](docs/coding-standards.md) に、
コードレビューを依頼された場合は加えて [docs/code-review.md](docs/code-review.md) に必ず従うこと。

内容が増えた際も、このファイルは各ドキュメントへの入口として簡潔に保ち、詳細は `docs/` 側に追記する。

## ドキュメント一覧

| ドキュメント | 内容 |
| --- | --- |
| [docs/overview.md](docs/overview.md) | プロジェクト概要（目的・対象ハードウェア・使用ライブラリ） |
| [docs/requirements.md](docs/requirements.md) | 要件・スコープ（初期マイルストーンと将来拡張） |
| [docs/legacy-code.md](docs/legacy-code.md) | 既存コード（WiFiスキャナーデモ）の扱い方針 |
| [docs/architecture.md](docs/architecture.md) | アーキテクチャ方針（ディレクトリ・モジュール構成） |
| [docs/rendering.md](docs/rendering.md) | 描画方針（画像アセット方式、表情切り替え・アニメーション） |
| [docs/processing-flow.md](docs/processing-flow.md) | 処理フロー解説（起動・毎フレーム処理・視線/描画の流れをmermaidで図解） |
| [docs/assets.md](docs/assets.md) | アセット管理（保存先・フォーマット・ディレクトリ構成） |
| [docs/avatar-definition.md](docs/avatar-definition.md) | アバター定義ファイル（avatar.json）と生成ツール・デバイス側読み込みの仕様（他アバター対応） |
| [docs/coding-standards.md](docs/coding-standards.md) | C++コーディング規約（C++11固定、MUST/SHOULD、既存コードの扱い、組込み・プロジェクト固有ルール） |
| [docs/code-review.md](docs/code-review.md) | コードレビュー手順（観点・重大度・既存違反の扱い・報告形式） |
| [docs/build.md](docs/build.md) | ビルド・実行方法 |
| [docs/git-workflow.md](docs/git-workflow.md) | Git運用（現時点は未決定） |
| [docs/porting-cores3.md](docs/porting-cores3.md) | CoreS3 + YOLO(Module LLM) プロジェクトへの移植計画と未確定課題 |
