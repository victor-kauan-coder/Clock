/*
 * ==========================================
 * Project: LED Matrix Clock & Telegram Bot
 * Author: Victor Kauan da Silva Miranda
 * GitHub: https://github.com/victor-kauan-coder
 * Role: Software Developer
 * ==========================================
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <time.h>
#include <WiFiManager.h>

// ==========================================
// 1. TELEGRAM CONFIGURATIONS
// ==========================================
#define BOT_TOKEN "Token"
const unsigned long CHECK_INTERVAL = 3000;
unsigned long lastCheckTime;

WiFiClientSecure secureClient;
UniversalTelegramBot bot(BOT_TOKEN, secureClient);

// ==========================================
// 2. LED MATRIX CONFIGURATIONS
// ==========================================
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 15

MD_Parola display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

char displayMessage[150];
char clockText[10];

bool isMessageMode = false;
unsigned long messageStartTime = 0;
unsigned long messageDuration = 30000;

// ==========================================
// 3. STATE CONTROLLED BY WEB DASHBOARD
// ==========================================
ESP8266WebServer server(80);

uint8_t currentBrightness = 3;
uint8_t currentSpeed = 50;
bool isTelegramActive = true;

// ==========================================
// AUXILIARY FUNCTIONS
// ==========================================
void updateClock() {
  time_t now = time(nullptr);
  struct tm* timeInfo = localtime(&now);
  sprintf(clockText, "%02d:%02d", timeInfo->tm_hour, timeInfo->tm_min);
}

void showMessage(const String& text) {
  text.toCharArray(displayMessage, 150);
  isMessageMode = true;
  messageStartTime = millis();

  display.displayClear();
  display.displayText(displayMessage, PA_LEFT, currentSpeed, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
}

void checkMessages(int totalMessages) {
  for (int i = 0; i < totalMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String receivedText = bot.messages[i].text;

    if (!isTelegramActive) {
      continue;
    }

    showMessage(receivedText);
    bot.sendMessage(chat_id, "✅ Message on screen!", "");
  }
}

void enableCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ==========================================
// 4. DASHBOARD ROUTE HANDLERS
// ==========================================

// GET /status
void handleStatus() {
  enableCORS();

  StaticJsonDocument<384> doc;
  doc["online"] = true;
  doc["hora"] = clockText;
  doc["brilho"] = currentBrightness;
  doc["velocidade"] = currentSpeed;
  doc["telegram"] = isTelegramActive;
  doc["mensagemAtiva"] = isMessageMode;
  doc["duracaoMensagem"] = messageDuration / 1000;
  doc["mensagem"] = isMessageMode ? displayMessage : "";

  String response;
  serializeJson(doc, response);

  server.send(200, "application/json", response);
}

// GET /setBrilho?valor=0-15
void handleSetBrightness() {
  enableCORS();

  if (!server.hasArg("valor")) {
    server.send(400, "text/plain", "Missing 'valor' parameter");
    return;
  }

  int value = server.arg("valor").toInt();
  value = constrain(value, 0, 15);

  currentBrightness = value;
  display.setIntensity(currentBrightness);

  server.send(200, "application/json", "{\"ok\":true,\"brilho\":" + String(currentBrightness) + "}");
}

// GET /setVelocidade?valor=10-100
void handleSetSpeed() {
  enableCORS();

  if (!server.hasArg("valor")) {
    server.send(400, "text/plain", "Missing 'valor' parameter");
    return;
  }

  int value = server.arg("valor").toInt();
  value = constrain(value, 10, 100);

  currentSpeed = value;
  
  if (isMessageMode) {
    display.displayClear();
    display.displayText(displayMessage, PA_LEFT, currentSpeed, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  }

  server.send(200, "application/json", "{\"ok\":true,\"velocidade\":" + String(currentSpeed) + "}");
}

// GET /setMensagem?texto=...&duracao=...
void handleSetMessage() {
  enableCORS();

  if (!server.hasArg("texto")) {
    server.send(400, "text/plain", "Missing 'texto' parameter");
    return;
  }

  String text = server.arg("texto");
  if (text.length() == 0) {
    server.send(400, "text/plain", "Empty message");
    return;
  }

  if (server.hasArg("duracao")) {
    long seconds = server.arg("duracao").toInt();
    seconds = constrain(seconds, 3, 300);
    messageDuration = (unsigned long)seconds * 1000UL;
  }

  showMessage(text);

  server.send(200, "application/json", "{\"ok\":true,\"duracaoMensagem\":" + String(messageDuration / 1000) + "}");
}

// GET /toggleTelegram?estado=on|off
void handleToggleTelegram() {
  enableCORS();

  if (!server.hasArg("estado")) {
    server.send(400, "text/plain", "Missing 'estado' parameter");
    return;
  }

  String state = server.arg("estado");
  isTelegramActive = (state == "on");

  server.send(200, "application/json", String("{\"ok\":true,\"telegram\":") + (isTelegramActive ? "true" : "false") + "}");
}

void handleOptions() {
  enableCORS();
  server.send(204);
}

void setupServer() {
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/setBrilho", HTTP_GET, handleSetBrightness);
  server.on("/setVelocidade", HTTP_GET, handleSetSpeed);
  server.on("/setMensagem", HTTP_GET, handleSetMessage);
  server.on("/toggleTelegram", HTTP_GET, handleToggleTelegram);
  server.onNotFound(handleOptions);

  server.begin();
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  display.begin();
  display.setIntensity(currentBrightness);
  display.displayClear();
  display.displayText("Starting...", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);

  WiFiManager wifiManager;

  display.displayText("Wi-Fi AP", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  wifiManager.autoConnect("Clock_Telegram");

  display.displayText("Connected!", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  delay(1000);

  Serial.print("Clock IP: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("clock")) {
    Serial.println("mDNS active -> access via http://clock.local");
  } else {
    Serial.println("Failed to start mDNS");
  }

  char ipText[20];
  WiFi.localIP().toString().toCharArray(ipText, 20);
  display.displayClear();
  display.displayText(ipText, PA_CENTER, 60, 2000, PA_PRINT, PA_NO_EFFECT);
  while (!display.displayAnimate()) {}

  secureClient.setInsecure();

  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  delay(1000);

  updateClock();
  display.displayText(clockText, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);

  setupServer();
}

// ==========================================
// LOOP
// ==========================================
void loop() {
  server.handleClient();
  MDNS.update();

  if (display.displayAnimate()) {
    if (isMessageMode) {
      display.displayReset();
    } else {
      updateClock();
      display.setTextBuffer(clockText);
      display.displayReset();
    }
  }

  if (isMessageMode) {
    if (millis() - messageStartTime > messageDuration) {
      isMessageMode = false;
      display.displayClear();
      updateClock();
      display.displayText(clockText, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    }
  } else {
    if (millis() - lastCheckTime > CHECK_INTERVAL) {
      int totalMessages = bot.getUpdates(bot.last_message_received + 1);

      while (totalMessages) {
        checkMessages(totalMessages);
        totalMessages = bot.getUpdates(bot.last_message_received + 1);
      }

      lastCheckTime = millis();
    }
  }
}