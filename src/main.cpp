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
#include "EmonLib.h"
#include <driver/adc.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Digital pin connected to the DHT sensor
#define DHTPIN 4 
#define DHTTYPE DHT11 

// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// Device ID
#define DEVICE_UID "4X"

// Your WiFi credentials
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Your Firebase Project Web API Key
#define API_KEY ""

// Your Firebase Realtime database URL
#define DATABASE_URL "https://smart-hommie-default-rtdb.firebaseio.com" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app

// The GPIO pin were the CT sensor is connected to (should be an ADC input)
#define ADC_INPUT 34

// The voltage in our apartment. Usually this is 230V in South Africa
#define HOME_VOLTAGE 230.0

// Force EmonLib to use 10bit ADC resolution
#define ADC_BITS    10
#define ADC_COUNTS  (1<<ADC_BITS)

EnergyMonitor emon1;

// Array to store 30 readings (and then transmit in one-go to AWS)
short measurements[30];
short measureIndex = 0;
unsigned long lastMeasurement = 0;
unsigned long timeFinishedSetup = 0;

// The frequency of energy measurement, every 1000ms
unsigned long update_energy_interval = 1000;

// Device Location config
String device_location = "Main Bedroom";

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
float watts = 0.0;

// JSON object to hold updated sensor values to be sent to firebase
FirebaseJson temperature_json;
FirebaseJson humidity_json;
FirebaseJson energy_json;

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

        // Set the database path where updates will be loaded for this device
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

void dhtt11_init(){
  dht.begin();

  // Initialise temprature json data
  temperature_json.add("deviceuid", DEVICE_UID);
  temperature_json.add("name", "Temperature");
  temperature_json.add("type", "Temperature");
  temperature_json.add("location", device_location);
  temperature_json.add("value", temperature);

  // Print out initial temperature values
  String jsonStr;
  temperature_json.toString(jsonStr, true);
  Serial.println(jsonStr);

  // Initialise humidity json data
  humidity_json.add("deviceuid", DEVICE_UID);
  humidity_json.add("name", "Humidity");
  humidity_json.add("type", "Humidity");
  humidity_json.add("location", device_location);
  humidity_json.add("value", humidity);

  String jsonStr2;
  humidity_json.toString(jsonStr2, true);
  Serial.println(jsonStr2);

 // Print out initial energy values
  energy_json.add("deviceuid", DEVICE_UID);
  energy_json.add("name", "Energy");
  energy_json.add("type", "Energy");
  energy_json.add("location", device_location);
  energy_json.add("value", watts);

  // Print out initial energy values
  String jsonStr3;
  energy_json.toString(jsonStr3, true);
  Serial.println(jsonStr3);
}

void setup() {
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
  analogReadResolution(10);

   // Initialize emon library (30 = calibration number)
  emon1.current(ADC_INPUT, 30);

  // Initialise serial communication for local diagnostics
  Serial.begin(115200);
  // Initialise Connection with location WiFi
  Wifi_Init();
  // Initialise firebase configuration and signup anonymously
  firebase_init();
  // Initialise DHTT11 module
  dhtt11_init();
}

void updateSensorReadings(){
  Serial.println("------------------------------------");
  Serial.println("Reading Sensor data ...");

  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  Serial.printf("Temperature reading: %.2f \n", temperature);
  Serial.printf("Humidity reading: %.2f \n", humidity);

  temperature_json.set("value", temperature);
  humidity_json.set("value", humidity);
}

void compute_energy(){
  // if (millis() - elapsedMillis > update_energy_interval ){
     elapsedMillis = millis();

    // Calculate Irms only
    double amps = emon1.calcIrms(1480);
    watts = amps * HOME_VOLTAGE;

    energy_json.set("value", watts);

    Serial.println("Energy Measurement - ");
    Serial.print("Amps: ");
    Serial.println(amps);

    Serial.println("Energy Measurement - ");
    Serial.print("Watts: ");
    Serial.println(watts);

  // }
}

void uploadSensorData() {
  if (millis() - elapsedMillis > update_interval && isAuthenticated && Firebase.ready())
    {
      elapsedMillis = millis();

      updateSensorReadings();
      compute_energy();

      String temperature_node = databasePath + "/temperature";  
      String humidity_node = databasePath + "/humidity"; 
      String energy_node = databasePath + "/energy";  

      if (Firebase.setJSON(fbdo, temperature_node.c_str(), temperature_json))
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

      if (Firebase.setJSON(fbdo, energy_node.c_str(), energy_json))
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