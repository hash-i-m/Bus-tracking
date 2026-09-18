#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include "mbedtls/md.h"

TinyGPSPlus gps;
HardwareSerial gpsSerial(2);   // UART2 for GPS
HardwareSerial sim900(1);      // UART1 for SIM900A

#define GPS_RX 16
#define GPS_TX 17
#define SIM_RX 4
#define SIM_TX 2

const char apn[] = "www";

// ===================== DEVICE IDENTITY (esp32-01 / bus1) =====================
const char* BUS_ID    = "bus1";
const char* DEVICE_ID = "esp32-01";
const char* SECRET    = "e3d2b3bb9875121546cd63cf5d3863a8";
// ================================================================================

const char* RELAY_HOST = "recverse.ibinujaleel.dev";
const int   RELAY_PORT = 8081;
const char* RELAY_PATH = "/p";

unsigned long lastSend = 0;
const unsigned long sendInterval = 500; // spec requires 0.5 seconds

uint32_t getCounter() {
  if (gps.time.isValid() && gps.date.isValid()) {
    struct tm t;
    t.tm_year = gps.date.year() - 1900;
    t.tm_mon  = gps.date.month() - 1;
    t.tm_mday = gps.date.day();
    t.tm_hour = gps.time.hour();
    t.tm_min  = gps.time.minute();
    t.tm_sec  = gps.time.second();
    time_t epoch = mktime(&t);
    return (uint32_t)epoch;
  }
  return millis() / 1000;
}

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  sim900.begin(9600, SERIAL_8N1, SIM_RX, SIM_TX);

  Serial.println("Initializing SIM900A...");
  delay(3000);

  sendAT("AT", 1000);
  sendAT("AT+CREG?", 1000);

  // Use the classic GPRS attach flow for TCP mode (not SAPBR -- that's for the HTTP client, which we're bypassing)
  sendAT("AT+CGATT=1", 2000);
  String cstt = String("AT+CSTT=\"") + apn + "\",\"\",\"\"";
  sendAT(cstt.c_str(), 2000);
  sendAT("AT+CIICR", 3000);
  sendAT("AT+CIFSR", 2000);

  Serial.println("Setup done. Waiting for GPS fix...");
}

void loop() {
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  if (millis() - lastSend > sendInterval) {
    lastSend = millis();

    if (gps.location.isValid() && gps.time.isValid() && gps.date.isValid()) {
      float lat = gps.location.lat();
      float lng = gps.location.lng();
      float spd_ms = gps.speed.isValid() ? gps.speed.knots() * 0.514444 : 0.0;
      uint32_t counter = getCounter();

      Serial.print("Fix: ");
      Serial.print(lat, 6);
      Serial.print(", ");
      Serial.print(lng, 6);
      Serial.print(" | speed(m/s): ");
      Serial.print(spd_ms, 2);
      Serial.print(" | counter: ");
      Serial.println(counter);

      sendPositionRawTCP(lat, lng, spd_ms, counter);
    } else {
      Serial.println("Waiting for GPS location + time/date fix -- skipping send.");
    }
  }
}

void sendAT(const char* cmd, int waitMs) {
  sim900.println(cmd);
  Serial.print(">> ");
  Serial.println(cmd);
  long t = millis();
  while (millis() - t < waitMs) {
    while (sim900.available()) {
      Serial.write(sim900.read());
    }
  }
}

// Same as before -- returns lowercase hex HMAC-SHA256
String hmacSha256Hex(const char* key, const String& message) {
  byte hmacResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)key, strlen(key));
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)message.c_str(), message.length());
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);

  String hex = "";
  for (int i = 0; i < 32; i++) {
    char buf[3];
    sprintf(buf, "%02x", hmacResult[i]);
    hex += buf;
  }
  return hex;
}

// Builds the JSON body, signs it, opens a raw TCP socket, and writes the full HTTP request by hand
void sendPositionRawTCP(float lat, float lng, float spd, uint32_t counter) {
  String body = "{\"b\":\"" + String(BUS_ID) +
                "\",\"d\":\"" + String(DEVICE_ID) +
                "\",\"c\":" + String(counter) +
                ",\"lat\":" + String(lat, 6) +
                ",\"lng\":" + String(lng, 6) +
                ",\"spd\":" + String(spd, 2) +
                ",\"hdg\":null}";

  String signature = hmacSha256Hex(SECRET, body);

  Serial.print("Body: ");
  Serial.println(body);
  Serial.print("Sig: ");
  Serial.println(signature);

  // Build the raw HTTP/1.1 request text by hand
  String request = "POST " + String(RELAY_PATH) + " HTTP/1.1\r\n";
  request += "Host: " + String(RELAY_HOST) + "\r\n";
  request += "X-Sig: " + signature + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(body.length()) + "\r\n";
  request += "Connection: close\r\n";  // close after response -- simplest to parse; can optimize to keep-alive later
  request += "\r\n";
  request += body;

  // Open the TCP connection
  String cipstart = "AT+CIPSTART=\"TCP\",\"" + String(RELAY_HOST) + "\"," + String(RELAY_PORT);
  sendAT(cipstart.c_str(), 4000);   // wait for "CONNECT OK"

  // Tell the module how many bytes we're about to send
  String cipsend = "AT+CIPSEND=" + String(request.length());
  sim900.println(cipsend);
  Serial.print(">> ");
  Serial.println(cipsend);
  delay(500); // wait for ">" prompt

  // Send the exact raw request bytes
  sim900.print(request);

  // Read the response (headers + body) for a few seconds
  long t = millis();
  while (millis() - t < 5000) {
    while (sim900.available()) {
      Serial.write(sim900.read());
    }
  }

  sendAT("AT+CIPCLOSE", 1000);
}