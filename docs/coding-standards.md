# C++ コーディング規約（C++11 版）

このドキュメントは、本プロジェクトでC++コードを書く際に**必ず従うルール**を定義する。
コーディングエージェントは、コードの生成・修正・レビュー時にこの規約を適用すること。
レビューの進め方・報告形式は [code-review.md](code-review.md) を参照。

- 対象言語バージョン: **C++11（gnu++11）**
- 想定環境: ESP32（Arduino core 2.0 系）/ M5Stack / Arduino フレームワーク
- 規約の強さ:
  - **MUST**: 必ず守る。違反する場合は理由をコメントで明記する
  - **SHOULD**: 原則守る。合理的な理由があれば例外可
  - **MUST NOT**: 禁止

### C++11 を選んだ理由

本プロジェクトは、M5Stack 上で動くアバターアニメーションを作れるプロジェクトとして将来公開する予定である。
利用者の環境（PlatformIO / Arduino IDE、ESP32 Arduino core 2.x / 3.x）を問わず、ビルド設定を変えずに
そのまま使えるようにするため、core 2.x の既定である C++11 に合わせる。

- **MUST NOT** `platformio.ini` で C++ の規格（`-std=`）を変更しない。core 2.x の既定 `gnu++11` でビルドが
  通ることを、C++11 準拠の最低限の確認とする。
- core 2.x でも `build_unflags = -std=gnu++11` + `build_flags = -std=gnu++17` で C++17 にはできる
  （実機環境で確認済み）が、利用者側にも同じ設定を求めることになるため採用しない。

---

## 適用範囲と既存コードの扱い

本規約は既存コード（`lib/` 以下の各モジュール、`src/main.cpp`）より後に制定したため、既存コードは
旧スタイル（インデント4、定数 `UPPER_SNAKE_CASE`、メンバ変数に末尾 `_` なし、
`try` の付かない出力引数関数 など）で書かれている。次の方針で段階的に移行する。

- **MUST** 新規ファイル・新規に追加するコード（クラス・関数・メンバ・定数）は本規約に完全準拠する。
- **MUST** 0章（C++14 以降の機能の禁止）は既存コードを含む全コードに適用する。
- **MUST** 既存コードを修正する際、修正範囲外の規約違反は**勝手に直さず**、指摘として報告する
  （→ 12章）。既存コードの規約への移行は、それ自体を目的とした別作業として行う。
- **SHOULD** 既存ファイルの一部を修正する場合、**フォーマット（インデント・波括弧の位置・アクセス指定子の
  インデント）はそのファイルの既存スタイルに合わせる**。1ファイル内でインデント幅が混在するのを避けるため。
  命名など、それ以外の規則は追加・変更したコードに本規約を適用する。
- **MUST** 既存モジュールの既存ファイル名・既存の公開識別子は、
  移行作業以外では改名しない。

---

## 0. 言語バージョンの制約（最重要）

**MUST NOT** C++14 以降の機能を使わない。以下は使用禁止とし、右欄の代替手段を使う。

| 使用禁止の機能 | 導入 | 代替手段 |
|---|---|---|
| `std::make_unique` | C++14 | `util::makeUnique`（→ 6章） |
| `std::optional` | C++17 | `bool` を返し出力引数に書き込む（→ 4章） |
| `std::string_view` | C++17 | `const char*`（+ 長さ）または `const std::string&` |
| `std::clamp` | C++17 | `std::min(std::max(v, lo), hi)` または自前の関数 |
| `std::size` / `std::data` / `std::empty` | C++17 | `std::array::size()`、配列は `sizeof(a) / sizeof(a[0])` を定数化 |
| `std::exchange` | C++14 | 一時変数を使って書く |
| `std::enable_if_t` など `_t` / `_v` 付きの型特性 | C++14/17 | `typename std::enable_if<...>::type` / `std::is_same<...>::value` |
| ネストした名前空間 `namespace a::b {}` | C++17 | `namespace a { namespace b { } }` |
| 構造化束縛 `auto [a, b] = ...` | C++17 | メンバに個別アクセス |
| `if constexpr` / `if (init; cond)` | C++17 | 通常の `if`、テンプレートの特殊化 |
| `[[nodiscard]]` / `[[maybe_unused]]` | C++17 | コメントで意図を明記、未使用変数は `(void)x;` |
| 関数の戻り値型の推論 `auto f() {...}` | C++14 | 戻り値型を明記する |
| ジェネリックラムダ `[](auto x){}` | C++14 | 型を明記する |
| ラムダの初期化キャプチャ `[x = std::move(y)]` | C++14 | 通常のキャプチャ、または関数オブジェクト |
| 桁区切り `1'000'000` | C++14 | 使わない |
| 2進数リテラル `0b1010` | C++14 | 16進数 `0x0A` を使う |
| メッセージなし `static_assert(cond)` | C++17 | `static_assert(cond, "理由")` |
| 複数文の `constexpr` 関数 | C++14 | `return` 1文のみで書く（三項演算子・再帰で表現） |
| 変数テンプレート `template<class T> constexpr T pi = ...` | C++14 | `static constexpr` メンバ関数・定数 |
| `inline` 変数 | C++17 | 名前空間スコープの `constexpr` 定数（→ 下記） |

**C++11 で使ってよい主な機能**: `auto`（変数）、範囲 for、`nullptr`、`enum class`、`constexpr`（上記制約付き）、`override` / `final`、`= default` / `= delete`、`std::unique_ptr`、`std::array`、`std::atomic`、ラムダ（型明記）、`static_assert`（メッセージ付き）、統一初期化 `{}`、メンバ変数のデフォルト初期化、委譲・継承コンストラクタ。

### GCC が gnu++11 でも通してしまう機能に注意

core 2.x の GCC（8.4）は、`-std=gnu++11` でも一部の C++14/17 の機能を独自拡張として受け入れる
（実機環境の GCC で確認済み）。ビルドが通っても C++11 準拠とは限らない。

| コンパイラの反応 | 該当する機能 |
|---|---|
| 警告のみで通る | 構造化束縛、`if constexpr`、`if (init; cond)`、ラムダの初期化キャプチャ |
| 何も出ずに通る | ネストした名前空間、2進数リテラル、メッセージなし `static_assert`、`[[nodiscard]]` |

- **MUST** 自分たちのコード（`lib/` の自作モジュール、`src/`）から出るコンパイラ警告は残さない（→ 8章）。
- 何も出ずに通る4つは、レビューで検出する（検出方法は [code-review.md](code-review.md)）。

### C++11 特有の注意

- **MUST** クラスの `static constexpr` データメンバを参照で渡す（`std::min(x, kMax)` など、`const T&` を
  受け取る関数に渡す）場合は、`.cpp` に定義（`constexpr int Foo::kMax;`）を置く。C++11 では定義が無いと
  リンクエラーになる。最適化の具合で出たり出なかったりするため、ビルドが通っても安心しない。
  **SHOULD** 迷う場合はクラスの外、名前空間スコープの `constexpr` 定数にする（こちらは定義不要）。
- `constexpr` メンバ関数は C++11 では暗黙に `const` になる。オブジェクトを変更するメンバ関数には付けない。

---

## 1. 命名規則

| 対象 | 規則 | 例 |
|---|---|---|
| クラス・構造体・enum | PascalCase | `SensorReader` |
| 関数・メソッド | camelCase | `readValue()` |
| 変数・引数 | camelCase | `sampleCount` |
| メンバ変数 | camelCase + 末尾 `_` | `buffer_` |
| 定数・`constexpr` | `k` + PascalCase | `kMaxRetry` |
| enum の値 | PascalCase | `State::Running` |
| マクロ | UPPER_SNAKE_CASE | `DEBUG_LOG` |
| 名前空間 | lower_snake_case | `app`, `sensor` |
| モジュール（`lib/` 直下のディレクトリ） | PascalCase | `GazeController` |
| ファイル名 | PascalCase（クラスを定義するファイルはクラス名と一致させる） | `SensorReader.h` |

- **MUST** 上表の規則に従う。
- モジュール名・ファイル名を PascalCase にするのは、M5Unified・M5GFX・ArduinoJson など Arduino 系ライブラリの
  慣習（`#include <M5Unified.h>`）に合わせ、公開時に利用者が見慣れた形にするため。
- **MUST** 識別子（変数・関数・クラス・ファイル名）は英語で付ける。
- **MUST** bool 型の変数・関数は `is` / `has` / `can` / `should` で始める。（例: `isReady`, `hasError()`）
- **MUST** 失敗し得る処理で出力引数を使う関数は `try` で始める。（例: `tryReadSensor()`、→ 4章）
- **SHOULD** 略語を乱用しない。（`cnt` → `count`, `tmp` → 意味のある名前）
- **SHOULD** マクロは原則使わない（→ 3章）。

```cpp
namespace app {
namespace sensor {

constexpr int kMaxRetry = 3;

class SensorReader {
 public:
  bool isReady() const;
  int readValue();

 private:
  int sampleCount_ = 0;
};

}  // namespace sensor
}  // namespace app
```

---

## 2. ファイル構成

- **MUST** ヘッダは `.h`、実装は `.cpp` とする（Arduino のエントリポイント `.ino` は除く）。Arduino 系ライブラリの慣習に合わせる。
- **SHOULD** 1クラス1ファイル（ヘッダ・実装の組）を基本とする。
- **MUST** 新規モジュールは `lib/<ModuleName>/<ModuleName>.h` + `.cpp` の構成とする
  （PlatformIO / Arduino の「ライブラリ名/ライブラリ名」の慣例）。
- **SHOULD** モジュール内でファイルを分類するサブディレクトリは小文字にする（例: `utility/`）。
- **MUST** インクルードガードは `#pragma once` に統一する。
- **MUST NOT** ヘッダファイル内で `using namespace` を書かない。
- **MUST** インクルードは以下の順に、グループ間を空行で区切って並べる。
  1. 対応するヘッダ（`Foo.cpp` なら `Foo.h`）
  2. C / C++ 標準ライブラリ
  3. 外部ライブラリ（`M5Unified.h` など）
  4. プロジェクト内ヘッダ

```cpp
#include "SensorReader.h"

#include <array>
#include <cstdint>

#include <M5Unified.h>

#include "config/Pins.h"
```

---

## 3. 型・変数

- **MUST** 変数は宣言時に初期化する。統一初期化 `{}` を推奨する。
- **MUST** 変更しない変数・引数・メソッドには `const` を付ける。
- **MUST NOT** マジックナンバーを使わない。`constexpr` の名前付き定数にする。
- **MUST NOT** `#define` で定数を定義しない。`constexpr` を使う。
- **MUST** 列挙型は `enum class` を使う。素の `enum` は使わない。
- **MUST** ポインタの null は `nullptr` を使う。`NULL` / `0` は使わない。
- **SHOULD** `auto` は右辺から型が明らかな場合（`makeUnique`、イテレータ、キャスト結果など）に限る。
- **SHOULD** ビット幅が意味を持つ箇所では `int32_t` / `uint8_t` などの固定幅整数型を使う。

```cpp
// NG
#define TIMEOUT 500
if (elapsed > 500) { ... }

// OK
constexpr uint32_t kTimeoutMs = 500;
if (elapsed > kTimeoutMs) { ... }
```

**注意（C++11 の集成体初期化）**: メンバにデフォルト初期化子（`int x = 0;`）を持つ構造体は、C++11 では集成体とみなされず `Foo{1, 2}` で初期化できない。`{}` で値を渡して初期化する構造体にはデフォルト初期化子を付けない。

- **MUST** デフォルト初期化子を持たない構造体の変数は、`Reading reading{};` のように `{}` で宣言する
  （全メンバがゼロで初期化される）。`Reading reading;` はメンバが未初期化になる。

```cpp
// C++11 では Reading{25.0f, 60.0f} がコンパイルエラーになる
struct Reading {
  float temperature = 0.0f;
  float humidity = 0.0f;
};

// OK: 集成体として初期化できる
struct Reading {
  float temperature;
  float humidity;
};

Reading a{25.0f, 60.0f};  // 値を指定
Reading b{};              // 全メンバ 0
```

---

## 4. 関数

- **SHOULD** 1関数1責務とし、目安40行以内に収める。
- **SHOULD** 引数は4個以下とし、それを超える場合は構造体にまとめる。
- **MUST** 引数の渡し方は以下に従う。

| ケース | 渡し方 |
|---|---|
| 小さい型（組み込み型、ポインタ等） | 値渡し `T` |
| 大きい型・読み取り専用 | `const T&` |
| 文字列（読み取り専用） | `const char*` または `const std::string&` |
| 所有権を受け取る | 値渡し `T` + `std::move` |
| 出力 | 原則**戻り値**で返す |

- **SHOULD** 複数の戻り値は構造体で返す。
- **MUST** 失敗し得る処理は、`bool`（成功で `true`）を返し、結果を出力引数（参照）に書き込む。関数名は `try` で始める。これが出力引数を使ってよい唯一のケースとする。
- **MUST** 失敗時は出力引数を変更しない。
- **SHOULD** 早期リターン（ガード節）を使い、ネストは3段以内に抑える。

```cpp
struct Reading {
  float temperature;
  float humidity;
};

bool tryReadSensor(Reading& out) {
  if (!isReady()) {
    return false;
  }
  out = Reading{readTemperature(), readHumidity()};
  return true;
}

// 呼び出し側
Reading reading{};
if (tryReadSensor(reading)) {
  show(reading);
}
```

---

## 5. クラス設計

- **MUST** アクセス指定子は `public` → `protected` → `private` の順に並べる。
- **MUST** メンバ変数は `private` にする（単純なデータ集約の `struct` を除く）。
- **MUST** 引数1つのコンストラクタには `explicit` を付ける。
- **MUST** 仮想関数をオーバーライドする際は `override` を付ける（`virtual` の重複記述はしない）。
- **MUST** 継承されることを想定したクラスのデストラクタは `virtual` にする。
- **SHOULD** 継承を想定しないクラスには `final` を付ける。
- **SHOULD** Rule of Zero を基本とする。リソースを直接管理する場合のみ Rule of Five（コピー/ムーブのコンストラクタ・代入演算子、デストラクタ）を揃える。
- **MUST** コピーを想定しないクラスは `= delete` で明示的に禁止する。

```cpp
class Display {
 public:
  explicit Display(uint8_t brightness) : brightness_(brightness) {}
  virtual ~Display() = default;

  Display(const Display&) = delete;
  Display& operator=(const Display&) = delete;

  virtual void draw() = 0;

 private:
  uint8_t brightness_;
};

class LcdDisplay final : public Display {
 public:
  using Display::Display;  // 継承コンストラクタ（C++11）
  void draw() override;
};
```

---

## 6. メモリ・リソース管理

- **MUST NOT** 生の `delete` を使わない。
- **MUST NOT** 生の `new` を使わない。唯一の例外は下記 `util::makeUnique` の実装内部とする。
- **MUST** 動的確保が必要な場合は `std::unique_ptr` を使い、生成には `util::makeUnique` を使う。
- **SHOULD** `std::shared_ptr` は所有権の共有が本当に必要な場合に限る。
- **MUST** ファイル・ロック・ハードウェアハンドルなどのリソースは RAII で管理する。
- **MUST** 生ポインタは「所有しない参照」の意味でのみ使う。null にならないなら参照 `T&` を優先する。
- **MUST NOT** C スタイルキャスト `(int)x` を使わない。`static_cast` / `reinterpret_cast` などを使う。

`std::make_unique` は C++14 のため、以下のヘルパーをプロジェクトに1つだけ用意して使う（配列版は用意しない。配列は `std::array` を使う）。
ヘルパーは最初に必要になった時点で作成する。

```cpp
// util/MakeUnique.h
#pragma once

#include <memory>
#include <utility>

namespace util {

template <typename T, typename... Args>
std::unique_ptr<T> makeUnique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

}  // namespace util
```

```cpp
// 使用例
auto display = util::makeUnique<LcdDisplay>(kDefaultBrightness);
```

---

## 7. エラー処理

- **MUST NOT** エラーを握りつぶさない。必ずログ出力またはハンドリングを行う。
- **MUST NOT** 例外（`throw` / `try` / `catch`）を使わない。エラーは戻り値で表現する（→ 4章）。
- **SHOULD** `assert` は開発時の前提条件チェックにのみ使い、実行時エラー処理の代わりにしない。
- **SHOULD** コンパイル時に検証できる前提は `static_assert(条件, "理由")` で表明する。

---

## 8. フォーマット・警告

- **MUST** フォーマットは `clang-format` で自動整形する。手動で揃えない。
  （リポジトリに `.clang-format` が整備されるまでは、本章の規則と本規約のコード例の見た目に合わせる）
- **MUST** インデントはスペース2つ（タブ禁止）。
- **SHOULD** 1行は100文字以内。
- **MUST** `if` / `for` / `while` の本体が1行でも波括弧 `{}` を省略しない。
- **MUST** 自分たちのコード（`lib/` の自作モジュール、`src/`）から出るコンパイラ警告は残さない。
  C++14 以降の機能が警告だけで通ってしまうのを防ぐため（→ 0章）。外部ライブラリ・フレームワークの警告は対象外。
- **SHOULD** 静的解析に `clang-tidy` を使い、警告を残さない（`modernize-*` のうち C++14 以降を要求するチェックは無効化する）。

```cpp
// NG
if (isError) return;

// OK
if (isError) {
  return;
}
```

---

## 9. コメント

- **MUST** コメント・ドキュメントは日本語で書く。
- **MUST** 「何をしているか」ではなく「**なぜそうしているか**」を書く。コードを読めば分かることは書かない。
  意図・非自明な制約・ハードウェア固有の注意点（タイミング、メモリ制約など）を中心に簡潔に書く。
- **SHOULD** 公開 API（ヘッダで公開するクラス・関数）には Doxygen 形式で説明を書く。
- **MUST** TODO は `// TODO(担当者): 内容` の形式で書く。

```cpp
/// @brief センサーから温湿度を読み取る
/// @param[out] out 成功時に読み取り結果を格納する。失敗時は変更しない
/// @return 読み取りに成功した場合 true
bool tryReadSensor(Reading& out);

// センサーの仕様上、起動直後50msは値が不安定なため待機する
constexpr uint32_t kWarmupMs = 50;
```

---

## 10. 組込み（M5Stack / ESP32）向けルール

### メモリ
- **MUST NOT** `loop()` 内やホットパスで動的確保を行わない（`makeUnique`、`std::string` の連結、`std::vector` の伸長を含む）。
- **MUST** 大きなバッファはスタックに置かず、静的領域に確保するか起動時に事前確保する。
- **SHOULD** Arduino の `String` は使わず、固定長バッファ（`std::array<char, N>` / `char[N]`）と `snprintf` を使う。

### タイミング
- **MUST NOT** `loop()` 内で長時間の `delay()` によるブロッキングを行わない。
- **MUST** 周期処理は `millis()` ベースのノンブロッキング方式で実装する。

```cpp
constexpr uint32_t kReadIntervalMs = 1000;
uint32_t lastReadMs = 0;

void loop() {
  M5.update();
  const uint32_t now = millis();
  if (now - lastReadMs >= kReadIntervalMs) {  // 引き算で比較し、millis() のオーバーフローに対応する
    lastReadMs = now;
    Reading reading{};
    if (tryReadSensor(reading)) {
      show(reading);
    }
  }
}
```

### 割り込み
- **MUST** 割り込みハンドラは最小限の処理（フラグ設定など）に留める。
- **MUST** ESP32 の割り込みハンドラには `IRAM_ATTR` を付ける。
- **MUST** 割り込みとメイン処理で共有する変数は `volatile` または `std::atomic` にする。

### ハードウェア抽象化
- **MUST** ピン番号・I2C アドレス・通信速度などのハードウェア定数は `constexpr` で1ファイル（例: `config/Pins.h`）に集約する。
- **SHOULD** ハードウェア依存部分（センサー・ディスプレイ等）はクラスで抽象化し、アプリケーションロジックと分離する。

---

## 11. プロジェクト固有ルール（Avator）

- **MUST NOT** コードにサンプルアバターのパーツ名・座標を書かない。パーツ名から役割（重ね順・グループ・
  瞼など）を推測しない。パーツ構成と動かし方はすべて定義ファイル `avatar.json` から得る
  （[avatar-definition.md](avatar-definition.md)）。
- **MUST** モジュール間は public メソッドを介してのみやり取りし、依存の向きは
  [architecture.md](architecture.md) の分離方針（例: `GazeController` は標準ライブラリ以外に依存しない）を守る。
- **SHOULD** 不要な抽象化・将来のための先回り実装は避け、今のスコープに必要な実装に留める。

---

## 12. エージェントへの作業指示

- 新規コードは本規約に完全準拠して生成すること。特に **0章（C++14 以降の機能の禁止）** を最優先で守ること。
- ライブラリのサンプルや外部資料のコードを流用する場合も、C++14 以降の機能が含まれていれば C++11 で書き直すこと。
- 既存コードを修正する際、修正範囲外の規約違反は**勝手に直さず**、指摘として報告すること。
- 規約に反する実装が必要な場合は、その箇所にコメントで理由を明記し、作業報告でも言及すること。
- 規約で判断できない場合は、[C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) → [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) の順に参照し、C++11 で実現できる形に落とし込むこと。
- コードレビューを依頼された場合は [code-review.md](code-review.md) の手順・報告形式に従うこと。
