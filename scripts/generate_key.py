#!/usr/bin/python

import os
import base64

def generate_preshared_key():
    # Generate 10 rando bytes
    key_bytes = os.urandom(10)

    # Base32 encoding for the PHP script
    base32_key = base64.b32encode(key_bytes).decode('utf-8').rstrip('=')

    # bytes array format for the ESP8266
    esp8266_array = [f"0x{b:02x}" for b in key_bytes]

    return base32_key, esp8266_array

# Generate the key
base32_key, esp8266_array = generate_preshared_key()

print("Base32 Key for PHP:", base32_key)
print("ESP8266 Key Array:", esp8266_array)

