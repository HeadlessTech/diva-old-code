// config.h - THE ONLY FILE TO EDIT FOR EACH MACHINE
//
// Fill in every setting below, save, then flash:
//   - flash.bat COM5   (in the repository folder), or
//   - Arduino IDE: open All_in_One.ino, board "ESP32 Dev Module", Upload.
// If a setting is missing or wrong, the build stops with a message that says
// which one, so a half-configured firmware never reaches a machine.

// Machine ID, exactly as in the VMS dashboard (letters, digits, - and _).
#define MACHINE_ID "CHANGE_ME"

// Machine type:
//   TYPE_STANDARD       20, 40, 60, 120 and 160 machines
//   TYPE_TWO_GROUP_160  160 machine with two groups: BELT (top button) and
//                       PANTY (bottom button)
#define MACHINE_TYPE TYPE_STANDARD

// Capacity: 20, 40, 60, 120 or 160
#define MACHINE_SIZE 20

// Internet connection: NETWORK_GSM (SIM card) or NETWORK_WIFI
#define NETWORK NETWORK_GSM

// Wi-Fi only: this site's Wi-Fi name and password. Leave "" on GSM machines.
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Installed modules: 1 = installed, 0 = not installed
#define HAS_RFID 1
#define HAS_BKASH 1
#define HAS_CASH 0

// Server:
//   SERVER_PRODUCTION  machines in the field (vms.headless.ltd)
//   SERVER_STAGING     testing only (vms-new.headless.ltd)
#define SERVER SERVER_PRODUCTION
