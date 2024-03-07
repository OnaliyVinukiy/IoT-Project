#include <MFRC522.h>
#include <MFRC522Extended.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TimeLib.h>
#include <ArduinoJson.h>

// Define pins
#define RST_PIN         22
#define SS_PIN          5
#define inputPin        2
#define Sensor_Pin      2
#define lightPin 4
#define lockpin 33
#define soundpin 14

// Wi-Fi credentials
const char* ssid = "OPPOF17";
const char* password = "Onaliy12334";

// Initialize RFID reader
MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600);
  while (!Serial);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");

  // Initialize SPI and RFID module
  SPI.begin();
  mfrc522.PCD_Init();
  delay(4);
  mfrc522.PCD_DumpVersionToSerial();
  Serial.println("Hold your Student ID Card near to the Scanner..");
  Serial.println();

  // Initialize pins
  pinMode(inputPin, INPUT);
  pinMode(Sensor_Pin, INPUT);
  pinMode(lightPin, OUTPUT);
  pinMode(lockpin, OUTPUT);
  pinMode(soundpin, OUTPUT);

  // Configure time using NTP
  configTime(5 * 3600 + 1800, 0, "pool.ntp.org");

  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("Time successfully configured using NTP.");
}

void loop() {
  // Motion sensor
  bool motion = digitalRead(inputPin);
  if (motion) {
     Serial.println("Motion detected: " + String(motion));
    digitalWrite(lightPin, LOW);
    digitalWrite(lockpin, LOW);
  } else {
    Serial.println("Motion not detected: " + String(motion));
    digitalWrite(lightPin, HIGH);
    digitalWrite(lockpin, HIGH);
  }

  // Sound sensor
  bool soundDetected = digitalRead(Sensor_Pin);
  int Senor_Value = analogRead(A0);
  if (soundDetected) {
    Serial.println("Sound detected \n");
  } else {
    Serial.println("Sound not detected \n");
  }

  delay(3000);

  // Check for RFID card
  if (!mfrc522.PICC_IsNewCardPresent()) {
    Serial.println("No card present");
    return;
  }

  // Read card UID
  if (!mfrc522.PICC_ReadCardSerial()) {
    Serial.println("Failed to read card");
    return;
  }

  // Dump card UID
  mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
  String cardID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    cardID += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    cardID += String(mfrc522.uid.uidByte[i], HEX);
  }
  cardID.toUpperCase();
  Serial.println("Card ID: " + cardID);

  // Get current date and time
  String currentDate = getCurrentDate();
  String currentTime = getTimeFromArduino();
  Serial.println("Current Date: " + currentDate);
  Serial.println("Current Time: " + currentTime);

  // Check booking status
  checkBookingStatus(cardID, currentDate, currentTime);

  delay(1500);
}

void checkBookingStatus(String cardID, String currentDate, String currentTime) {
  Serial.println("Checking booking status...");

  // Make HTTP GET request to Firebase to check booking status
  String url = "https://smart-study-room-default-rtdb.firebaseio.com/.json";
  HTTPClient http;
  if (http.begin(url)) {
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        JsonObject bookings = doc["bookings"];
        for (JsonPair entry : bookings) {
          String bookingStudentId = entry.value()["studentId"];
          String bookingDate = entry.value()["date"];
          String intime = entry.value()["intime"];
          String outtime = entry.value()["outtime"];

          int currentHour = currentTime.substring(0, 2).toInt();
          int currentMinute = currentTime.substring(3, 5).toInt();

          int inHour = intime.substring(0, 2).toInt();
          int inMinute = intime.substring(3, 5).toInt();

          int outHour = outtime.substring(0, 2).toInt();
          int outMinute = outtime.substring(3, 5).toInt();

          if (bookingStudentId == cardID && bookingDate == currentDate &&
              (currentHour > inHour || (currentHour == inHour && currentMinute >= inMinute)) &&
              (currentHour < outHour || (currentHour == outHour && currentMinute <= outMinute))) {
            Serial.println("Access Granted");
            return;
          }
        }
        Serial.println("Access Denied");
      } else {
        Serial.println("Error parsing JSON response");
      }
    } else {
      Serial.println("Error getting booking status from Firebase");
    }
    http.end();
  } else {
    Serial.println("Failed to connect to Firebase");
  }
}

String getCurrentDate() {
  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);
  char formattedDate[11];
  strftime(formattedDate, sizeof(formattedDate), "%Y-%m-%d", timeinfo);
  return String(formattedDate);
}

String getTimeFromArduino() {
  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);
  char formattedTime[9];
  strftime(formattedTime, sizeof(formattedTime), "%H:%M:%S", timeinfo);
  return String(formattedTime);
}
