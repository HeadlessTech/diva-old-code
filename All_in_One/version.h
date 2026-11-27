// version.h - firmware version. Raise it with every release that goes to
// machines (major.minor.patch); the build date and time are added for you.
#pragma once

#define FIRMWARE_VERSION "2.0.0"

// Sent with every "online" status (the server logs it) and printed at boot,
// e.g. "2.0.0 STD Nov 27 2026 10:15:00" (STD = standard, TG = two-group).
#if IS_TWO_GROUP
#define FW_VERSION FIRMWARE_VERSION " TG " __DATE__ " " __TIME__
#else
#define FW_VERSION FIRMWARE_VERSION " STD " __DATE__ " " __TIME__
#endif
