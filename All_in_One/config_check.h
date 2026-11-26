// config_check.h - checks config.h and derives the code's build switches.
// Not edited per machine. Every problem stops the build with a message
// naming the setting in config.h.
#pragma once

#include <esp_arduino_version.h>

// The firmware is built and tested with exactly this ESP32 board package.
#if ESP_ARDUINO_VERSION_MAJOR != 2 || ESP_ARDUINO_VERSION_MINOR != 0 || ESP_ARDUINO_VERSION_PATCH != 17
#error "Install ESP32 board package 2.0.17 (Arduino IDE: Tools > Board > Boards Manager > esp32 > 2.0.17)"
#endif

// Values for the settings in config.h.
#define TYPE_STANDARD 1
#define TYPE_TWO_GROUP_160 2
#define NETWORK_GSM 1
#define NETWORK_WIFI 2
#define SERVER_PRODUCTION 1
#define SERVER_STAGING 2

// The old per-machine switches now come from config.h.
#if defined(USE_WIFI) || defined(IS_DEVELOPMENT) || defined(HAS_RFID_MODULE) || defined(HAS_BKASH_MODULE) || defined(HAS_CASH_MODULE)
#error "Old setting found (USE_WIFI, IS_DEVELOPMENT or HAS_*_MODULE): set NETWORK, SERVER and HAS_RFID/HAS_BKASH/HAS_CASH in config.h instead"
#endif

// --- every setting is present ----------------------------------------------
#ifndef MACHINE_ID
#error "config.h: MACHINE_ID is missing"
#endif
#ifndef MACHINE_TYPE
#error "config.h: MACHINE_TYPE is missing"
#endif
#ifndef MACHINE_SIZE
#error "config.h: MACHINE_SIZE is missing"
#endif
#ifndef NETWORK
#error "config.h: NETWORK is missing"
#endif
#if !defined(WIFI_SSID) || !defined(WIFI_PASSWORD)
#error "config.h: WIFI_SSID and WIFI_PASSWORD must be there (\"\" on GSM machines)"
#endif
#if !defined(HAS_RFID) || !defined(HAS_BKASH) || !defined(HAS_CASH)
#error "config.h: HAS_RFID, HAS_BKASH and HAS_CASH must all be there (1 or 0)"
#endif
#ifndef SERVER
#error "config.h: SERVER is missing"
#endif

// --- every setting has an allowed value ------------------------------------
// (A misspelled value such as TYPE_STANDRD counts as 0 here, so it fails too.)
#if MACHINE_TYPE != TYPE_STANDARD && MACHINE_TYPE != TYPE_TWO_GROUP_160
#error "config.h: MACHINE_TYPE must be TYPE_STANDARD or TYPE_TWO_GROUP_160"
#endif
#if MACHINE_SIZE != 20 && MACHINE_SIZE != 40 && MACHINE_SIZE != 60 && MACHINE_SIZE != 120 && MACHINE_SIZE != 160
#error "config.h: MACHINE_SIZE must be 20, 40, 60, 120 or 160"
#endif
#if NETWORK != NETWORK_GSM && NETWORK != NETWORK_WIFI
#error "config.h: NETWORK must be NETWORK_GSM or NETWORK_WIFI"
#endif
#if SERVER != SERVER_PRODUCTION && SERVER != SERVER_STAGING
#error "config.h: SERVER must be SERVER_PRODUCTION or SERVER_STAGING"
#endif
#if (HAS_RFID != 0 && HAS_RFID != 1) || (HAS_BKASH != 0 && HAS_BKASH != 1) || (HAS_CASH != 0 && HAS_CASH != 1)
#error "config.h: HAS_RFID, HAS_BKASH and HAS_CASH must each be 1 (installed) or 0 (not installed)"
#endif
#if !HAS_RFID && !HAS_BKASH && !HAS_CASH
#error "config.h: no payment module installed; set at least one of HAS_RFID, HAS_BKASH, HAS_CASH to 1"
#endif

// --- the settings work together ---------------------------------------------
#if MACHINE_TYPE == TYPE_TWO_GROUP_160
#if MACHINE_SIZE != 160
#error "config.h: TYPE_TWO_GROUP_160 needs MACHINE_SIZE 160"
#endif
#if HAS_CASH
#error "config.h: TYPE_TWO_GROUP_160 can't have HAS_CASH 1: the cash acceptor's input (GPIO 35) is a limit switch on this machine"
#endif
#if !HAS_RFID
#error "config.h: TYPE_TWO_GROUP_160 needs HAS_RFID 1: the customer picks a group with a button, then taps the card"
#endif
#endif

namespace config_check {
constexpr bool sameText(const char* a, const char* b) {
  return *a == *b && (*a == '\0' || sameText(a + 1, b + 1));
}
constexpr bool idCharsOk(const char* s) {
  return *s == '\0' || (((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') ||
                         *s == '-' || *s == '_') &&
                        idCharsOk(s + 1));
}
}  // namespace config_check

static_assert(sizeof(MACHINE_ID) > 1, "config.h: MACHINE_ID is empty");
static_assert(sizeof(MACHINE_ID) <= 33, "config.h: MACHINE_ID is longer than 32 characters");
static_assert(config_check::idCharsOk(MACHINE_ID),
              "config.h: MACHINE_ID may only contain letters, digits, - and _ (no spaces)");
static_assert(!config_check::sameText(MACHINE_ID, "CHANGE_ME"),
              "config.h: set MACHINE_ID to this machine's ID from the VMS dashboard");
#if NETWORK == NETWORK_WIFI
static_assert(sizeof(WIFI_SSID) > 1, "config.h: WIFI_SSID is empty, but NETWORK is NETWORK_WIFI");
#endif

// --- build switches used by the code (derived; never set these by hand) ----
#if NETWORK == NETWORK_WIFI
#define USE_WIFI
#endif
#if SERVER == SERVER_STAGING
#define IS_DEVELOPMENT
#endif
#if HAS_RFID
#define HAS_RFID_MODULE
#endif
#if HAS_BKASH
#define HAS_BKASH_MODULE
#endif
#if HAS_CASH
#define HAS_CASH_MODULE
#endif
#define IS_TWO_GROUP (MACHINE_TYPE == TYPE_TWO_GROUP_160)
