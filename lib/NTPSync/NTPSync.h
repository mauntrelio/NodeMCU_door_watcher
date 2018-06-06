#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
static const char ntpServerName[] = "0.de.pool.ntp.org";

time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
