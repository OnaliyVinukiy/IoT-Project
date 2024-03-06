#include <MFRC522.h>
#include <MFRC522Extended.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TimeLib.h> // Include the Time library
#include <ArduinoJson.h>

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

MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600);
  while (!Serial); // Wait for serial port to connect. Needed for native USB port only

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

  // Initialize input pins
  pinMode(inputPin, INPUT);
  pinMode(Sensor_Pin, INPUT);
  pinMode(lightPin, OUTPUT);
  Serial.println("\n\nLet's Begin\n");

  // Initialize lock and sound pins
  pinMode(lockpin, OUTPUT);
  pinMode(soundpin, OUTPUT);

  // Configure time using NTP
  Serial.println("Configuring time using NTP...");
  configTime(5 * 3600 + 1800, 0, "pool.ntp.org"); // Set time via NTP for Indian Standard Time (UTC+5:30)

  time_t now = time(nullptr);
  Serial.print("Waiting for time sync");
  while (now < 8 * 3600) { // wait for time to be set (8 hours is an arbitrary value)
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println();

  if (now == 0) {
    Serial.println("Failed to configure time using NTP!");
  } else {
    Serial.println("Time successfully configured using NTP.");
    Serial.println("Current time: " + String(ctime(&now)));
  }
}


void loop() {
  bool accessGranted = false; // Initialize accessGranted variable

  // Motion sensor
  bool motion = digitalRead(inputPin);
  if (motion) {
    Serial.println("Motion detected: " + String(motion));
    digitalWrite(lightPin, LOW);
    digitalWrite(lockpin, LOW);
    //digitalWrite(soundpin,LOW);
  } else {
    Serial.println("Motion not detected: " + String(motion));
    digitalWrite(lightPin, HIGH);
    digitalWrite(lockpin, HIGH);
    // digitalWrite(soundpin,HIGH);
  }

  bool Sensor_State = digitalRead(Sensor_Pin);
  int Senor_Value = analogRead(A0);
  Serial.println("\nSensor_State: " + String(Sensor_State));

  if (Sensor_State == true) {
    Serial.println("Sound Detected");
  } else {
    Serial.println("Sound Not Detected");
  }

  delay(3000);

  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!mfrc522.PICC_IsNewCardPresent()) {
    Serial.println("No card present");
    return;
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {
    Serial.println("Failed to read card");
    return;
  }

  // Dump debug info about the card; PICC_HaltA() is automatically called
  mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
  Serial.print("Card ID: ");
  String cardID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    cardID.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : ""));
    cardID.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  cardID.toUpperCase();
  Serial.println(cardID);

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
  bool accessGranted = false;
  Serial.println("Checking booking status...");

  // Make HTTP GET request to Firebase to check booking status
  String url = "https://smart-study-room-default-rtdb.firebaseio.com/.json";
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    Serial.println("HTTP GET request successful");

    String payload = http.getString();
    Serial.println("Response payload: " + payload);

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      Serial.println("JSON parsing successful");

      JsonObject bookings = doc["bookings"];

      for (JsonPair entry : bookings) {
        String bookingStudentId = entry.value()["studentId"];
        String bookingDate = entry.value()["date"];
        String intime = entry.value()["intime"];
        String outtime = entry.value()["outtime"];

        Serial.println("Booking Student ID: " + bookingStudentId);
        Serial.println("Booking Date: " + bookingDate);
        Serial.println("Booking In Time: " + intime);
        Serial.println("Booking Out Time: " + outtime);

        // Splitting time into hours and minutes
        int currentHour = currentTime.substring(0, 2).toInt();
        int currentMinute = currentTime.substring(3, 5).toInt();

        int inHour = intime.substring(0, 2).toInt();
        int inMinute = intime.substring(3, 5).toInt();

        int outHour = outtime.substring(0, 2).toInt();
        int outMinute = outtime.substring(3, 5).toInt();

        Serial.println("Current Time (Hour): " + String(currentHour));
        Serial.println("Current Time (Minute): " + String(currentMinute));
        Serial.println("In Time (Hour): " + String(inHour));
        Serial.println("In Time (Minute): " + String(inMinute));
        Serial.println("Out Time (Hour): " + String(outHour));
        Serial.println("Out Time (Minute): " + String(outMinute));

        // Check if current time is within the booking time range
        if (bookingStudentId == cardID && bookingDate == currentDate) {
    if (currentHour > inHour || (currentHour == inHour && currentMinute >= inMinute)) {
        if (currentHour < outHour || (currentHour == outHour && currentMinute <= outMinute)) {
            Serial.println("Access Granted");
            accessGranted = true;
            break;
        }
    }
}

}
      }

      if (!accessGranted) {
        Serial.println("Access Denied");
        Serial.println("Current Time: " + currentTime);
Serial.println("In Time: " + intime);
Serial.println("Out Time: " + outtime);

      }
    } else {
      Serial.println("Error parsing JSON response");
    }
  } else {
    Serial.println("Error getting booking status from Firebase");
  }

  http.end();
}

String getCurrentDate() {
  // Get current date
  time_t now = time(nullptr); // Get the current time
  struct tm *timeinfo = localtime(&now); // Convert the current time to the local time

  // Extract year, month, and day
  int currentYear = timeinfo->tm_year + 1900; // tm_year represents years since 1900
  int currentMonth = timeinfo->tm_mon + 1; // tm_mon is 0-indexed
  int currentDay = timeinfo->tm_mday;

  String currentDate = String(currentYear) + "-" + String(currentMonth) + "-" + String(currentDay);
  return currentDate;
}

String getTimeFromArduino() {
  // Get current time
  time_t now = time(nullptr); // Get the current time
  struct tm *timeinfo = localtime(&now); // Convert the current time to the local time

  // Extract hour, minute, and second
  int currentHour = timeinfo->tm_hour;
  int currentMinute = timeinfo->tm_min;
  int currentSecond = timeinfo->tm_sec;

  String currentTime = String(currentHour) + ":" + String(currentMinute) + ":" + String(currentSecond);
  return currentTime;
}

