#include <SPI.h>
#include <MFRC522.h>
#include <DS1302.h>
#include <Preferences.h>

Preferences preferences;
#define Led_pin 14
#define Buzzer_pin 12
// RTC
#define RTC_SET_FLAG 0xAA
#define RTC_DAT 21
#define RTC_SCL 22
#define RTC_RST 16

// RFID
#define SDA_PIN 5
// use together RTC&RFID
#define RST_PIN 13

MFRC522 rfid(SDA_PIN, RST_PIN);
DS1302 rtc(RTC_RST, RTC_DAT, RTC_SCL);
touch_pad_t touchPin;

RTC_DATA_ATTR int bootCount = 0;

int THRESHOLD = 20;

const char *dayToString(Time::Day day)
{
    switch (day)
    {
    case Time::kSunday:
        return "Sunday";
    case Time::kMonday:
        return "Monday";
    case Time::kTuesday:
        return "Tuesday";
    case Time::kWednesday:
        return "Wednesday";
    case Time::kThursday:
        return "Thursday";
    case Time::kFriday:
        return "Friday";
    case Time::kSaturday:
        return "Saturday";
    default:
        return "Unknown";
    }
}

unsigned long lastTouchTime = 0;
const unsigned long sleepDelay = 10000;
void Timeout10s_SLEEP()
{
    unsigned long currentTime = millis();
    if (currentTime - lastTouchTime >= sleepDelay)
    {
        Serial.println("time out");
        delay(500);
        Serial.println("Going to sleep now");
        // Enter deep sleep
        esp_deep_sleep_start();
    }
}

void print_wakeup_touchpad()
{
    touchPin = esp_sleep_get_touchpad_wakeup_status();

    switch (touchPin)
    {
    case 0:
        Serial.println("Touch detected on GPIO 4");
        break;
    case 7:
        Serial.println("Touch detected on GPIO 27");
        break;
    default:
        Serial.println("Wakeup not by touchpad");
        break;
    }
}

bool readUID()
{
    unsigned long startTime = millis();

    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial())
    {
        // ... (existing code)
        beep_beep();
        // Display the UID
        Serial.print("UID:");
        String currentUID = "";
        for (int i = 0; i < rfid.uid.size; i++)
        {
            currentUID += (rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
            currentUID += String(rfid.uid.uidByte[i], HEX);
        }
        // Serial.println(currentUID);

        // Store UID in NVS
        preferences.putString("lastUID", currentUID);

        // Store UID, date, and time in NVS
        storeUIDInNVS(currentUID);

        // stamp_time();
        lastTouchTime = millis();
        return true; // Found RFID card, exit loop
    }
    return false;
}

void storeUIDInNVS(String uid)
{
    Time now = rtc.time();
    String entry = uid + " " + dayToString(now.day) + " " +
                   String(now.yr) + "-" +
                   (now.mon < 10 ? "0" : "") + String(now.mon) + "-" +
                   (now.date < 10 ? "0" : "") + String(now.date) + " " +
                   (now.hr < 10 ? "0" : "") + String(now.hr) + ":" +
                   (now.min < 10 ? "0" : "") + String(now.min) + ":" +
                   (now.sec < 10 ? "0" : "") + String(now.sec);

    Serial.println(entry);

    // Save the entry in NVS
    preferences.putString("entries", preferences.getString("entries", "") + entry + "\n");
}

void printTwoDigits(int number)
{
    if (number < 10)
    {
        Serial.print("0");
    }
    Serial.print(String(number));
}

void stamp_time()
{
    Time now = rtc.time();
    Serial.print(String(now.yr));
    Serial.print("-");
    printTwoDigits(now.mon);
    Serial.print("-");
    printTwoDigits(now.date);
    Serial.print(" ");
    Serial.print(dayToString(now.day));
    Serial.print(" ");
    printTwoDigits(now.hr);
    Serial.print(":");
    printTwoDigits(now.min);
    Serial.print(":");
    printTwoDigits(now.sec);
    Serial.println();
    delay(1000);
}

void beep_beep()
{
    digitalWrite(Buzzer_pin, HIGH);
    delay(50);
    digitalWrite(Buzzer_pin, LOW);
    delay(50);
}

void setup()
{
    Serial.begin(115200);
    pinMode(Led_pin, OUTPUT);
    pinMode(Buzzer_pin, OUTPUT);

    SPI.begin();     // Initialize SPI bus
    rfid.PCD_Init(); // Initialize RFID module
    delay(50);
    stamp_time();
    bootCount++;
    Serial.println("Boot number: " + String(bootCount));
    delay(50);
    print_wakeup_touchpad(); // display GPIO
    touchPin = esp_sleep_get_touchpad_wakeup_status();
    touchSleepWakeUpEnable(4, THRESHOLD);
    touchSleepWakeUpEnable(27, THRESHOLD);
    Serial.flush();
    rtc.writeProtect(false); // can edit time
    rtc.halt(false);         // stop time if true
    uint8_t flag = rtc.readRam(0);
    if (flag != RTC_SET_FLAG)
    {
        uint16_t yr;
        uint8_t mon, date, hr, min, sec;
        Time::Day day;

        Serial.println();
        Serial.println("RTC time NOT set");
        Serial.println("Enter format yyyy/mm/dd hh:mm:ss weekday");
        while (!Serial.available())
            ;
        String dtString = Serial.readStringUntil('\n');
        sscanf(dtString.c_str(), "%hu/%hhu/%hhu %hhu:%hhu:%hhu %hhu",
               &yr, &mon, &date, &hr, &min, &sec, &day);

        Time now(yr, mon, date, hr, min, sec, day);
        rtc.time(now);
        rtc.writeRam(0, RTC_SET_FLAG);
        Serial.println("successfully set time to" + dtString);
    }
    preferences.begin("myApp", false); // Open the NVS
    if (touchPin == 7)
    {
        String allEntries = preferences.getString("entries", "");
        Serial.println("CSV Data\n" + allEntries);
        beep_beep();
        beep_beep();
    }
    if (touchPin == 0)
    {
        beep_beep();
        beep_beep();
    }
    Serial.println("Scan ID Card ...");
}
void loop()
{

    digitalWrite(Led_pin, HIGH);

    // Serial.printf("checkbeep = %d\n", checkbeep);
    if (touchPin == 0)
    {
        if (readUID())
        {
            Serial.println("Check in successful");
            Serial.println("Go to Sleep");
            esp_deep_sleep_start();
        }
    }
    else if (touchPin == 7)
    {
        if (readUID())
        {
            Serial.println("Check in successful");
            Serial.println("Go to Sleep");
            esp_deep_sleep_start();
        }
    }
    else
    {
        if (readUID())
        {
            Serial.println("Check in successful");
            Serial.println("Go to Sleep");
            esp_deep_sleep_start();
        }
    }

    Timeout10s_SLEEP();
}