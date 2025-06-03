#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <time.h>
#include <Timezone.h>
#include <TimeLib.h>
#include <Ticker.h>
#include <TOTP.h>

#include <Wire.h>
#include <hd44780.h> // include hd44780 library header file
#include <hd44780ioClass/hd44780_I2Cexp.h> // i2c LCD i/o class header

#include "../lib/NTPSync/NTPSync.h"
#include "../lib/AsyncHTTPClient/AsyncHTTPClient.h"

#include "config.h"

// web server
AsyncWebServer server(80);
// web event source
AsyncEventSource events("/events");

IPAddress localIP;
bool ledState = false;
bool alarm_flag = false; // alarm is beeping or not
int alarm_status = 0; // unknown alarm status (0 = off, 1 = on, not in the time, 2 = on, in time, door closed, 3 = rings)
bool v_dots = false;
int door_status = -1; // unknown door status (0 = closed, 1 = open)
int clock_loops = 0;

// lcd display
hd44780_I2Cexp lcd;

// handle web request for data
char * webData()
{
  // prepare output data
  static char infoDoor[2];
  static char infoAlarm[2];
  static char json[16];
  snprintf(infoDoor, 2,"%d", door_status);
  snprintf(infoAlarm, 2,"%d", alarm_status);

  // spit out JSON with data
  strcpy(json, "{\"d\":");
  strcat(json, infoDoor);
  strcat(json, ",\"a\":");
  strcat(json, infoAlarm);
  strcat(json, "}");
  return json;
}

void urlEncode(char *string)
{
    char charToEncode;
    int posToEncode;
    while (((posToEncode=strspn(string,"1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_.~"))!=0) &&(posToEncode<strlen(string)))
    {
        charToEncode=string[posToEncode];
        memmove(string+posToEncode+3,string+posToEncode+1,strlen(string+posToEncode));
        string[posToEncode]='%';
        string[posToEncode+1]="0123456789ABCDEF"[charToEncode>>4];
        string[posToEncode+2]="0123456789ABCDEF"[charToEncode&0xf];
        string+=posToEncode+3;
    }
}

// update an output based on an input
void switching(const int input, const int output) {
  if (digitalRead(input) == HIGH) {
    digitalWrite(output, LOW);
  } else {
    digitalWrite(output, HIGH);
  }
}

// physical alarm: blink led, beep buzzer, blink LCD backlight
void blinker()
{
  digitalWrite(PIN_LIGHT, ledState);
  digitalWrite(PIN_LED, ledState);
  if (ledState) {
    lcd.backlight();
    tone(PIN_BUZZER, BUZZER_FREQ);
  } else {
    lcd.noBacklight();
    noTone(PIN_BUZZER);
  }
  ledState = !ledState;
}

// reset physical alarm
void blinkReset()
{
  Serial.println("Reset blink");
  digitalWrite(PIN_LIGHT, LOW);
  noTone(PIN_BUZZER);
  lcd.backlight();
  switching(PIN_SWITCH_0, PIN_LED);
  switching(PIN_SWITCH_1, PIN_LIGHT);
}

// timer for the blinker (active alarm)
Ticker timer_alarm(blinker, 250);

// update the status of the alarm
void updateAlarmStatus(Timezone tz)
{
  int current_alarm_status = alarm_status;
  // first check the switch
  int button = digitalRead(PIN_SWITCH_0);
  alarm_status = (button == HIGH) ? 0 : 1;

  // alarm activated, run further checks
  if (alarm_status > 0) {
    // check the time
    time_t utc = now();
    time_t local_time = tz.toLocal(utc);
    int currentHour = hour(local_time);
    // update the status based on alarm activation time and door status
    if (currentHour >= ALARM_START || currentHour <= ALARM_END) {
      alarm_status = (door_status == 0) ? 2 : 3;
    };
  }

  // emit update status event for web clients
  if (alarm_status != current_alarm_status) {
    events.send(webData(),"status",millis());
  }
}

// Function to format time with time zone
void formatTime(char *buf, const char *_fmt, Timezone tz)
{
	time_t utc = now();
	time_t local_time = tz.toLocal(utc);
  struct tm *info;
  info = localtime(&local_time);
  strftime(buf, 16, _fmt, info);
}

// Send push notification (door is open)
void sendPush()
{

  TOTP totp = TOTP(secretKey, 10);
  String authCode = totp.getCode(now());
  Serial.print("OTP:" );
  Serial.println(authCode);

  char dateTime[25];
  formatTime(dateTime, format_time, CE);
  urlEncode(dateTime);

  char requestBody[180] = "GET /services/sendalert.php";
  strcat(requestBody, "?body=");
  strcat(requestBody, dateTime);
  strcat(requestBody,"%20-%20T%C3%BCr%20ist%20offen!&title=T%C3%BCr%20Beobachtung&otp=");
  strcat(requestBody, authCode.c_str());
  strcat(requestBody, " HTTP/1.0\r\nHost: ");
  strcat(requestBody,  HOST);
  strcat(requestBody, "\r\n\r\n");
  
  Serial.print("Requesting URL: ");
  Serial.println(requestBody);
  sendHTTP(String(requestBody), HOST);
}

// Register DNS
// void registerDNS()
// {
//   char requestBody[160] = "GET /services/dns.php";
//   strcat(requestBody, "?name=");
//   strcat(requestBody, NAME);
//   strcat(requestBody, "&ip=");
//   strcat(requestBody, localIP.toString().c_str());
//   Serial.print("Requesting URL: ");
//   Serial.println(requestBody);
//   strcat(requestBody, " HTTP/1.0\r\nHost: ");
//   strcat(requestBody,  HOST);
//   strcat(requestBody, "\r\n\r\n");
//   sendHTTP(String(requestBody), HOST);
// }

// Start the alarm
void startAlarm()
{
  // alarm already running
  if (alarm_flag)
    return;

  Serial.println("Starting the alarm");
  timer_alarm.start();
  alarm_flag = true;
  sendPush();
}

// Stop the alarm
void stopAlarm()
{
  // alarm not running
  if (!alarm_flag)
    return;

  Serial.println("Stopping the alarm");
  timer_alarm.stop();
  // reset the physical alarms
  blinkReset();
  alarm_flag = false;
}

// Display current time and date in LCD
void displayTime()
{
  char buffer[16];
  formatTime(buffer, format_time, CE);
  lcd.setCursor(0,1);
  lcd.print(buffer);
  Serial.println(buffer);
}

// Update the lcd display and status of the alarm
void clockUpdate()
{
  // update the status of the alarm
  updateAlarmStatus(CE);

  // display bell if alarm is on
  lcd.setCursor(15, 0);
  (alarm_status > 1) ? lcd.print('\001') : lcd.print(" ");

  // start/stop the alarm if that is the case
  (alarm_status == 3) ? startAlarm() : stopAlarm();

  // check the switches
  switching(PIN_SWITCH_0, PIN_LED);
  switching(PIN_SWITCH_1, PIN_LIGHT);

  // update the clock display every 15 seconds
  if (clock_loops == 0) {
    displayTime();
  }

  lcd.setCursor(2, 1);
  // blink the ":" between hour and minute every half second
  (v_dots) ? lcd.write(":") : lcd.print(" ");
  v_dots = !v_dots;
  clock_loops++;

  if (clock_loops == 30)
    clock_loops = 0;
}

// timer for the clock update
Ticker timer_clock(clockUpdate, 500);

// Read the door sensor
void sensor()
{
  int val = digitalRead(PIN_SENSOR);
  if (val == HIGH && door_status != 1) {
    lcd.home();
    lcd.print("T\xF5r ist offen!");
    Serial.println("Door is open!");
    door_status = 1;
    events.send(webData(),"status",millis());
  } else if (val == LOW && door_status != 0) {
    lcd.home();
    lcd.print("T\xF5r ist zu.   ");
    Serial.println("Door is closed!");
    stopAlarm();
    door_status = 0;
    events.send(webData(),"status",millis());
  }
}

// setup
void setup()
{

  // set up the web server
  // Initialize SPIFFS
  SPIFFS.begin();
  // web server routes
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", String(webData()));
  });  

  pinMode(PIN_LED, OUTPUT);
	pinMode(PIN_LIGHT, OUTPUT);
	pinMode(PIN_SENSOR, INPUT_PULLUP);
  pinMode(PIN_SWITCH_0, INPUT_PULLUP);
  pinMode(PIN_SWITCH_1, INPUT_PULLUP);

  Serial.begin(9600);

  // initialize i2c lcd
	Wire.begin(PIN_SDA, PIN_SCL);
	Wire.beginTransmission(I2C_ADDR);
  while (! Serial);
	lcd.begin(16, 2);
  lcd.setBacklight(255);
  lcd.clear();
	Serial.println("Starting...");
	lcd.print("Starting...");
  lcd.createChar(1, bell);

  // initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);
	Serial.print("Connecting to "); Serial.println(SSID);
	lcd.home();
	lcd.print("Connect "); lcd.print(SSID);
	lcd.setCursor(0, 1);
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
		lcd.print(".");
    counter++;
    if (counter == 17) {
      lcd.clear();
      lcd.print("Connect "); lcd.print(SSID);
      lcd.setCursor(0, 1);
      counter = 0;
    }
  }

  localIP = WiFi.localIP();

  lcd.clear();
  lcd.print("Connected! IP:");
  lcd.setCursor(0, 1);
  lcd.print(localIP.toString());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(localIP.toString());

  // register to the DNS
  // registerDNS();

  // initialize time sync
  setSyncProvider(getNtpTime);
  setSyncInterval(SYNC_INTERVAL);

	delay(2000);
  lcd.clear();

  // start the clock
  timer_clock.start();

 // start the web server
 server.addHandler(&events);
 server.begin();

} // setup()

// loop
void loop()
{
  timer_clock.update(); // the clock
  timer_alarm.update(); // blinks the alarm (if running)
  sensor(); // the door sensor
} // loop()
