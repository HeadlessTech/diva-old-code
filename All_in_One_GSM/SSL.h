// SSL.h
#pragma once

#include <memory>
#include <SSLClient.h>

#include "AWS_Root_CA.h"
#include "GsmClient.h"

#define MQTT_PACKET_SIZE 1024

std::unique_ptr<SSLClient> gsmSSLClient = nullptr;

// my_cert is Device Certificate
const char my_cert[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIDWjCCAkKgAwIBAgIVAIG44ADEfA3KXbsrd/wbHAobfNOCMA0GCSqGSIb3DQEB\n"
"CwUAME0xSzBJBgNVBAsMQkFtYXpvbiBXZWIgU2VydmljZXMgTz1BbWF6b24uY29t\n"
"IEluYy4gTD1TZWF0dGxlIFNUPVdhc2hpbmd0b24gQz1VUzAeFw0yMzA0MDkwNjU2\n"
"NTNaFw00OTEyMzEyMzU5NTlaMB4xHDAaBgNVBAMME0FXUyBJb1QgQ2VydGlmaWNh\n"
"dGUwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDBPU4g/QIA+t3F/tgg\n"
"5ZWcYq8AeiyfAmXN2pqNp52vsvERREnUrbdSxv1YPW+0GRj2x/EJS4coX8XZGQZo\n"
"tOyuEW94o87TF/zr+jgkG2bkSqzvUgy8Ekm5jvndGtrXQ+RVhyTX4XruCnV/Bav7\n"
"taxzaV0FUI1Z6urHZ4MyLj28vT8MwScsUC8KTLZQC8vasiI8TjaXipjSHxed4Y1f\n"
"urZ0nGi4Q9aLT2I7m2PyI0zP5gFqXznANQZMF7ltCo3Uvvmg42obP6OgdjZ8s0HG\n"
"4KIMO3hOMxFlUT5Z7vHUkHWNS1RDb2OXSLeHzCtGkF4DYFV5/OS64PJvAmpWg75y\n"
"JqH7AgMBAAGjYDBeMB8GA1UdIwQYMBaAFN/KAjznh+Y5NZCCgTa/5k0CGIi9MB0G\n"
"A1UdDgQWBBQgNk1YOb4ZZIXdhy2Q0agj3ATEADAMBgNVHRMBAf8EAjAAMA4GA1Ud\n"
"DwEB/wQEAwIHgDANBgkqhkiG9w0BAQsFAAOCAQEAd7AayVGt/FQkeUGkV0dwbppa\n"
"HAzVxv1TihxF3yQdWOxxk1OaxXNRpJtVSzirQzg6+syKsqE5w2yhEc9+DHiUN2Ks\n"
"1zqIakNEk9R7mP9SbxFvKnEKDis4yKuB5YHgM6vHBabUOdmC5hKcVg/KzOjKUMOe\n"
"mJpi/rd9h1VyT1Y023juedq3UaZacHBKTVaTdsdiPUucP8fm9jqW6uhPZA0dfIig\n"
"HI3lcC+bD86ANv67WMICvJpaqsgGuq8XBz7aQWgwq2bRg85Mzs5DRlgtjvHm1xr+\n"
"3dGpgbMc7QVBpfweGVmZ1FVqi6jSlTUY5ff8ojQm8BV5xL2bBtSUh+LMghuySA==\n"
"-----END CERTIFICATE-----\n";


// my_key is Device Private Key
const char my_key[] =
"-----BEGIN RSA PRIVATE KEY-----\n"
"MIIEogIBAAKCAQEAwT1OIP0CAPrdxf7YIOWVnGKvAHosnwJlzdqajaedr7LxEURJ\n"
"1K23Usb9WD1vtBkY9sfxCUuHKF/F2RkGaLTsrhFveKPO0xf86/o4JBtm5Eqs71IM\n"
"vBJJuY753Rra10PkVYck1+F67gp1fwWr+7Wsc2ldBVCNWerqx2eDMi49vL0/DMEn\n"
"LFAvCky2UAvL2rIiPE42l4qY0h8XneGNX7q2dJxouEPWi09iO5tj8iNMz+YBal85\n"
"wDUGTBe5bQqN1L75oONqGz+joHY2fLNBxuCiDDt4TjMRZVE+We7x1JB1jUtUQ29j\n"
"l0i3h8wrRpBeA2BVefzkuuDybwJqVoO+ciah+wIDAQABAoIBAGaYUOHM0oKIrrRP\n"
"cJpOqlckQbukIgquMDTSClqJEnxVKg47zmMfdAR758gwnhzlck6bqwp/UhwKdTWl\n"
"RrTS0hI86Y0Ik2eaBcuXxrTM45zzVMufKWm03/Wnxlswxd2iixkprlL7wNOfM/Cj\n"
"p0L1msCPwTZwJQwHYG1MNOiwilCoz1YtjEoxqDTow6Z+1CsnX8vBPPqqrLw+T3zI\n"
"riwzuhk6RvuU6EqEfEWCcR5Ooknr4yzw60+lGnbl9TlX0adrh0WxzCAjBhkVl4z+\n"
"q0okB3r3mZyqwtAt4xPSiBeSFiD3p7L30oiBdcDexf2WeUwlVBIqP/iUlPaZ1e2N\n"
"A3kBUiECgYEA+57u2b/17kL7cjWxAYPZb12UZm6s0+ZjBuW+CeEMipYAFb35v24K\n"
"ifGqgKqrFioaKTh+lVZc+xNehPTC6jpd1j6LhwSJOdtGZ07Nbl68W5oChxPfTlng\n"
"YL7O/QXhZvSp/6rdachxqiQ4ub/JOUoJ7YMEgKaRFGBKF8tZd+D7hSsCgYEAxJpC\n"
"yxbUrUfyW+yaWm8C8BM6U5AouSZOb1tHrs8AaPGJ1V+heY2NMQiYRz0GICUKdIDs\n"
"q3FlmzpALNhrG+i91gqv3jBuzzaHipboc0bVHrOfH+pyBO4yj7zTEoIi5rJeLp61\n"
"tPhMhpHmyBGULCDYCL/PRQDGKac5qUjFx/JJjnECgYALV2yYsAhbxvUifr0Nvm2s\n"
"kWFBBLgRjQ7lnrFkLJquHHjAgiGhuUoMUbJHvmmbxCYNuZgrDwt93a+i+o1WlW7p\n"
"W0Rav5+X1gRkX67uzKJc1qJKiGDGXVK5yZ/2068HPvvypDb6r6g/7QYJiVV+mZG8\n"
"/OzVB6z+aCCu1eGgf6mWpQKBgELaJa5DZCxME7h4WwA3UWtrTmEN2xsiUHeDNXvW\n"
"vc/xminpK9JWdNVgk89di9CwZ763/EJL+/3tr9bP/JPv72VyGt1V4MgrRam1qPNR\n"
"+bRH5esHYKAWkzDXR3q6r718l5QIuccAy5zSzgPoHldnD67h8kwHeFq/l1bd/QsB\n"
"c0uxAoGAc09bm/A8UTn03Q+5FMic/upJyAjBC/nrmqVS2JvhzLNtKWk+LQUIFGqA\n"
"wmaYU4rHVyJQ2OFU9FDpoi/DoNIFajGuJO12IQpCkxgTKX/4xCsuX0i9k5gPr5BV\n"
"2Vj8OXv10b6B9BfxsyiI3bOn9M9v4H09nB8k/WSCt12UksqvPms=\n"
"-----END RSA PRIVATE KEY-----\n";

// TLS Authentication
SSLClientParameters mTLS = SSLClientParameters::fromPEM(my_cert, sizeof my_cert, my_key, sizeof my_key);

void initializeSSL() {
  Serial.printf("Free heap before initializeSSL: %d\n", ESP.getFreeHeap());

  // Check if memory is critically low before initializing SSL
  if (ESP.getFreeHeap() < 45000) {  // Set a low memory threshold, e.g., 70 KB
    Serial.println("Very low memory detected. Restarting ESP32 to prevent crash.");
    delay(100);  // Short delay for Serial output
    ESP.restart();  // Restart ESP32 to clear memory
    return;
  }

  Serial.printf("Free heap after deleting GSM SSL: %d\n", ESP.getFreeHeap());
  // Ensure only Wi-Fi SSL client is active
  if (!gsmSSLClient) {
    gsmSSLClient.reset(new SSLClient(gsmClient, TAs, TAs_NUM, A5));
    gsmSSLClient->setMutualAuthParams(mTLS);
  }
  delay(200);  // Short delay for memory stabilization

  Serial.printf("Free heap after initializeSSL: %d\n", ESP.getFreeHeap());
}

