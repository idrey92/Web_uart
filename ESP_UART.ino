#include <WiFi.h>
#include <WebServer.h>
#include <esp_system.h>

HardwareSerial &uart = Serial1;

int currentBaud = 9600;
int rxPin = 7;
int txPin = 6;
int bufferLimit = 3000;  // Максимальный размер буфера
String uartBuffer = "";

const int baudRates[] = {9600, 19200, 38400, 57600, 115200};
const int allowedPins[] = {3, 4, 5, 6, 7, 8, 9, 10};

const char* ssid = "ESP32-UART";
const char* password = "12345678";

WebServer server(80);

bool isValidPin(int pin) {
  for (int i = 0; i < sizeof(allowedPins) / sizeof(allowedPins[0]); i++) {
    if (allowedPins[i] == pin) return true;
  }
  return false;
}

String getPage() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>UART Console</title>";
  html += "<style>body{font-family:monospace;background:#111;color:#0f0;}textarea,input,select,button{font-size:16px;}#console{width:100%;height:300px;background:#000;color:#0f0;padding:10px;overflow-y:auto;white-space:pre-wrap;border:1px solid #0f0;margin-bottom:1em;}</style></head><body>";
  
  html += "<h2>UART Console</h2>";
  html += "<div id='console'></div>";

  html += "<h3>Send Data</h3>";
  html += "<form onsubmit='sendData();return false;'><input id='out' type='text' style='width:70%;'><button type='submit'>Send</button></form>";

  html += "<h3>Settings</h3>";
  html += "<form action='/setParams' method='get'>";
  html += "Baud: <select name='baud'>";
  for (int rate : baudRates) {
    html += "<option value='" + String(rate) + "'";
    if (rate == currentBaud) html += " selected";
    html += ">" + String(rate) + "</option>";
  }
  html += "</select><br>";

  html += "RX Pin: <select name='rx'>";
  for (int pin : allowedPins) {
    html += "<option value='" + String(pin) + "'";
    if (pin == rxPin) html += " selected";
    html += ">" + String(pin) + "</option>";
  }
  html += "</select><br>";

  html += "TX Pin: <select name='tx'>";
  for (int pin : allowedPins) {
    html += "<option value='" + String(pin) + "'";
    if (pin == txPin) html += " selected";
    html += ">" + String(pin) + "</option>";
  }
  html += "</select><br>";

  html += "Buffer Size: <input name='bufsize' type='number' value='" + String(bufferLimit) + "' min='500' max='10000'><br><br>";

  html += "<button type='submit'>Apply</button>";
  html += "</form><br>";

  html += "<form action='/restart' method='get'><button type='submit'>Restart ESP</button></form>";

  html += "<p><b>Current Config:</b> Baud: " + String(currentBaud) +
          ", RX: " + String(rxPin) + ", TX: " + String(txPin) +
          ", Buffer: " + String(bufferLimit) + " bytes</p>";

  html += "<script>";
  html += "function updateConsole(){fetch('/uart').then(r=>r.text()).then(t=>{let c=document.getElementById('console');c.innerText=t;c.scrollTop=c.scrollHeight;});}";
  html += "setInterval(updateConsole, 1000);";
  html += "function sendData(){let d=document.getElementById('out').value;fetch('/send?data='+encodeURIComponent(d));document.getElementById('out').value='';}";
  html += "</script></body></html>";
  return html;
}

void handleRoot() {
  server.send(200, "text/html; charset=UTF-8", getPage());
}

void handleUart() {
  server.send(200, "text/plain; charset=UTF-8", uartBuffer);
}

void handleSend() {
  if (server.hasArg("data")) {
    String data = server.arg("data");
    uart.print(data);
    uart.print("\r\n");
  }
  server.send(200, "text/plain", "OK");
}

void handleSetParams() {
  if (server.hasArg("baud")) currentBaud = server.arg("baud").toInt();
  if (server.hasArg("rx")) {
    int rx = server.arg("rx").toInt();
    if (isValidPin(rx)) rxPin = rx;
  }
  if (server.hasArg("tx")) {
    int tx = server.arg("tx").toInt();
    if (isValidPin(tx)) txPin = tx;
  }
  if (server.hasArg("bufsize")) {
    int size = server.arg("bufsize").toInt();
    if (size >= 500 && size <= 10000) bufferLimit = size;
  }

  uart.end();
  uart.begin(currentBaud, SERIAL_8N1, rxPin, txPin);

  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleRestart() {
  server.send(200, "text/plain", "Restarting...");
  delay(500);
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  uart.begin(currentBaud, SERIAL_8N1, rxPin, txPin);

  WiFi.softAP(ssid, password);
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/uart", handleUart);
  server.on("/send", handleSend);
  server.on("/setParams", handleSetParams);
  server.on("/restart", handleRestart);
  server.begin();
}

void loop() {
  server.handleClient();

  while (uart.available()) {
    char c = uart.read();
    // Поддержка UTF-8 и кириллицы невозможна напрямую — но хотя бы не ломаем байты
    uartBuffer += c;
    if (uartBuffer.length() > bufferLimit) {
      uartBuffer = uartBuffer.substring(uartBuffer.length() - bufferLimit);
    }
  }
}
