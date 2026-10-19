// 【レガシー参考実装】WiFiスキャナーデモ
//
// これはプロジェクト初期の main.cpp（WiFiスキャン結果表示・電源アイコンのタッチ判定）を
// そのまま退避したもの。docs/legacy-code.md の方針に従い、削除せず参考実装として保持する。
// examples/ 以下は PlatformIO の通常ビルド対象外のため、ここに置いてもファームウェアには含まれない。
// M5.Touch によるタッチ判定や M5.Power.powerOff() の使い方を参照する際に利用する。

#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>

// 電源アイコンの矩形領域（右上）
constexpr int iconX = 270;
constexpr int iconY = 10;
constexpr int iconSize = 36;

// 起動時の待機画面
void drawIdleScreen() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextColor(WHITE);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Press BtnA");
    M5.Display.println("to scan WiFi");
}

// スキャン結果一覧の表示
void drawResultScreen(int n) {
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);

    if (n == 0) {
        M5.Display.setTextSize(2);
        M5.Display.println("No networks found");
        return;
    }

    M5.Display.setTextSize(1);
    M5.Display.printf("Found %d networks\n\n", n);

    // 電波強度が強い順にソート
    int indices[n];
    for (int i = 0; i < n; i++) indices[i] = i;
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (WiFi.RSSI(indices[i]) < WiFi.RSSI(indices[j])) {
                int tmp = indices[i];
                indices[i] = indices[j];
                indices[j] = tmp;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        int idx = indices[i];
        String ssid = WiFi.SSID(idx);
        int rssi = WiFi.RSSI(idx);
        bool encrypted = (WiFi.encryptionType(idx) != WIFI_AUTH_OPEN);

        M5.Display.printf("%2d) %-20s %4ddBm %s\n",
                          i + 1,
                          ssid.c_str(),
                          rssi,
                          encrypted ? "[Lock]" : "");
    }

    M5.Display.setCursor(0, M5.Display.height() - 12);
    M5.Display.println("Press BtnA to rescan");
}

// スキャン中の表示
void drawScanningScreen() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextColor(WHITE);
    M5.Display.setTextSize(2);
    M5.Display.println("Scanning...");
}

void drawPowerIcon() {
  int cx = iconX + iconSize / 2;
  int cy = iconY + iconSize / 2;
  M5.Display.fillCircle(cx, cy, iconSize / 2, TFT_RED);
  M5.Display.fillRect(cx - 2, cy - iconSize / 2 - 2, 4, iconSize / 2, TFT_BLACK);
  M5.Display.drawCircle(cx, cy, iconSize / 2, TFT_WHITE);
}

bool isPowerIconHit(int x, int y) {
  return x >= iconX &&
        x <= iconX + iconSize &&
        y >= iconY &&
        y <= iconY + iconSize;
}

// WiFiスキャンを実行し、結果を返す
int runWifiScan() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int n = WiFi.scanNetworks();
    return n;
}

// ボタンA押下時のハンドラ（スキャン実行→表示までの一連の流れ）
void onButtonAPressed() {
    drawScanningScreen();
    int n = runWifiScan();
    drawResultScreen(n);
    WiFi.scanDelete(); // メモリ解放
}

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  M5.begin(cfg);
  // put your setup code here, to run once:
  drawIdleScreen();
  drawPowerIcon();
}

void loop() {
  M5.update();
  auto t = M5.Touch.getDetail();

  if (M5.BtnA.wasPressed()) {
      onButtonAPressed();
  }

  if (t.wasPressed()) {
    Serial.printf("Touch: x=%d y=%d\n", t.x, t.y);
    if (isPowerIconHit(t.x, t.y)) {
      Serial.println("Power icon pressed!");
      // ここで電源オフ処理を実行する
      M5.Display.fillRect(0, 0, M5.Display.width(), M5.Display.height(), TFT_BLACK);
      M5.Display.setCursor(10, 10);
      M5.Display.setTextColor(TFT_WHITE);
      M5.Display.setTextSize(2);

      Serial.println("Entering deep sleep (simulated power off)");
      delay(100); // シリアル送信を待つ
      M5.Power.powerOff(); // 実際の電源オフ処理
    }
  }

  delay(10);
}
