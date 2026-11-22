// Clock.h
#pragma once

// Real time for TLS certificate validation.
//
// The ESP32 has no battery-backed clock, so SSLClient (BearSSL) checks the
// broker certificate's validity dates against the firmware's *compile time*
// unless told otherwise. When AWS IoT rotates its broker certificate, every
// machine built before the new certificate's "valid from" date rejects it
// ("Certificate is expired or not yet valid.") and stops connecting. This is
// what took all MQTT machines offline on 2026-11-12.
//
// This file gets the real time once per boot and hands it to SSLClient:
//   1. Network time: SIM800 NTP (AT+CNTP) then the modem clock (AT+CCLK?,
//      network-broadcast time via AT+CLTS=1); or ESP32 SNTP on Wi-Fi, which
//      keeps running in the background and is adopted whenever it answers.
//   2. Otherwise the last good time saved in NVS (a lower bound, kept fresh
//      by clockMaintain()).
//   3. Otherwise the compile date.
// The best known time is applied with applyVerificationTime() every time the
// SSLClient is created or reconnects.

#include <time.h>
#include <sys/time.h>
#include <Preferences.h>
#include <SSLClient.h>

#include "WifiClient.h"
#include "GsmClient.h"

// ---------------------------------------------------------------------------
// Test switches (incident write-up, section 7.3). Keep commented out in
// production builds.
// ---------------------------------------------------------------------------
// Reproduce the bug: validate certificates against 2026-09-01, before the
// broker certificate's 2026-09-08 "valid from". Connecting must fail.
// #define CLOCK_TEST_FORCE_UNIX 1788220800UL
//
// Exercise the fallback: point NTP at a host that cannot resolve and skip the
// modem clock, so the saved time from an earlier boot is used.
// #define CLOCK_TEST_BOGUS_NTP

#if defined(CLOCK_TEST_FORCE_UNIX) || defined(CLOCK_TEST_BOGUS_NTP)
#warning "Clock.h test switch enabled: do NOT flash this build to machines in the field"
#endif

enum class ClockSource : uint8_t {
  COMPILE_DATE,  // nothing better known
  SAVED,         // last good time from NVS (lower bound)
  MODEM_CLOCK,   // SIM800 AT+CCLK? without a successful NTP sync this boot
  NTP,           // NTP sync succeeded this boot (SIM800 AT+CNTP or ESP32 SNTP)
};

#ifdef CLOCK_TEST_BOGUS_NTP
static const char* const CLOCK_NTP_SERVERS[] = { "ntp.invalid" };
#else
static const char* const CLOCK_NTP_SERVERS[] = { "pool.ntp.org", "time.google.com", "time.aws.com" };
#endif
static constexpr size_t CLOCK_NTP_SERVER_COUNT = sizeof(CLOCK_NTP_SERVERS) / sizeof(CLOCK_NTP_SERVERS[0]);

static const char* clockNtpServer(size_t i) {
  return i < CLOCK_NTP_SERVER_COUNT ? CLOCK_NTP_SERVERS[i] : nullptr;
}

// Times more than this far past the compile date are treated as garbage
// (e.g. a corrupted modem clock), so they are never used or saved.
static constexpr uint32_t CLOCK_MAX_AHEAD_OF_BUILD_S = 20UL * 366UL * 86400UL;      // ~20 years
static constexpr unsigned long CLOCK_SAVE_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL;  // 6 h
static constexpr unsigned long CLOCK_RESYNC_COOLDOWN_MS = 60000UL;
#ifdef USE_WIFI
// lwIP SNTP waits SNTP_RECV_TIMEOUT (15 s) for a server before trying the
// next one, after a random start-up delay of up to 5 s. Waiting 22 s at boot
// lets a dead first server fail over to the second. Anything slower is picked
// up later by clockAdoptBackgroundSync().
static constexpr unsigned long CLOCK_WIFI_SNTP_BOOT_WAIT_MS = 22000UL;
static constexpr unsigned long CLOCK_WIFI_SNTP_RESYNC_WAIT_MS = 5000UL;
static bool clockSntpStarted = false;
#endif

static ClockSource clockSource = ClockSource::COMPILE_DATE;
static bool clockFallbackReady = false;
static uint32_t clockFallbackUnix = 0;          // fallback time at clockFallbackSetAtMs
static unsigned long clockFallbackSetAtMs = 0;
static unsigned long clockLastSavedAtMs = 0;
static bool clockResyncAttempted = false;
static unsigned long clockLastResyncAtMs = 0;

Preferences clockNvs;
static bool clockNvsOpen = false;

// ---------------------------------------------------------------------------
// Date helpers (UTC only; no dependence on the TZ environment)
// ---------------------------------------------------------------------------

// Days since 1970-01-01 for a proleptic Gregorian date (H. Hinnant's days_from_civil).
static int32_t clockDaysFromCivil(int32_t y, int32_t m, int32_t d) {
  y -= m <= 2;
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const int32_t yoe = y - era * 400;
  const int32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

static int64_t clockUnixFromCivil(int y, int mo, int d, int h, int mi, int s) {
  return (int64_t)clockDaysFromCivil(y, mo, d) * 86400LL + h * 3600LL + mi * 60LL + s;
}

// Compile date at 00:00 UTC, parsed from __DATE__ ("Nov 22 2026").
static uint32_t clockCompileUnix() {
  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char* date = __DATE__;
  char mon[4] = { date[0], date[1], date[2], '\0' };
  const char* p = strstr(months, mon);
  const int month = p ? (int)(p - months) / 3 + 1 : 1;
  const int day = atoi(date + 4);
  const int year = atoi(date + 7);
  return (uint32_t)clockUnixFromCivil(year, month, day, 0, 0, 0);
}

// A time is believable if it is not before this firmware was built (one day
// of slack for the build machine's timezone) and not absurdly far ahead.
static bool clockIsPlausible(int64_t unixTime) {
  const int64_t built = (int64_t)clockCompileUnix();
  return unixTime >= built - 86400LL && unixTime < built + (int64_t)CLOCK_MAX_AHEAD_OF_BUILD_S;
}

static void clockFormat(uint32_t unixTime, char* out, size_t outLen) {
  time_t t = (time_t)unixTime;
  struct tm tmUtc;
  gmtime_r(&t, &tmUtc);
  // snprintf with integers only (no strftime, no float formatting).
  snprintf(out, outLen, "%04d-%02d-%02d %02d:%02d:%02d UTC",
           tmUtc.tm_year + 1900, tmUtc.tm_mon + 1, tmUtc.tm_mday,
           tmUtc.tm_hour, tmUtc.tm_min, tmUtc.tm_sec);
}

static const char* clockSourceName(ClockSource src) {
  switch (src) {
    case ClockSource::NTP: return "NTP";
    case ClockSource::MODEM_CLOCK: return "modem clock (AT+CCLK)";
    case ClockSource::SAVED: return "saved time (NVS)";
    default: return "compile date";
  }
}

// ---------------------------------------------------------------------------
// NVS
// ---------------------------------------------------------------------------

// Retried on every use until it succeeds: a failed begin() (e.g. a transient
// allocation failure inside nvs_open) must not disable saving for the boot.
static void clockOpenNvs() {
  if (!clockNvsOpen) {
    clockNvsOpen = clockNvs.begin("clock", false);
    if (!clockNvsOpen) Serial.println("[clock] NVS open failed; will retry");
  }
}

static uint32_t clockLoadSaved() {
  clockOpenNvs();
  if (!clockNvsOpen) return 0;
  return clockNvs.getUInt("unix", 0);
}

// Non-authoritative times (fallback + uptime, modem clock) only ever move the
// saved time forward, so it stays a lower bound. An NTP time is authoritative
// and always overwrites it, which also repairs a bad value saved earlier.
static void clockSave(uint32_t unixTime, bool authoritative) {
#ifdef CLOCK_TEST_FORCE_UNIX
  (void)unixTime;
  (void)authoritative;
  return;
#else
  if (!clockIsPlausible((int64_t)unixTime)) return;
  clockLastSavedAtMs = millis();  // a failure is retried at the next interval, not every loop
  clockOpenNvs();
  if (!clockNvsOpen) return;
  const uint32_t stored = clockNvs.getUInt("unix", 0);
  if (authoritative ? unixTime != stored : unixTime > stored) {
    if (clockNvs.putUInt("unix", unixTime) == 0) Serial.println("[clock] NVS save failed");
  }
#endif
}

// ---------------------------------------------------------------------------
// Current time
// ---------------------------------------------------------------------------

// Best known UTC time, seconds since 1970-01-01.
uint32_t clockNowUnix() {
#ifdef CLOCK_TEST_FORCE_UNIX
  return (uint32_t)(CLOCK_TEST_FORCE_UNIX);
#else
  if (clockSource == ClockSource::NTP || clockSource == ClockSource::MODEM_CLOCK) {
    return (uint32_t)time(nullptr);
  }
  if (!clockFallbackReady) return clockCompileUnix();
  return clockFallbackUnix + (millis() - clockFallbackSetAtMs) / 1000UL;
#endif
}

static void clockSetFromNetwork(uint32_t unixTime, ClockSource src) {
  struct timeval tv = { (time_t)unixTime, 0 };
  settimeofday(&tv, nullptr);
  clockSource = src;
  clockSave(unixTime, src == ClockSource::NTP);
}

// ---------------------------------------------------------------------------
// Network time sources
// ---------------------------------------------------------------------------

#ifdef USE_WIFI

// If background SNTP has set the system time since it was started, switch to
// it. Only SNTP sets the system time once it is running (it was cleared just
// before), so a plausible value here is an SNTP answer, not a stale clock.
// Returns true if the clock is on NTP time.
static bool clockAdoptBackgroundSync() {
  if (clockSource == ClockSource::NTP) return true;
  if (!clockSntpStarted) return false;
  const time_t now = time(nullptr);
  if (!clockIsPlausible((int64_t)now)) return false;
  Serial.println("[clock] Wi-Fi SNTP answered in the background; switching to NTP time");
  clockSetFromNetwork((uint32_t)now, ClockSource::NTP);
  return true;
}

// Starts SNTP once per boot and never restarts it: configTime() stops and
// re-inits SNTP, which would restart from the first server every time and
// defeat lwIP's failover to the other servers. SNTP keeps retrying in the
// background after this returns.
static bool clockSyncFromNetworkFor(unsigned long waitMs) {
  if (!clockSntpStarted) {
    // The ESP32 keeps system time across a soft reset, so clear it first and
    // treat only a freshly set, plausible value as an SNTP answer.
    struct timeval zero = { 0, 0 };
    settimeofday(&zero, nullptr);
    configTime(0, 0, clockNtpServer(0), clockNtpServer(1), clockNtpServer(2));
    clockSntpStarted = true;
  }

  const unsigned long start = millis();
  while (millis() - start < waitMs) {
    if (clockAdoptBackgroundSync()) {
      Serial.printf("[clock] Wi-Fi SNTP answered after %lu ms\n", (unsigned long)(millis() - start));
      return true;
    }
    delay(200U);
  }
  Serial.printf("[clock] no SNTP answer in %lu ms; retrying in background\n", waitMs);
  return false;
}

static bool clockSyncFromNetwork() {
  return clockSyncFromNetworkFor(clockSntpStarted ? CLOCK_WIFI_SNTP_RESYNC_WAIT_MS : CLOCK_WIFI_SNTP_BOOT_WAIT_MS);
}

#else  // GSM

// Reads AT+CCLK? and converts it to UTC using the timezone the modem reports.
static bool clockReadModemUtc(uint32_t& outUnix) {
  int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
  float tz = 0.0f;
  if (!modem.getNetworkTime(&y, &mo, &d, &h, &mi, &s, &tz)) {
    Serial.println("[clock] AT+CCLK? failed");
    return false;
  }
  if (mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 || s > 60) {
    Serial.println("[clock] AT+CCLK? returned an invalid date");
    return false;
  }
  // The modem reports the timezone in quarter hours (TinyGSM divides by 4).
  // Work in integers: printing a float would make newlib keep a permanent
  // dtoa buffer on the heap, and production firmware prints no floats.
  const int32_t tzQuarters = (int32_t)(tz * 4.0f + (tz >= 0.0f ? 0.5f : -0.5f));
  const int64_t utc = clockUnixFromCivil(y, mo, d, h, mi, s) - (int64_t)tzQuarters * 900LL;
  Serial.printf("[clock] modem: %04d-%02d-%02d %02d:%02d:%02d tz=%dq\n", y, mo, d, h, mi, s, (int)tzQuarters);
  if (!clockIsPlausible(utc)) {
    Serial.println("[clock] modem clock is not plausible (not synced?)");
    return false;
  }
  outUnix = (uint32_t)utc;
  return true;
}

// Needs the GPRS bearer (AT+SAPBR profile 1) up, which tryGsmConnect() does.
static bool clockSyncFromNetwork() {
  uint32_t unixTime = 0;

  for (size_t i = 0; i < CLOCK_NTP_SERVER_COUNT; i++) {
    // TimeZone 0 -> the modem clock is set to UTC.
    const byte code = modem.NTPServerSync(clockNtpServer(i), 0);
    Serial.printf("[clock] SIM800 NTP %s -> code %u\n", clockNtpServer(i), (unsigned)code);
    if (code == 1 && clockReadModemUtc(unixTime)) {
      clockSetFromNetwork(unixTime, ClockSource::NTP);
      return true;
    }
  }

#ifndef CLOCK_TEST_BOGUS_NTP
  // No NTP answer (or this SIM800 firmware lacks AT+CNTP). The modem clock may
  // still hold network-broadcast time (AT+CLTS=1, set by TinyGSM on restart).
  // It is not authoritative: if it is behind the saved time (a known lower
  // bound), it is wrong, so leave the choice to the fallback.
  if (clockReadModemUtc(unixTime)) {
    const uint32_t saved = clockLoadSaved();
    if (unixTime < saved) {
      Serial.printf("[clock] modem < saved (%lu<%lu), ignored\n",
                    (unsigned long)unixTime, (unsigned long)saved);
      return false;
    }
    clockSetFromNetwork(unixTime, ClockSource::MODEM_CLOCK);
    return true;
  }
#endif
  return false;
}

// The SIM800 has no background sync; time is only fetched on request.
static bool clockAdoptBackgroundSync() {
  return clockSource == ClockSource::NTP;
}

#endif  // USE_WIFI

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Log lines are kept under 64 characters per printf: Arduino's printf
// malloc()s a temporary buffer for anything longer.
static void clockLogState(const char* when) {
  char buf[32];
  const uint32_t now = clockNowUnix();
  clockFormat(now, buf, sizeof buf);
  Serial.printf("[clock] %s: source=%s\n", when, clockSourceName(clockSource));
  Serial.printf("[clock]   time=%s (%lu)\n", buf, (unsigned long)now);
#ifdef CLOCK_TEST_FORCE_UNIX
  Serial.println("[clock] CLOCK_TEST_FORCE_UNIX is set: TLS uses the forced time");
#endif
}

static void clockUseFallback() {
  // Keep a network time from earlier in this boot (system time keeps running).
  if (clockSource == ClockSource::NTP || clockSource == ClockSource::MODEM_CLOCK) return;

  const uint32_t compiled = clockCompileUnix();
  const uint32_t saved = clockLoadSaved();
  uint32_t best = compiled;
  ClockSource src = ClockSource::COMPILE_DATE;
  if (saved > best && clockIsPlausible((int64_t)saved)) {
    best = saved;
    src = ClockSource::SAVED;
  }
  if (clockFallbackReady) {
    // The fallback already running this boot (saved/compile time + uptime)
    // is still a lower bound and may be later than anything stored.
    const uint32_t running = clockNowUnix();
    if (running >= best) {
      best = running;
      src = clockSource;
    }
  }

  clockFallbackUnix = best;
  clockFallbackSetAtMs = millis();
  clockFallbackReady = true;
  clockSource = src;
}

// Call once the transport (GPRS bearer or Wi-Fi) is up and before
// initializeSSL(). Does nothing if NTP already succeeded this boot.
void clockEnsure() {
  if (clockSource == ClockSource::NTP) {
    clockLogState("already synced");
    return;
  }

  // Open NVS first: its small permanent allocation then sits below the
  // temporary ones made during the sync, instead of leaving holes between
  // them right before initializeSSL() needs one large block.
  clockOpenNvs();

  Serial.printf("[clock] heap before sync: %u\n", (unsigned)ESP.getFreeHeap());
  const unsigned long start = millis();
  const bool ok = clockSyncFromNetwork();
  Serial.printf("[clock] sync %s in %lu ms, heap after: %u\n",
                ok ? "ok" : "FAILED", (unsigned long)(millis() - start), (unsigned)ESP.getFreeHeap());
  if (!ok) clockUseFallback();
  clockLogState("boot");

  // Counts as a sync attempt, so a TLS failure right after a failed sync
  // waits for the cooldown instead of repeating it immediately.
  clockResyncAttempted = true;
  clockLastResyncAtMs = millis();
}

// After a TLS/MQTT failure that happened once TCP was up: re-fetch the time
// if we are not on NTP time, at most once per CLOCK_RESYNC_COOLDOWN_MS.
bool clockResyncDue() {
  if (clockAdoptBackgroundSync()) return false;
  return !clockResyncAttempted || millis() - clockLastResyncAtMs >= CLOCK_RESYNC_COOLDOWN_MS;
}

// Returns true only if the time improved: NTP was reached, or the source
// changed (e.g. saved time -> modem clock). Re-reading the same modem clock is
// not an improvement, so the caller does not wait on it again.
bool clockResync() {
  clockResyncAttempted = true;
  clockLastResyncAtMs = millis();
  Serial.println("[clock] TLS failed on non-NTP time; re-fetching");
  const ClockSource before = clockSource;
  const bool ok = clockSyncFromNetwork();
  if (!ok) clockUseFallback();
  const bool improved = ok && (clockSource == ClockSource::NTP || clockSource != before);
  clockLogState(improved ? "resync" : "resync, no better time");
  return improved;
}

// Call from loop(): keeps the saved fallback time fresh (every 6 h) and
// re-bases the uptime-derived fallback before millis() can wrap.
void clockMaintain() {
  clockAdoptBackgroundSync();
  if (millis() - clockLastSavedAtMs < CLOCK_SAVE_INTERVAL_MS) return;
  const uint32_t now = clockNowUnix();
  if (clockFallbackReady && clockSource != ClockSource::NTP && clockSource != ClockSource::MODEM_CLOCK) {
    clockFallbackUnix = now;
    clockFallbackSetAtMs = millis();
  }
  clockSave(now, false);
  clockLastSavedAtMs = millis();  // also when clockSave() skips the write
}

// Sets SSLClient's certificate validation time. BearSSL keeps it in the x509
// context, which SSLClient only initialises in its constructor, so it survives
// stop()/reconnect; it is reapplied on every create/connect anyway.
void applyVerificationTime(SSLClient& client) {
  clockAdoptBackgroundSync();
  const uint32_t now = clockNowUnix();
  client.setVerificationTime(now / 86400UL + 719528UL, now % 86400UL);
}
