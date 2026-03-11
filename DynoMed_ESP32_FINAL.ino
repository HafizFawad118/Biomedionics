// ╔══════════════════════════════════════════════════════════════╗
// ║          DynoMed Force Sensor — ESP32 Complete Code          ║
// ║                                                              ║
// ║  Hardware:  ESP32 + Load Cell + HX711 + OLED SSD1306        ║
// ║  Cloud:     Firebase Realtime Database                       ║
// ║  Website:   DynoMed_Website.html                            ║
// ║                                                              ║
// ║  OLED aur Serial Monitor DONO same cheez dikhate hain       ║
// ╚══════════════════════════════════════════════════════════════╝

// ─────────────────────────────────────────────────────────────
//  Libraries install karo (Sketch > Manage Libraries):
//
//  1.  "HX711" by bogde
//  2.  "Adafruit SSD1306" by Adafruit
//  3.  "Adafruit GFX Library" by Adafruit
//  4.  "Firebase Arduino Client Library for ESP8266 and ESP32"
//      by Mobizt
// ─────────────────────────────────────────────────────────────

#include <WiFi.h>
#include <FirebaseESP32.h>
#include <HX711.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ╔══════════════════════════════════════════════════════════════╗
// ║   ⚠️  SIRF YE 4 VALUES APNI DAALO — BAKI KUCH MAT BADLO    ║
// ╚══════════════════════════════════════════════════════════════╝

#define WIFI_SSID      "HUAWEI-yJ9e"
//                      ↑ apna WiFi network naam

#define WIFI_PASSWORD  "hammad123"
//                      ↑ apna WiFi password

#define FIREBASE_HOST  "biomedionics-5d664-default-rtdb.firebaseio.com"
//                      ↑ Firebase Console > Realtime Database > URL
//                        (https:// hata ke sirf domain daalo)

#define FIREBASE_AUTH  "Pjfop6VbVGIWrCMYLrooGC7gTC9UlceDS3j3a0Il"
//                      ↑ Firebase Console > Project Settings >
//                        Service Accounts > Database secrets > Show
//                        (ye abhi bhi tumhe Firebase se copy karni hogi)

// ════════════════════════════════════════════════════════════════

// ─── HX711 Wiring ───────────────────────────────────────────
//   HX711 DT  →  ESP32 GPIO 16
//   HX711 SCK →  ESP32 GPIO 17
//   HX711 VCC →  ESP32 3.3V
//   HX711 GND →  ESP32 GND
#define HX711_DOUT  16
#define HX711_SCK   17

// ─── OLED Wiring ────────────────────────────────────────────
//   OLED SDA  →  ESP32 GPIO 21
//   OLED SCL  →  ESP32 GPIO 22
//   OLED VCC  →  ESP32 3.3V
//   OLED GND  →  ESP32 GND

// ─── Load Cell Wiring (to HX711) ────────────────────────────
//   Load Cell Red   →  HX711 E+
//   Load Cell Black →  HX711 E-
//   Load Cell White →  HX711 A+
//   Load Cell Green →  HX711 A-

// ─── Calibration ────────────────────────────────────────────
// Pehli baar:
//   1. CALIBRATION_MODE = true karo
//   2. Upload karo, Serial Monitor kholo (115200 baud)
//   3. Koi weight mat rakho → zero value note karo
//   4. Known weight rakho (e.g. 1kg) → value note karo
//   5. CALIBRATION_FACTOR = (with_weight - zero) / kg
//   6. Wo number neeche daalo, CALIBRATION_MODE = false karo
//   7. Dobara upload karo

#define CALIBRATION_FACTOR   2280.0f   // ← Apne sensor ka factor yahan
#define CALIBRATION_MODE     false     // ← Calibrate karte waqt true karo

// ════════════════════════════════════════════════════════════════
//  CONSTANTS
// ════════════════════════════════════════════════════════════════
#define OLED_WIDTH   128
#define OLED_HEIGHT  64
#define OLED_ADDR    0x3C
#define READING_INTERVAL_MS  1000   // 1 second per reading

// ════════════════════════════════════════════════════════════════
//  OBJECTS
// ════════════════════════════════════════════════════════════════
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
HX711            scale;
FirebaseData     fbdo;
FirebaseAuth     fbAuth;
FirebaseConfig   fbConfig;

// ════════════════════════════════════════════════════════════════
//  GLOBAL VARIABLES
// ════════════════════════════════════════════════════════════════
String deviceID   = "";    // ESP32 MAC address — auto milega
String fbBasePath = "";    // /dynometer/<MAC>
String localIP    = "";
bool   fbReady    = false;

// Loop timing
unsigned long lastReadTime = 0;

// Status tracking (Serial mein repeat na ho)
String lastStatus = "";

// ════════════════════════════════════════════════════════════════
//
//   OLED + SERIAL DUAL PRINT FUNCTION
//
//   Ye function ek saath OLED aur Serial Monitor dono pe
//   same information print karta hai.
//
//   Parameters:
//     h     = header line (top)
//     l2    = second line
//     l3    = third line (bigFont=true hoga to bada)
//     foot  = footer (inverted bar at bottom, "" = skip)
//     bigFont = true: l3 ko size-2 (bada) font mein dikha
//     forceSerial = true: har baar Serial pe print karo
//
// ════════════════════════════════════════════════════════════════
void dualPrint(String h, String l2, String l3, String foot,
               bool bigFont = false, bool forceSerial = false)
{
  // ── OLED ──────────────────────────────────────────────────
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  // Header (line 1)
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print(h);

  // Divider
  oled.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Line 2
  oled.setTextSize(1);
  oled.setCursor(0, 13);
  oled.print(l2);

  // Line 3
  if (bigFont) {
    oled.setTextSize(2);
    oled.setCursor(0, 26);
  } else {
    oled.setTextSize(1);
    oled.setCursor(0, 26);
  }
  oled.print(l3);

  // Footer (inverted bottom bar)
  if (foot.length() > 0) {
    oled.fillRect(0, 53, 128, 11, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
    oled.setTextSize(1);
    oled.setCursor(2, 54);
    oled.print(foot);
    oled.setTextColor(SSD1306_WHITE);
  }

  oled.display();

  // ── SERIAL MONITOR ────────────────────────────────────────
  // Sirf tab print karo jab content change ho (ya forceSerial)
  String combined = h + l2 + l3 + foot;
  if (forceSerial || combined != lastStatus) {
    lastStatus = combined;
    Serial.println(F("┌────────────────────────────────┐"));
    if (h.length()    > 0) Serial.println("│  " + h);
    if (l2.length()   > 0) Serial.println("│  " + l2);
    if (l3.length()   > 0) Serial.println("│  " + l3);
    if (foot.length() > 0) Serial.println("│  [" + foot + "]");
    Serial.println(F("└────────────────────────────────┘"));
  }
}

// ════════════════════════════════════════════════════════════════
//  BLINK ANIMATION — "Connecting" state ke liye
// ════════════════════════════════════════════════════════════════
void blinkConnecting(String idFull) {
  // Frame A — normal
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);   oled.print("  DynoMed Pro");
  oled.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  oled.setCursor(0, 13);  oled.print("  ID: " + idFull);
  oled.setTextSize(1);
  oled.setCursor(0, 28);  oled.print("  >> Connecting...");
  oled.fillRect(0, 53, 128, 11, SSD1306_WHITE);
  oled.setTextColor(SSD1306_BLACK);
  oled.setCursor(2, 54);  oled.print("  Website signal...");
  oled.display();
  delay(500);

  // Frame B — inverted flash
  oled.clearDisplay();
  oled.fillRect(0, 0, 128, 64, SSD1306_WHITE);
  oled.setTextColor(SSD1306_BLACK);
  oled.setTextSize(1);
  oled.setCursor(10, 18);  oled.print(">> CONNECTING <<");
  oled.setCursor(16, 34);  oled.print("Please wait...");
  oled.display();
  delay(350);
}

// ════════════════════════════════════════════════════════════════
//
//   SETUP — ek baar chalta hai power on pe
//
// ════════════════════════════════════════════════════════════════
void setup() {

  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("╔══════════════════════════════════╗"));
  Serial.println(F("║    DynoMed Pro — Starting Up     ║"));
  Serial.println(F("║  Load Cell + HX711 + OLED + FB   ║"));
  Serial.println(F("╚══════════════════════════════════╝"));

  // ── OLED Initialize ──────────────────────────────────────
  Wire.begin(21, 22);   // SDA=21, SCL=22
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("[OLED] ❌ Not found! Check: SDA=21, SCL=22"));
    // Carry on — Serial Monitor pe sab dikhta rahega
  } else {
    Serial.println(F("[OLED] ✓ Initialized (128x64 SSD1306)"));
  }
  oled.clearDisplay();
  oled.display();

  // Boot screen
  dualPrint("  DynoMed Pro", "  v1.0", "  Starting...", "  Please wait...");
  delay(1500);

  // ── STEP 1: MAC Address (Device ID) ──────────────────────
  // WiFi.mode set karna zarori hai MAC ke liye
  WiFi.mode(WIFI_STA);
  delay(100);

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);

  char macBuf[13];
  snprintf(macBuf, sizeof(macBuf),
           "%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  deviceID   = String(macBuf);
  fbBasePath = "/dynometer/" + deviceID;

  // Serial pe clearly print karo
  Serial.println();
  Serial.println(F("╔══════════════════════════════════╗"));
  Serial.println(F("║          YOUR DEVICE ID           ║"));
  Serial.println("║      >>  " + deviceID + "  <<      ║");
  Serial.println(F("║   Enter this ID on the website!   ║"));
  Serial.println(F("╚══════════════════════════════════╝"));
  Serial.println("  Firebase path: " + fbBasePath);
  Serial.println();

  // OLED pe bade font mein 3 baar dikhao (6 seconds)
  String p1 = deviceID.substring(0, 6);   // e.g. A4CF12
  String p2 = deviceID.substring(6, 12);  // e.g. 3B7F2E

  for (int i = 0; i < 3; i++) {
    // Show
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.print("  Your Device ID:");
    oled.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    oled.setTextSize(2);         // BADA FONT
    oled.setCursor(4, 15);
    oled.print(p1);              // A4CF12
    oled.setCursor(4, 36);
    oled.print(p2);              // 3B7F2E

    oled.fillRect(0, 53, 128, 11, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
    oled.setTextSize(1);
    oled.setCursor(2, 54);
    oled.print("Enter on website!");
    oled.display();
    delay(1700);

    // Brief flash off
    oled.clearDisplay();
    oled.display();
    delay(150);
  }

  // ── STEP 2: HX711 / Load Cell ────────────────────────────
  Serial.println(F("[HX711] Initializing Load Cell..."));
  dualPrint("  DynoMed Pro", "  Load Cell:", "  Initializing...", "  Connecting sensor");
  delay(800);

  scale.begin(HX711_DOUT, HX711_SCK);

  // Calibration Mode
  if (CALIBRATION_MODE) {
    Serial.println(F("[CALIB] ===== CALIBRATION MODE ====="));
    dualPrint("  CALIBRATION", "  Remove weight", "  Serial 115200", "  Reading zero...");
    delay(3000);

    scale.tare();
    long zeroVal = scale.read_average(10);

    Serial.println("[CALIB] Zero value (bina weight): " + String(zeroVal));
    Serial.println(F("[CALIB] Ab apna known weight rakho..."));

    dualPrint("  CALIBRATION", "  Zero done!", "  Place weight now", "  Waiting 5 sec...");
    delay(5000);

    long withVal = scale.read_average(10);
    Serial.println("[CALIB] Value with weight: " + String(withVal));
    Serial.println(F("[CALIB] ─────────────────────────────"));
    Serial.println(F("[CALIB] CALIBRATION_FACTOR formula:"));
    Serial.println("[CALIB] (withVal - zeroVal) / weight_kg");
    Serial.println("[CALIB] = (" + String(withVal) + " - " +
                   String(zeroVal) + ") / YOUR_KG");
    Serial.println(F("[CALIB] Wo number upar CALIBRATION_FACTOR mein daalo"));
    Serial.println(F("[CALIB] Phir CALIBRATION_MODE = false karo"));
    Serial.println(F("[CALIB] ─────────────────────────────"));

    dualPrint("  CALIB DONE!", "  Check Serial", "  Calc factor", "  Set false & upload");

    while (true);  // User ko ruk ke calculate karne do
  }

  // Normal mode
  if (!scale.is_ready()) {
    Serial.println(F("[HX711] ❌ Not responding! Check wiring:"));
    Serial.println(F("[HX711]    DT  = GPIO 16"));
    Serial.println(F("[HX711]    SCK = GPIO 17"));
    dualPrint("  HX711 ERROR!", "  Not found!", "  Check wiring", "  DT=16  SCK=17");
    delay(4000);
    // Carry on — baad mein check karte rahe
  } else {
    scale.set_scale(CALIBRATION_FACTOR);
    scale.tare();
    Serial.println(F("[HX711] ✓ Ready!"));
    Serial.println("  Calibration factor: " + String(CALIBRATION_FACTOR));
    dualPrint("  DynoMed Pro", "  Load Cell:", "  Ready! ✓", "  Tare done (zero)");
    delay(1200);
  }

  // ── STEP 3: WiFi Connect ─────────────────────────────────
  Serial.println(F("[WiFi] Connecting..."));
  Serial.println("  SSID: " + String(WIFI_SSID));

  dualPrint("  DynoMed Pro", "  WiFi:", "  Connecting...", String(WIFI_SSID));

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    attempts++;

    // Dots animation on OLED
    String dots = "";
    for (int d = 0; d < (attempts % 4); d++) dots += ".";

    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 0);  oled.print("  DynoMed Pro");
    oled.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    oled.setCursor(0, 13); oled.print("  WiFi: " + String(WIFI_SSID));
    oled.setCursor(0, 26); oled.print("  Connecting" + dots);
    oled.setCursor(0, 40); oled.print("  Attempt: " + String(attempts) + "/40");
    oled.fillRect(0, 53, 128, 11, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
    oled.setCursor(2, 54); oled.print("  Please wait...");
    oled.display();

    if (attempts % 8 == 0) {
      Serial.println("  Trying... " + String(attempts) + "/40");
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[WiFi] ❌ FAILED! Check SSID and Password."));
    dualPrint("  WiFi FAILED!", "  Check:", "  Name & Password", "  Restart device");
    while (true);  // Device restart karni hogi
  }

  localIP = WiFi.localIP().toString();
  Serial.println(F("[WiFi] ✓ Connected!"));
  Serial.println("  IP Address : " + localIP);
  Serial.println("  MAC Address: " + deviceID);

  dualPrint("  DynoMed Pro", "  WiFi: OK ✓", "  IP: " + localIP, "  Cloud connect...");
  delay(1200);

  // ── STEP 4: Firebase Connect ──────────────────────────────
  Serial.println(F("[Firebase] Connecting..."));

  fbConfig.host = FIREBASE_HOST;
  fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&fbConfig, &fbAuth);
  Firebase.reconnectWiFi(true);

  // Firebase mein device register karo
  bool ok1 = Firebase.setString(fbdo, fbBasePath + "/status",   "waiting");
  bool ok2 = Firebase.setString(fbdo, fbBasePath + "/deviceId", deviceID);
  bool ok3 = Firebase.setString(fbdo, fbBasePath + "/ip",       localIP);

  if (ok1 && ok2 && ok3) {
    fbReady = true;
    Serial.println(F("[Firebase] ✓ Connected & Registered!"));
    Serial.println("  Path: " + fbBasePath);
  } else {
    Serial.println(F("[Firebase] ❌ Error!"));
    Serial.println("  Reason: " + fbdo.errorReason());
    dualPrint("  Firebase ERR!", "  Check:", "  Host & Auth Key", "  Restart device");
    delay(5000);
    // Carry on — retry ho jayega
  }

  // ── READY ─────────────────────────────────────────────────
  Serial.println();
  Serial.println(F("╔══════════════════════════════════╗"));
  Serial.println(F("║         DEVICE IS READY!          ║"));
  Serial.println("║  ID  : " + deviceID + "  ║");
  Serial.println("║  IP  : " + localIP + "          ║");
  Serial.println("║  FB  : " + String(fbReady ? "Connected ✓" : "ERROR ✗") + "         ║");
  Serial.println(F("║  Status : Waiting for website      ║"));
  Serial.println(F("╚══════════════════════════════════╝"));
  Serial.println();

  dualPrint(
    "  DynoMed Ready!",
    "  ID: " + p1 + p2,
    "  Waiting...",
    "  Connect on website"
  );
}

// ════════════════════════════════════════════════════════════════
//
//   LOOP — bar bar chalta hai
//
// ════════════════════════════════════════════════════════════════
void loop() {
  if (!fbReady) return;

  String p1 = deviceID.substring(0, 6);
  String p2 = deviceID.substring(6, 12);

  // Firebase se current status pado
  if (!Firebase.getString(fbdo, fbBasePath + "/status")) {
    Serial.println("[FB ERR] Read failed: " + fbdo.errorReason());
    delay(2000);
    return;
  }

  String status = fbdo.stringData();

  // ════════════════════════════════════════════════════════
  //  STATUS = "waiting"
  //  Koi abhi connected nahi
  // ════════════════════════════════════════════════════════
  if (status == "waiting") {

    // OLED: show ID aur wait message
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 0);  oled.print("  DynoMed Pro");
    oled.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    oled.setCursor(0, 13); oled.print("  ID: " + p1);
    oled.setCursor(0, 23); oled.print("       " + p2);
    oled.setCursor(0, 36); oled.print("  Waiting for");
    oled.setCursor(0, 46); oled.print("  website...");
    oled.fillRect(0, 53, 128, 11, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
    oled.setCursor(2, 54); oled.print("  Enter ID on web");
    oled.display();

    // Serial: sirf ek baar print (jab change ho)
    static String lastPrinted = "";
    if (lastPrinted != "waiting") {
      lastPrinted = "waiting";
      Serial.println(F("[STATUS] → waiting"));
      Serial.println("  Device ID: " + deviceID);
      Serial.println(F("  Koi website connect nahi hua"));
    }

    delay(1000);
  }

  // ════════════════════════════════════════════════════════
  //  STATUS = "connecting"
  //  Website ne ID enter kiya — signal aa raha hai
  // ════════════════════════════════════════════════════════
  else if (status == "connecting") {

    static String lastPrinted = "";
    if (lastPrinted != "connecting") {
      lastPrinted = "connecting";
      Serial.println(F("[STATUS] → connecting"));
      Serial.println(F("  Website se signal aa raha hai..."));
      Serial.println(F("  OLED blink ho raha hai"));
    }

    // OLED: blink animation
    blinkConnecting(p1 + " " + p2);
  }

  // ════════════════════════════════════════════════════════
  //  STATUS = "connected"
  //  Website connected hai — data bhejte raho!
  // ════════════════════════════════════════════════════════
  else if (status == "connected") {

    // Timing — sirf READING_INTERVAL_MS ke baad data bhejo
    unsigned long now = millis();
    if (now - lastReadTime < READING_INTERVAL_MS) {
      delay(50);
      return;
    }
    lastReadTime = now;

    // Force read karo
    float forceKg = 0.0f;
    float forceN  = 0.0f;

    if (scale.is_ready()) {
      forceKg = scale.get_units(5);          // 5 readings ka average
      if (forceKg < 0.0f) forceKg = 0.0f;  // Negative nahi hona chahiye
      forceN  = forceKg * 9.80665f;          // kg → Newton
    } else {
      Serial.println(F("[HX711] Not ready — skipping reading"));
    }

    // ── OLED: Live reading screen ──────────────────────────
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);

    // Header
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.print("DynoMed [LIVE]");
    oled.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    // Force value — bada
    oled.setTextSize(2);
    oled.setCursor(0, 14);
    oled.print(String(forceN, 1) + "N");

    // kg value — chhota
    oled.setTextSize(1);
    oled.setCursor(0, 38);
    oled.print("= " + String(forceKg, 3) + " kg");

    // Footer — CONNECTED bar
    oled.fillRect(0, 53, 128, 11, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
    oled.setCursor(2, 54);
    oled.print(" CONNECTED | Sending...");
    oled.display();

    // ── Firebase: data push karo ───────────────────────────
    FirebaseJson json;
    json.set("force",     forceN);
    json.set("forceKg",   forceKg);
    json.set("timestamp", (long)millis());
    json.set("unit",      "Newton");

    bool sent = Firebase.pushJSON(fbdo, fbBasePath + "/readings", json);

    // ── Serial Monitor: har reading print karo ─────────────
    // forceSerial=true — har baar print ho (live data stream)
    Serial.println(
      "[DATA] " +
      String(forceN, 2) + " N  |  " +
      String(forceKg, 4) + " kg  |  " +
      "Firebase: " + (sent ? "✓ Sent" : "✗ Failed — " + fbdo.errorReason())
    );
  }

  // ════════════════════════════════════════════════════════
  //  STATUS = "disconnected"
  //  Website ne disconnect kiya
  // ════════════════════════════════════════════════════════
  else if (status == "disconnected") {

    Serial.println(F("[STATUS] → disconnected"));
    Serial.println(F("  Website ne disconnect kiya"));
    Serial.println(F("  Resetting to waiting..."));

    dualPrint(
      "  DynoMed Pro",
      "  Disconnected!",
      "  ID: " + p1 + p2,
      "  Reconnect on web"
    );

    delay(1800);

    // Wapas waiting pe jao
    Firebase.setString(fbdo, fbBasePath + "/status", "waiting");
    Serial.println(F("[STATUS] → Reset to: waiting"));
  }

  // ════════════════════════════════════════════════════════
  //  Unknown status
  // ════════════════════════════════════════════════════════
  else {
    Serial.println("[WARNING] Unknown status: '" + status + "' — resetting");
    Firebase.setString(fbdo, fbBasePath + "/status", "waiting");
    delay(1000);
  }
}
