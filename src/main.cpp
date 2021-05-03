/*
  Fabrice Beya
  The complete project details can be found here: 

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include "DHT.h"

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Digital pin connected to the DHT sensor
#define DHTPIN 25 
#define DHTTYPE DHT11 

// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// Device ID
#define DEVICE_UID "1X"

// Your WiFi credentials
#define WIFI_SSID "WIFI_AP"
#define WIFI_PASSWORD "WIFI_PASSWORD"

// Your Firebase Project Web API Key
#define API_KEY "YOUR_API_KEY"

// Your Firebase Realtime database URL
#define DATABASE_URL "https://smart-hommie-default-rtdb.firebaseio.com" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app

// Device Location config
String device_location = "Living Room";

// Firebase Realtime Database Object
FirebaseData fbdo;

// Firebase Authentication Object
FirebaseAuth auth;

// Firebase configuration Object
FirebaseConfig config;

// Firebase database path
String databasePath = "";

// Firebase Unique Identifier
String fuid = "";

// Stores the elapsed time from device start up
unsigned long elapsedMillis = 0;

// The frequency of sensor updates to firebase, set to 10seconds
unsigned long update_interval = 10000;

// Dummy counter to test initial firebase updates
int count = 0;

// Store device authentication status
bool isAuthenticated = false;

// Variables to hold sensor readings
float temperature = 24.7;
float humidity = 60;

// JSON object to hold update values
FirebaseJson temperatue_json;
FirebaseJson humidity_json;

void Wifi_Init() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
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
}

void firebase_init() {
    // configure firebase API Key
    config.api_key = API_KEY;

    // configure firebase realtime database url
    config.database_url = DATABASE_URL;

    // Enable WiFi reconnection 
    Firebase.reconnectWiFi(true);

    Serial.println("------------------------------------");
    Serial.println("Sign up new user...");

    // Sign in to firebase Anonymously
    if (Firebase.signUp(&config, &auth, "", ""))
    {
        Serial.println("Success");
        isAuthenticated = true;

        // Set the databae path where updates will be loaded for this device
        databasePath = "/" + device_location;
        fuid = auth.token.uid.c_str();
    }
    else
    {
        Serial.printf("Failed, %s\n", config.signer.signupError.message.c_str());
        isAuthenticated = false;
    }

    // Assign the callback function for the long running token generation task, see addons/TokenHelper.h
    config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

    // Initialise firebase service
    Firebase.begin(&config, &auth);
}

void setup() {
  // Initialise serial communication for local diagnostics
  Serial.begin(115200);
  // Initialise Connection with location WiFi
  Wifi_Init();
  // Initialise firebase configuration and signup anonymously
  firebase_init();
  dht.begin();

  // Initialise temprature and humidity json data
  temperatue_json.add("deviceuid", DEVICE_UID);
  temperatue_json.add("name", "DHT11-Temp");
  temperatue_json.add("type", "Temperature");
  temperatue_json.add("location", device_location);
  temperatue_json.add("value", temperature);
  String jsonStr;
  temperatue_json.toString(jsonStr, true);
  Serial.println(jsonStr);

  humidity_json.add("deviceuid", DEVICE_UID);
  humidity_json.add("name", "DHT11-Hum");
  humidity_json.add("type", "Humidity");
  humidity_json.add("location", device_location);
  humidity_json.add("value", humidity);
  String jsonStr2;
  humidity_json.toString(jsonStr2, true);
  Serial.println(jsonStr2);
}

void updateSensorReadings(){
  Serial.println("------------------------------------");
  Serial.println("Reading Sensor data ...");

  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  // Check if any reads failed and exit early (to try again).
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  Serial.printf("Temperature reading: %.2f \n", temperature);
  Serial.printf("Humidity reading: %.2f \n", humidity);

  temperatue_json.set("value", temperature);
  humidity_json.set("value", humidity);
}

void uploadSensorData() {
  if (millis() - elapsedMillis > update_interval && isAuthenticated && Firebase.ready())
    {
      elapsedMillis = millis();

      updateSensorReadings();

      String temperature_node = databasePath + "/temperature";  
      String humidity_node = databasePath + "/humidity";  

      if (Firebase.setJSON(fbdo, temperature_node.c_str(), temperatue_json))
      {
          Serial.println("PASSED");
          Serial.println("PATH: " + fbdo.dataPath());
          Serial.println("TYPE: " + fbdo.dataType());
          Serial.println("ETag: " + fbdo.ETag());
          Serial.print("VALUE: ");
          printResult(fbdo); //see addons/RTDBHelper.h
          Serial.println("------------------------------------");
          Serial.println();
      }
      else
      {
          Serial.println("FAILED");
          Serial.println("REASON: " + fbdo.errorReason());
          Serial.println("------------------------------------");
          Serial.println();
      }

      if (Firebase.setJSON(fbdo, humidity_node.c_str(), humidity_json))
      {
          Serial.println("PASSED");
          Serial.println("PATH: " + fbdo.dataPath());
          Serial.println("TYPE: " + fbdo.dataType());
          Serial.println("ETag: " + fbdo.ETag());
          Serial.print("VALUE: ");
          printResult(fbdo); //see addons/RTDBHelper.h
          Serial.println("------------------------------------");
          Serial.println();
      }
      else
      {
          Serial.println("FAILED");
          Serial.println("REASON: " + fbdo.errorReason());
          Serial.println("------------------------------------");
          Serial.println();
      } 
    }
}

void loop() {
  uploadSensorData();
}