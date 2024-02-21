#include <SPIFFS.h>
#define DATA_FILE "/data.txt"
void saveDataToSPIFFS(float temperature, float batteryVoltage)
{
    File file = SPIFFS.open(DATA_FILE, FILE_APPEND);
    if (!file)
    {
        Serial.println("Failed to open file for writing");
        return;
    }

    file.write((uint8_t *)&temperature, sizeof(temperature));
    file.write((uint8_t *)&batteryVoltage, sizeof(batteryVoltage));

    file.close();
}

void uploadDataFromSPIFFS()
{
    File file = SPIFFS.open(DATA_FILE, FILE_READ);
    if (!file)
    {
        Serial.println("No data to upload from SPIFFS");
        return;
    }

    while (file.available())
    {
        float temperature;
        float batteryVoltage;

        size_t bytesRead = file.read((uint8_t *)&temperature, sizeof(temperature));
        if (bytesRead != sizeof(temperature))
        {
            Serial.println("Error reading temperature from SPIFFS");
            break;
        }

        bytesRead = file.read((uint8_t *)&batteryVoltage, sizeof(batteryVoltage));
        if (bytesRead != sizeof(batteryVoltage))
        {
            Serial.println("Error reading battery voltage from SPIFFS");
            break;
        }

        // sensorReadings.clearFields();
        Serial.print("temperature:");
        Serial.println(temperature);
        Serial.print("voltage :");
        Serial.println(batteryVoltage);
        // sensorReadings.addField("battery_voltage", batteryVoltage);

        Serial.print("Writing data from SPIFFS: ");
        // Serial.println(client.pointToLineProtocol(sensorReadings));

        // client.writePoint(sensorReadings);
    }

    file.close();

    // Delete data file from SPIFFS after upload
    SPIFFS.remove(DATA_FILE);
}
void setup()
{
    Serial.begin(115200);
    // Initialize SPIFFS
    if (!SPIFFS.begin(true))
    {
        Serial.println("Failed to mount file system");
        return;
    }
    saveDataToSPIFFS(33.55, 85.77);
    saveDataToSPIFFS(13.57, 75.77);
    saveDataToSPIFFS(63.55, 36.77);
    saveDataToSPIFFS(13.35, 25.77);
    saveDataToSPIFFS(93.55, 14.77);
    uploadDataFromSPIFFS();
}
void loop()
{
}
