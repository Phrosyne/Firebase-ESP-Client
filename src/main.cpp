#include <WiFi.h>
#include <esp_wpa2.h>
#include <esp_sleep.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <hush.h>
#include <HX711.h>

#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP 5

String pass = WIFI_PASS;

const String FIREBASE_API_KEY = FIREBASE_API;
const String FIREBASE_PROJECT_ID = PROJECT_ID;
const String COLLECTION_NAME = "sensor_readings";

HX711 scale;
const int LOADCELL_DOUT_PIN = 5;
const int LOADCELL_SCK_PIN = 4;

void sendDataToFirestore(double);

void setup()
{
  Serial.begin(115200);

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(450);
  scale.tare();

  WiFi.disconnect(true); // Disconnect from any previous networks
  WiFi.mode(WIFI_STA);

  // Configure WPA2 Enterprise Enterprise Identity and Password
  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)WIFI_ID, strlen(WIFI_ID));
  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)WIFI_ID, strlen(WIFI_ID));
  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)WIFI_PASS, strlen(WIFI_PASS));

  // Enable WPA2 Enterprise
  esp_wifi_sta_wpa2_ent_enable();

  WiFi.begin(WIFI_SSID);

  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Serial.println("Ready:");
}

void loop()
{
  if (scale.is_ready()) {
    double s1 = scale.get_units(1);
    double s2 = scale.get_units(1);
    double s3 = scale.get_units(1);

    double sensorValue;
    if ((s2 <= s1 && s1 <= s3) || (s3 <= s1 && s1 <= s2)) {
      sensorValue = s1;
    } else if ((s1 <= s2 && s2 <= s3) || (s3 <= s2 && s2 <= s1)) {
      sensorValue = s2;
    } else {
      sensorValue = s3;
    }

    sendDataToFirestore(sensorValue);
    delay(5000);
  }
}

void sendDataToFirestore(double food_level)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Error: Not connected to Wi-Fi.");
    return;
  }

  // Assemble the Firestore REST API URL.
  // By POSTing to the collection name, Firestore creates a random document ID.
  String url = "";
  url += "https://firestore.googleapis.com/v1/projects/";
  url += FIREBASE_PROJECT_ID;
  url += "/databases/(default)/documents/";
  url += COLLECTION_NAME;
  url += "/Sensor1?key=";
  url += FIREBASE_API_KEY;

  // Create the request payload (body) in the JSON format required by Firestore.
  // This structure is the most critical part and a common source of errors.
  StaticJsonDocument<256> jsonDoc;

  JsonObject fields = jsonDoc.createNestedObject("fields");

  JsonObject sensorIdField = fields.createNestedObject("food_level");
  sensorIdField["doubleValue"] = food_level;

  String jsonPayload;
  serializeJson(jsonDoc, jsonPayload);

  // Start the HTTP request
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  Serial.println("--- STARTING HTTP REQUEST ---");
  Serial.print("URL: ");
  Serial.println(url);
  Serial.print("Payload: ");
  Serial.println(jsonPayload);

  // Send the POST request with the JSON payload
  int httpResponseCode = http.PATCH(jsonPayload);

  // Check the server's response
  if (httpResponseCode > 0)
  {
    String responsePayload = http.getString();
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    Serial.print("Response: ");
    Serial.println(responsePayload);
  }
  else
  {
    Serial.print("Error on POST request. Error code: ");
    Serial.println(httpResponseCode);
  }

  Serial.println("--- END OF HTTP REQUEST --- \n");

  // Free resources
  http.end();
}