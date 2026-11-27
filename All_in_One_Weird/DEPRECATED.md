# All_in_One_Weird is deprecated

Don't build or flash this firmware. It stops with an error on purpose.

The two-group 160 machine (BELT group on the top button, PANTY group on the
bottom button, e.g. BBKD01) now runs the same firmware as every other
machine: **`All_in_One`**, with this in `All_in_One/config.h`:

```cpp
#define MACHINE_TYPE TYPE_TWO_GROUP_160
#define MACHINE_SIZE 160
#define HAS_RFID 1
#define HAS_CASH 0
```

Everything this firmware did is in `All_in_One` (buttons, two-motor
columns, per-group stock, its screens), plus the move to our MQTT broker,
the display memory fix and the other fixes since. The folder is kept only
for reference; it still connects to AWS IoT, which no longer works.
