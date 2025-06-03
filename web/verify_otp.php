<?php

function base32Decode($base32String) {
    $alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ234567';
    $base32String = strtoupper($base32String);
    $base32String = str_replace('=', '', $base32String);
    $binaryString = '';

    for ($i = 0; $i < strlen($base32String); $i++) {
        $char = $base32String[$i];
        $binaryString .= str_pad(base_convert(strpos($alphabet, $char), 10, 2), 5, '0', STR_PAD_LEFT);
    }

    $byteArray = [];
    for ($i = 0; $i < strlen($binaryString); $i += 8) {
        $byteArray[] = bindec(substr($binaryString, $i, 8));
    }

    return pack('C*', ...$byteArray);
}

function generateTOTP($key, $timeSlice, $period, $codeDigits) {
    $timeBytes = pack('N*', 0) . pack('N*', $timeSlice);
    $hash = hash_hmac('sha1', $timeBytes, $key, true);

    $offset = ord($hash[19]) & 0xf;
    $binary = (ord($hash[$offset]) & 0x7f) << 24 |
              (ord($hash[$offset + 1]) & 0xff) << 16 |
              (ord($hash[$offset + 2]) & 0xff) << 8 |
              (ord($hash[$offset + 3]) & 0xff);

    $otp = $binary % pow(10, $codeDigits);
    return str_pad($otp, $codeDigits, '0', STR_PAD_LEFT);
}

function verifyTOTP($base32Key, $otp, $window = 1, $period = 30) {
    $key = base32Decode($base32Key);
    $timeSlice = floor(time() / $period);

    for ($i = -$window; $i <= $window; $i++) {
        $calculatedOtp = generateTOTP($key, $timeSlice + $i, $period, 6);
        if ($calculatedOtp === $otp) {
            return true;
        }
    }
    return false;
}

