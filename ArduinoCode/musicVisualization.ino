#include "arduinoFFT.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>

#define SAMPLES 128             // Must be a power of 2
#define SAMPLING_FREQUENCY 5000 // Hz, must be less than 10000 due to ADC

const int sampleWindow = 50; // Sample window width in mS (50 mS = 20Hz)
unsigned int sample;
const int numReadings = 10;

int readings[numReadings]; // the readings from the analog input
int average = 0;           // the average

int iterations = 0;
bool recording = false;

arduinoFFT FFT = arduinoFFT();
unsigned int sampling_period_us;
unsigned long microseconds;

double vReal[SAMPLES];
double vImag[SAMPLES];
double vHZ[20];
double vVolts[20];
double vPeaks[20];
int iterationStep = 0;

// Change this!!
const char *ssid = "__CHANGE_ME__";
const char *password = "__CHANGE_ME__";

//Your Domain name with URL path or IP address with path
const char *serverName = "http://{YOUR_SERVER_IP}/update-sensor";

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelay = 600000;
// Set timer to 5 seconds (5000)
unsigned long timerDelay = 5000;

ESP8266WebServer server(80);

void sampleRecording(int iteration);

int red = 4; // D2
int green = 13; // D7
int blue = 5; // D1
int rgbPower = 14; // D5

// connect to wifi – returns true if successful or false if not
void wifiSetup()
{
  // Connect
  Serial.printf("[WIFI] Connecting to %s ", ssid);
  WiFi.begin(ssid, password);

  // Wait
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // Connected!
  Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
}

void setup()
{
  Serial.begin(9600);

  pinMode(red, OUTPUT);
  pinMode(blue, OUTPUT);
  pinMode(green, OUTPUT);
  pinMode(rgbPower, OUTPUT);

  // Setup time
  digitalWrite(red, LOW);
  digitalWrite(green, LOW);
  digitalWrite(blue, HIGH);
  analogWrite(rgbPower, 25);


  randomSeed(98155);
  for (int thisReading = 0; thisReading < numReadings; thisReading++)
  {
    readings[thisReading] = 0;
  }

  sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQUENCY));
  delay(2000);

  // Initialise wifi connection
  wifiSetup();

  server.on("/start", []()
  {
    iterations = 0;
    recording = true;
    Serial.println("Recording starting ...");
    server.send(200, "text/html", "");
  });

  server.on("/stop", []()
  {
    // Reset the data so you get a new song on start
    memset(vHZ, 0, sizeof(vHZ));
    memset(vVolts, 0, sizeof(vVolts));
    memset(vPeaks, 0, sizeof(vPeaks));

    recording = false;
    Serial.println("Recording stopping ...");
    server.send(200, "text/html", "");
  });
  server.begin();

  digitalWrite(red, LOW);
  digitalWrite(green, LOW);
  digitalWrite(blue, LOW);
}

void loop()
{
  server.handleClient();

  if (recording)
  {
    digitalWrite(green, LOW);
    digitalWrite(red, HIGH);
    digitalWrite(blue, HIGH);

    for (iterationStep = 0; iterationStep < 20; iterationStep++) {
      sampleRecording(iterationStep);
      delay(10);
    }

    iterationStep = 0;

    WiFiClient client;
    HTTPClient http;

    if (http.begin(client, serverName))
    {
      // Specify content-type header
      //http.addHeader("Content-Type", "application/json");

      String readingsOutput;
      for (int i = 0; i < 20; i++)
      {
        readingsOutput = readingsOutput + (String)"[" + String(vHZ[i]) + "," + vPeaks[i] + "," + vVolts[i] + (String)"]";
        if (i != 19) {
          readingsOutput = readingsOutput + (String)",";
        }
      }

      // Send HTTP POST request
      //int httpResponseCode = http.POST(voltsOut);

      // If you need an HTTP request with a content type: application/json, use the following:
      http.addHeader("Content-Type", "application/json");
      int httpResponseCode = http.POST("{\"readings\":[" + readingsOutput + "]}");

      // If you need an HTTP request with a content type: text/plain
      //http.addHeader("Content-Type", "text/plain");
      //int httpResponseCode = http.POST("Hello, World!");

      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);

      // Free resources
      http.end();

      memset(vHZ, 0, sizeof(vHZ));
      memset(vVolts, 0, sizeof(vVolts));
      memset(vPeaks, 0, sizeof(vPeaks));

      // Reset the counter variable
      iterations = 0;
    }
  } else {
    digitalWrite(green, HIGH);
    digitalWrite(red, LOW);
    digitalWrite(blue, HIGH);
  }
}

void sampleRecording(int iterationNum)
{

  unsigned int signalMax = 0;
  unsigned int signalMin = 1024;

  for (int i = 0; i < SAMPLES; i++)
  {
    microseconds = micros(); //Overflows after around 70 minutes!

    sample = analogRead(0);
    if (sample < 1024) // toss out spurious readings
    {
      if (sample > signalMax)
      {
        signalMax = sample; // save just the max levels
      }
      else if (sample < signalMin)
      {
        signalMin = sample; // save just the min levels
      }
    }

    vReal[i] = sample;
    vImag[i] = 0;

    while (micros() < (microseconds + sampling_period_us))
    {
    }
  }
  FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(vReal, vImag, SAMPLES, FFT_FORWARD);
  FFT.ComplexToMagnitude(vReal, vImag, SAMPLES);
  double x;
  double hertz;
  FFT.MajorPeak(vReal, SAMPLES, SAMPLING_FREQUENCY, &x, &hertz);

  double peakToPeak = signalMax - signalMin;       // max - min = peak-peak amplitude
  double volts = (peakToPeak * 5) / 1024; // convert to volts

  vHZ[iterationNum] = hertz;
  vVolts[iterationNum] = volts;
  vPeaks[iterationNum] = peakToPeak;
}