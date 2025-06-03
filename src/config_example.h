#include <pins_arduino.h>

const int I2C_ADDR = 0x3F;

const int PIN_SENSOR = D7;  
const int PIN_LIGHT = D0;
const int PIN_SCL = D2;
const int PIN_SDA = D3;
const int PIN_BUZZER = D5;
const int PIN_SWITCH_0 = D6;
const int PIN_SWITCH_1 = D1;
const int PIN_LED = D8;

const int BUZZER_FREQ = 2500;
const int ALARM_START = 20;
const int ALARM_END = 7;
const int SYNC_INTERVAL = 7200;

// hostname of server providing service as
// - pushbullet notification gateway
// - dynamic DNS of local IP address
const char* HOST = "HOSTNAME_SERVICE";
// name to register in the dynamic DNS
const char* NAME = "NAME_TO_REGISTER";
// parameters for Wi-Fi connection
const char* SSID = "Your_SSID";
const char* PASS = "Your_WiFi_Password";
// pre-shared key to generate TOTP for authentication over HTTP
uint8_t secretKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const char* format_time = "%H:%M %d.%m.%y";
// char format_date[] = "%a %d %b %Y";
// char format_date[] = "%d/%m/%Y T\xF5r";

// Central European Time (Berlin, Rome)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};  // Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};    // Central European Standard Time
Timezone CE(CEST, CET);

// custom char for bell
uint8_t bell[8]  = {0x04,0x0e,0x0e,0x0e,0x1f,0x00,0x04,0x00};
