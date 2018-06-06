#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <time.h>
#include <Timezone.h>
#include <TimeLib.h>
#include <Ticker.h>

#include <Wire.h>
#include <hd44780.h> // include hd44780 library header file
#include <hd44780ioClass/hd44780_I2Cexp.h> // i2c LCD i/o class header

#include "../lib/NTPSync/NTPSync.h"
#include "../lib/AsyncHTTPClient/AsyncHTTPClient.h"

#include "config.h"

bool ledState = false;
bool alarm_status = false;
bool v_dots = false;
int door_status = -1; // unknown door status (0 = closed, 1 = open)
int clock_loops = 0;
char last_alert[25] = "";  // last pushbullet alert id

// lcd display
hd44780_I2Cexp lcd;

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
  digitalWrite(PIN_LIGHT, LOW);
  noTone(PIN_BUZZER);
  lcd.backlight();
  switching(PIN_SWITCH_0, PIN_LED);
  switching(PIN_SWITCH_1, PIN_LIGHT);
}

Ticker timer_alarm(blinker, 250);

// check if is time for the alarm to beep
bool isAlarmOn(Timezone tz)
{
  int button = digitalRead(PIN_SWITCH_0);
  if (button == HIGH) {
    return false;
  }
	time_t utc = now();
	time_t local_time = tz.toLocal(utc);
  int H = hour(local_time);
  return (H >= ALARM_START || H <= ALARM_END);
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
  char requestBody[160] = "GET /services/sendalert.php";
  char dateTime[25];
  formatTime(dateTime, format_time, CE);
  urlEncode(dateTime);
  strcat(requestBody, "?body=");
  strcat(requestBody, dateTime);
  strcat(requestBody,"%20-%20T%C3%BCr%20ist%20offen!&title=T%C3%BCr%20Beobachtung");
  Serial.print("Requesting URL: ");
  Serial.println(requestBody);
  strcat(requestBody, " HTTP/1.0\r\nHost: ");
  strcat(requestBody,  HOST);
  strcat(requestBody, "\r\n\r\n");
  sendHTTP(String(requestBody), last_alert, HOST);
}

// Cancel push notification (door has been closed)
void cancelPush()
{
  char responseBody[10];
  if (strcmp(last_alert,"") == 0) {
    Serial.println("last_alert is EMPTY, no message to dismiss!");
    return;
  }
  char requestBody[160] = "GET /services/dismissalert.php?id=";
  strcat(requestBody,last_alert);
  Serial.print("Requesting URL: ");
  Serial.println(requestBody);
  strcat(requestBody, " HTTP/1.0\r\nHost: ");
  strcat(requestBody,  HOST);
  strcat(requestBody, "\r\n\r\n");
  sendHTTP(String(requestBody), responseBody, HOST);
  strcpy(last_alert,"");
}

// Start the alarm
void startAlarm()
{
  // alarm already running
  if (alarm_status)
    return;

  Serial.println("Starting the alarm");
  timer_alarm.start();
  alarm_status = true;
  sendPush();
}

// Stop the alarm
void stopAlarm()
{
  // alarm not running
  if (!alarm_status)
    return;

  Serial.println("Stopping the alarm");
  timer_alarm.stop();
  // reset the physical alarms
  blinkReset();
  alarm_status = false;
  cancelPush();
}

// Display current time and date in LCD
void displayTime()
{
  char buffer[16];
  formatTime(buffer, format_time, CE);
  lcd.setCursor(0,1);
  lcd.print(buffer);
}

// Update the lcd display
void clockUpdate()
{
  // display bell if alarm is on
  lcd.setCursor(15, 0);
  (isAlarmOn(CE)) ? lcd.print('\001') : lcd.print(" ");

  switching(PIN_SWITCH_0, PIN_LED);
  switching(PIN_SWITCH_1, PIN_LIGHT);

  // update the clock display every 15 seconds
  if (clock_loops == 0) {
    displayTime();
    // start the alarm if that is the case
    (door_status == 1 && isAlarmOn(CE)) ? startAlarm() : stopAlarm();
  }

  lcd.setCursor(2, 1);
  // blink the ":" between hour and minute every half second
  (v_dots) ? lcd.write(":") : lcd.print(" ");
  v_dots = !v_dots;
  clock_loops++;

  if (clock_loops == 30)
    clock_loops = 0;
}

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
  } else if (val == LOW && door_status != 0) {
    lcd.home();
    lcd.print("T\xF5r ist zu.   ");
    Serial.println("Door is closed!");
    stopAlarm();
    door_status = 0;
  }
}

// setup
void setup()
{
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

  lcd.clear();
  lcd.print("Connected! IP:");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // initialize time sync
  setSyncProvider(getNtpTime);
  setSyncInterval(SYNC_INTERVAL);

	delay(2000);
  lcd.clear();

  // start the clock
  timer_clock.start();

} // setup()

void loop()
{
  timer_clock.update(); // the clock
  timer_alarm.update(); // the alarm (if running)
  sensor(); // the door sensor
} // loop()
