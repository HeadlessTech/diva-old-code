// SSL.h
#pragma once

#include "src/SSLClient/SSLClient.h"

#include "IoT_Root_CA.h"  // our CAs (main + backup), see broker.h
#include "WifiClient.h"
#include "GsmClient.h"

#ifdef USE_WIFI
std::unique_ptr<SSLClient> wifiSSLClient = nullptr;
#else
std::unique_ptr<SSLClient> gsmSSLClient = nullptr;
#endif


void initializeSSL() {
  Serial.printf("Free heap before initializeSSL: %d\n", ESP.getFreeHeap());
  // Diagnostics only: SSLClient needs one contiguous block, which a total
  // free-heap figure does not guarantee.
  Serial.printf("Largest free block: %u (SSLClient: %u)\n",
                (unsigned)ESP.getMaxAllocHeap(), (unsigned)sizeof(SSLClient));

  // Check if memory is critically low before initializing SSL
  if (ESP.getFreeHeap() < 55000) {  // Set a low memory threshold, e.g., 70 KB
    Serial.println("Very low memory detected. Restarting ESP32 to prevent crash.");
    delay(50U);  // Short delay for Serial output
    ESP.restart();  // Restart ESP32 to clear memory
    return;
  }

#ifdef USE_WIFI
  if (!wifiSSLClient) {
    // The analog-pin argument is unused: the TLS random generator is seeded
    // from the ESP32 hardware RNG (SSLClient patch, tools/vendor-libraries.sh).
    // It used to be A5 = GPIO 33, the column-6 motor pin.
    wifiSSLClient.reset(new SSLClient(wifiClient, TAs, TAs_NUM, -1));
  }
#else
  if (!gsmSSLClient) {
    // The analog-pin argument is unused: the TLS random generator is seeded
    // from the ESP32 hardware RNG (SSLClient patch, tools/vendor-libraries.sh).
    // It used to be A5 = GPIO 33, the column-6 motor pin.
    gsmSSLClient.reset(new SSLClient(gsmClient, TAs, TAs_NUM, -1));
  }
#endif
  delay(200U);  // Short delay for memory stabilization

  Serial.printf("Free heap after initializeSSL: %d\n", ESP.getFreeHeap());
  Serial.printf("Min free heap: %u, loop stack left: %u\n",
                (unsigned)ESP.getMinFreeHeap(), (unsigned)uxTaskGetStackHighWaterMark(NULL));
}

