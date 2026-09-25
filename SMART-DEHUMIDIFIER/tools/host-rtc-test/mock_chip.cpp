#include "Arduino.h"
#include <time.h>
int gPin[64];
MockP P;
FakeChip gChip = {{0}, {0}, 1};
// controllable system clock (settimeofday needs root in a container)
static time_t gFakeNow = 0;
void rtc_test_set_now(time_t t) { gFakeNow = t; }
time_t time(time_t *t) { if (t) *t = gFakeNow; return gFakeNow; }
