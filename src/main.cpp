#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <UMS3.h>
#include <esp_bt.h>
#include <Adafruit_NeoPixel.h>
#include <FS.h>
#include <time.h>
#include <InfluxData.h>
// Define the InfluxData object
// InfluxData influxData;
#define PIN 2      // Input your RGB LED module GPIO pin
#define NUM_LEDS 1 // Number of LEDs in your module
bool usbStatus = true;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, PIN, NEO_GRB + NEO_KHZ800);

int DELAYVAL = 500; // Time (in milliseconds) to pause between pixels
bool writing = false;
UMS3 ums3;

WiFiMulti wifiMulti;

#define DEVICE "eps32-s3"

#define WIFI_SSID "SSID_OF_NETWORK"
#define WIFI_PASSWORD "PASSWORD_OF_NETWORK"

#define WIFI_SSID1 "SSID_OF_NETWORK1"
#define WIFI_PASSWORD1 "PASSWORD_OF_NETWORK1"
#define WIFI_SSID2 "SSID_OF_NETWORK2"
#define WIFI_PASSWORD2 "PASSWORD_OF_NETWORK2"
#define WIFI_SSID3 "SSID_OF_NETWORK3"
#define WIFI_PASSWORD3 "PASSWORD_OF_NETWORK3"

#define INFLUXDB_URL "http://10.20.2.50:5006"
#define INFLUXDB_TOKEN "3n56566UfmFARbfTey0xLliyjQaNmFNIe0gv0qX6Ls7U7EqVocMEWTbihsA=="
#define INFLUXDB_ORG "928c8dd365c43aa5"
#define INFLUXDB_BUCKET "farm"
#define TZ_INFO "AEST-10AEDT,M10.1.0,M4.1.0/3"

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

Point sensorReadings("measurements");

#define ONE_WIRE_BUS 1
#define DATA_FILE "/data.txt"
// Global variables to track time and battery voltage
unsigned long startMillis = 0;
float initialBatteryVoltage = 0.0;
int fileCount = 0;
float temperature = 0.0;
float batteryVoltage = 0.0;
int counter = 0;
float temp[1000] = {};
float volt[1000] = {};
time_t timestamp[1000] = {};
struct DataPoint
{
  String timestampStr; // Store timestamp as string
};
DataPoint dataPoint;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Input a value 0 to 255 to get a color value.
uint32_t Wheel(byte WheelPos)
{
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85)
  {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170)
  {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
void rainbow(uint8_t wait)
{
  uint16_t i, j;

  for (j = 0; j < 256 * 2; j++)
  { // repeat twice for a smoother transition
    for (i = 0; i < strip.numPixels(); i++)
    {
      strip.setPixelColor(i, Wheel((i + j) & 255));
    }
    strip.show();
    delay(wait); // we acnt dekay here any more we will use the general 30 secs delay
    // break;
  }
}

void saveDataToSPIFFS(float temperature, float batteryVoltage)
{
  counter++;
  temp[counter] = temperature;
  volt[counter] = batteryVoltage;
  timestamp[counter] = time(nullptr) + 3600;
  Serial.print("data saved at address :");
  Serial.println(counter);
  time_t timestamps = time(nullptr);
  char timestampStr[50];
  sprintf(timestampStr, "%s", asctime(gmtime(&timestamps)));
  Serial.println(timestampStr);
}

void uploadDataFromSPIFFS()
{
  if (counter > 0)
  {
    writing = true;
    temperature = temp[counter];
    batteryVoltage = volt[counter];

    Serial.print("Writing data from memory :");
    Serial.println(counter);
    Serial.print("temperature:");
    Serial.println(temperature);
    Serial.print("voltage :");
    Serial.println(batteryVoltage);
    delay(500);
    Serial.print("data retrived  at address :");
    Serial.println(counter);
    Serial.print("data retrived  in unix  time :");
    Serial.println(timestamp[counter]);
    counter--;
  }
  else
  {
    Serial.print("no data from memory ");
    writing = false;
  }
}

void initDS18B20()
{
  sensors.begin();
}
int trial = 0;
// Enable messages batching and retry buffer
#define WRITE_PRECISION WritePrecision::S
#define MAX_BATCH_SIZE 10
#define WRITE_BUFFER_SIZE 30
void setup()
{
  Serial.begin(115200);
  ums3.begin();
  sensors.begin();
  WiFi.mode(WIFI_STA);

  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  wifiMulti.addAP(WIFI_SSID1, WIFI_PASSWORD1);
  wifiMulti.addAP(WIFI_SSID2, WIFI_PASSWORD2);
  wifiMulti.addAP(WIFI_SSID3, WIFI_PASSWORD3);
  btStop();

  while (wifiMulti.run() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
    trial++;
    if (trial > 10)
    {
      trial = 0;
      break;
    }
  }
  Serial.println();

  initDS18B20();

  sensorReadings.addTag("device", DEVICE);
  sensorReadings.addTag("location", "farm");

  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  if (client.validateConnection())
  {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  }
  else
  {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  startMillis = millis();                           // Store the current time
  initialBatteryVoltage = ums3.getBatteryVoltage(); // Store initial battery voltage
  strip.begin();                                    // Initialize the NeoPixel library
  strip.show();                                     // Initialize all pixels to 'off'

  client.setWriteOptions(WriteOptions().writePrecision(WRITE_PRECISION).batchSize(MAX_BATCH_SIZE).bufferSize(WRITE_BUFFER_SIZE).useServerTimestamp(false));
}

void loop()
{
  // Check USB presence
  sensorReadings.clearFields();
  int usbPresence = ums3.getVbusPresent();

  if (usbPresence == usbStatus && WiFi.status() != WL_CONNECTED)
  {
    WiFi.mode(WIFI_STA);
    WiFi.reconnect();
    wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
    wifiMulti.addAP(WIFI_SSID1, WIFI_PASSWORD1);
    wifiMulti.addAP(WIFI_SSID2, WIFI_PASSWORD2);
    wifiMulti.addAP(WIFI_SSID3, WIFI_PASSWORD3);
    btStop();
    while (wifiMulti.run() != WL_CONNECTED)
    {
      Serial.print(".");
      delay(500);
      trial++;
      if (trial > 10)
      {
        trial = 0;
        Serial.println("wifi dis-connected");
        break;
      }
    }
  }
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);
  batteryVoltage = ums3.getBatteryVoltage();
  // delay(1000);

  // Upload data from SPIFFS if WiFi connection is restored
  if (client.validateConnection())
  {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  }
  else
  {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  // client.writePoint(sensorReadings);

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Wifi connection lost");
    //  Save data to SPIFFS if USB power is not present
    saveDataToSPIFFS(temperature, batteryVoltage);
  }
  else
  {
    // Upload data from SPIFFS if WiFi connection is restored
    if (client.validateConnection())
    {
      Serial.print("Connected to InfluxDB: ");
      Serial.println(client.getServerUrl());
    }
    else
    {
      Serial.print("InfluxDB connection failed: ");
      Serial.println(client.getLastErrorMessage());
    }
    // Upload data from SPIFFS if WiFi connection is restored
    // uploadDataFromSPIFFS();

    if (writing)
    {

      time_t timestamps = timestamp[counter + 1]; // Obtain the timestamp from your data source
      time_t tnow = time(&timestamps);
      Serial.print("Synchronized time: ");
      Serial.println(ctime(&tnow));
      Serial.print("Original time in unix: ");
      Serial.println(timestamps);
      Serial.print("Original time from memory: ");
      Serial.println(ctime(&timestamps));
      Serial.print("Original time in unix from memory: ");
      Serial.println(timestamp[counter + 1]);

      // Set the precision for sensorReadings
      sensorReadings.setTime(WritePrecision::S); // Set precision to seconds
                                                 // Set the timestamp for sensorReadings
      sensorReadings.setTime(timestamps);        // Set timestamp without specifying precision

      // Print synchronized time after setting the time for sensorReadings
      // time(sensorReadings.getTime()); // Get the synchronized time
      Serial.print("Synchronized time: ");
      Serial.println((sensorReadings.getTime()));
      Serial.print("Synchronized time2 : ");
      Serial.println(ctime(&timestamps));
      char timestampStr[50];
      sprintf(timestampStr, "%s", asctime(gmtime(&timestamps)));
      strftime(timestampStr, sizeof(timestampStr), "%Y-%m-%dT%H:%M:%SZ", gmtime(&timestamps));
      // sensorReadings.setTime(timestamp, WritePrecision::S);
      sensorReadings.addField("temperature", temperature);
      sensorReadings.addField("battery_voltage", batteryVoltage);
      sensorReadings.addField("usb_presence", usbPresence);
      Serial.print("Writing old data: ");

      sensorReadings.setTime(tnow);
      client.setWriteOptions(WriteOptions().writePrecision(WRITE_PRECISION).batchSize(MAX_BATCH_SIZE).bufferSize(WRITE_BUFFER_SIZE).useServerTimestamp(false));
      Serial.println(client.pointToLineProtocol(sensorReadings));
      client.writePoint(sensorReadings);
    }
    else
    {
      time_t timestamps = time(nullptr);
      char timestampStr[50];
      sprintf(timestampStr, "%s", asctime(gmtime(&timestamps)));
      sensorReadings.addField("temperature", temperature);
      sensorReadings.addField("battery_voltage", batteryVoltage);
      sensorReadings.addField("usb_presence", usbPresence);
      sensorReadings.setTime(time(&timestamps));
      Serial.print("Writing: ");
      Serial.println(client.pointToLineProtocol(sensorReadings));
      // Resync time with NTP server
      configTime(0, 0, "pool.ntp.org", "time.nis.gov");
      timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
      Serial.println(client.writePoint(sensorReadings));
      client.writePoint(sensorReadings);
    }
  }
  // Read battery voltage
  float currentBatteryVoltage = ums3.getBatteryVoltage();

  // Check if battery voltage is below 3.60V or 4 hours have elapsed (whichever comes first)
  if ((currentBatteryVoltage < 3.60) || (millis() - startMillis >= (4 * 60 * 60 * 1000) && !ums3.getVbusPresent()))
  {
    // Shut down the ESP32
    // esp_deep_sleep_start();
    Serial.println("esp 32 will turn off now");
  }
  // Send data to InfluxDB if USB power is present
  if (ums3.getVbusPresent() == usbStatus)
  {

    if (fileCount == 0)
    {
      rainbow(60 * 1000 / 256); // 60 seconds divided by 256-color spectrum
      strip.clear();
      strip.show();
    }
    fileCount++;
    Serial.print("fileCount : ");
    Serial.println(fileCount);
    if (fileCount >= 120)
      fileCount = 0; // do thid every 60 seconds
  }
  else
  {
    fileCount = 0;
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    Serial.println("wifi disconnected successfully");
    // wifiMulti.disconnect();
    //  Save data to SPIFFS if USB power is not present
    // saveDataToSPIFFS(temperature, batteryVoltage);
  }

  if (writing)
  {
    Serial.println("Wait 5s writing data");
    delay(5000);
  }
  else
  {
    Serial.println("Wait 30s");
    delay(30000);
  }
}