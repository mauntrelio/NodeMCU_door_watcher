<?php

include "config.php";
include "verify_otp.php";

$title = $_GET["title"];
$body = $_GET["body"];
$email = $_GET["email"];
$channel = $_GET["channel"];
$otp = $_GET["otp"];

// Function to return a response with the HTTP error
function sendHttpResponse($statusCode, $message) {
    http_response_code($statusCode);
    header("Content-Type: application/json");
    echo json_encode(["error" => $message]);
    exit();
}

// Check for the presence of the OTP and its validity
if (empty($otp)) {
    sendHttpResponse(401, "OTP missing.");
}

if (!verifyTOTP($OTP_BASE32_KEY, $otp, $OTP_TOLERANCE)) {
    sendHttpResponse(401, "OTP invalid.");
}

$json = array("body" => $body, "title" => $title, "type" => "note");

// Set the email address or channel, or use the default channel
if ($email) {
    $json["email"] = $email;
} elseif ($channel) {
    $json["channel_tag"] = $channel;
} else {
    $json["channel_tag"] = $PUSHBULLET_CHANNEL; // Use the default channel
}

$data = json_encode($json);

$ch = curl_init($PUSHBULLET_URL);
curl_setopt($ch, CURLOPT_CUSTOMREQUEST, "POST");
curl_setopt($ch, CURLOPT_POSTFIELDS, $data);
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
curl_setopt($ch, CURLOPT_HTTPHEADER, array(
    "Content-Type: application/json",                                        
    "Content-Length: " . strlen($data),
    "Access-Token: " . $PUSHBULLET_KEY
));

$result = curl_exec($ch);
$httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);

if ($httpCode >= 200 && $httpCode < 300) {
    $data = json_decode($result, true);
    header("Content-Type: application/json");
    echo json_encode(["result" => "success", "iden" => $data["iden"] ?? ""]);
} else {
    sendHttpResponse(500, "Failed to send push notification.");
}

curl_close($ch);
