#include "buzzer.h"

// ---- pattern table: segments of (on ms, off ms); off==0 = last segment
struct Pat {
  uint8_t  n;                 // segment count
  uint16_t on[6];
  uint16_t off[6];
  uint8_t  prio;              // higher preempts
};

static const Pat P[] = {
  /*NONE*/        {0, {0,0,0,0,0,0}, {0,0,0,0,0,0}, 0},
  /*KEY*/         {1, {50,0,0,0,0,0},          {0,0,0,0,0,0},            1}, // #1
  /*INVALID*/     {2, {80,80,0,0,0,0},         {100,0,0,0,0,0},          2}, // #2
  /*TICK*/        {1, {50,0,0,0,0,0},          {0,0,0,0,0,0},            4}, // #3
  /*MODE*/        {1, {100,0,0,0,0,0},         {0,0,0,0,0,0},            2}, // #4
  /*SAVED*/       {2, {80,80,0,0,0,0},         {80,0,0,0,0,0},           3}, // #5
  /*BACK*/        {1, {100,0,0,0,0,0},         {0,0,0,0,0,0},            1}, // #6
  /*DOOR_AJAR*/   {3, {150,150,150,0,0,0},     {100,100,0,0,0,0},        7}, // #7 /2s
  /*NO_TRAYS*/    {3, {100,100,300,0,0,0},     {150,150,0,0,0,0},        6}, // #8 /3s
  /*UNSTABLE*/    {2, {100,100,0,0,0,0},       {150,0,0,0,0,0},          5}, // #9 /3s
  /*READY*/       {1, {500,0,0,0,0,0},         {0,0,0,0,0,0},            3}, // #11
  /*BATT_LOW*/    {3, {150,150,150,0,0,0},     {100,100,0,0,0,0},        5}, // #12
  /*BATT_CRIT*/   {4, {100,100,100,100,0,0},   {100,100,100,0,0,0},      8}, // #13 /30s
  /*SETPOINT*/    {2, {100,100,0,0,0,0},       {100,0,0,0,0,0},          3}, // #14
  /*FAN_ON*/      {1, {80,0,0,0,0,0},          {0,0,0,0,0,0},            2}, // #15
  /*DOOR_OPEN_RUN*/{1,{100,0,0,0,0,0},         {0,0,0,0,0,0},            9}, // #16 /150ms
  /*MIDWAY*/      {2, {100,100,0,0,0,0},       {150,0,0,0,0,0},          3}, // #17
  /*APPROACH*/    {2, {80,80,0,0,0,0},         {100,0,0,0,0,0},          4}, // #18 /60s
  /*TIMEOUT5*/    {3, {100,100,100,0,0,0},     {100,100,0,0,0,0},        5}, // #19 /30s
  /*WARN*/        {2, {150,150,0,0,0,0},       {150,0,0,0,0,0},          6}, // #23 /15s
  /*CRITICAL*/    {1, {5000,0,0,0,0,0},        {0,0,0,0,0,0},           10}, // #24a
  /*CRITICAL_R*/  {2, {250,250,0,0,0,0},       {250,0,0,0,0,0},          6}, // #24b /10s
  /*RECOVERED*/   {2, {50,50,0,0,0,0},         {60,0,0,0,0,0},           3}, // #25
  /*E05_LOAD*/    {1, {2000,0,0,0,0,0},        {0,0,0,0,0,0},            8}, // #26
  /*AP_UP*/       {1, {80,0,0,0,0,0},          {0,0,0,0,0,0},            2}, // #27
  /*CLIENT*/      {1, {120,0,0,0,0,0},         {0,0,0,0,0,0},            2}, // #28
  /*LOG_SAVED*/   {2, {80,80,0,0,0,0},         {80,0,0,0,0,0},           3}, // #29
  /*STORE_FULL*/  {3, {100,100,100,0,0,0},     {100,100,0,0,0,0},        4}, // #30
  /*BROWNOUT_RET*/{3, {120,120,120,0,0,0},     {120,120,0,0,0,0},        4}, // #21
  /*ANOMALY*/     {3, {80,80,80,0,0,0},        {80,80,0,0,0,0},          5}, // #22
  /*MAINT*/       {3, {120,120,120,0,0,0},     {150,150,0,0,0,0},        4}, // #33
  /*INIT_FAIL*/   {1, {2000,0,0,0,0,0},        {0,0,0,0,0,0},            8}, // #34
  /*COOL_DONE*/   {1, {200,0,0,0,0,0},         {0,0,0,0,0,0},            2}, // #35
  /*SHUTDOWN*/    {1, {1000,0,0,0,0,0},        {0,0,0,0,0,0},            9}, // #36
  /*FACT_RESET*/  {4, {1500,100,100,100,0,0},  {200,100,100,0,0,0},      9}, // #37
  /*POWER_ON*/    {6, {250,250,250,250,250,250},{250,250,250,250,250,0}, 4},
  /*CYCLE_START*/ {1, {3000,0,0,0,0,0},        {0,0,0,0,0,0},            9},
  /*CYCLE_DONE*/  {1, {5000,0,0,0,0,0},        {0,0,0,0,0,0},            9},
  /*ERROR*/       {1, {5000,0,0,0,0,0},        {0,0,0,0,0,0},           10},
  /*DOOR*/        {1, {1000,0,0,0,0,0},        {0,0,0,0,0,0},            5},
  /*MODE_CHANGE*/ {1, {3000,0,0,0,0,0},        {0,0,0,0,0,0},            8},
};

#if BUZZER_ENABLED
#include "config.h"

Buzzer buzzer;
struct Pat {
  uint8_t  n;                 // segment count
  uint16_t on[6];
  uint16_t off[6];
  uint8_t  prio;              // higher preempts
};

static const Pat P[] = {
  /*NONE*/        {0, {0,0,0,0,0,0}, {0,0,0,0,0,0}, 0},
  /*KEY*/         {1, {50,0,0,0,0,0},          {0,0,0,0,0,0},            1}, // #1
  /*INVALID*/     {2, {80,80,0,0,0,0},         {100,0,0,0,0,0},          2}, // #2
  /*TICK*/        {1, {50,0,0,0,0,0},          {0,0,0,0,0,0},            4}, // #3
  /*MODE*/        {1, {100,0,0,0,0,0},         {0,0,0,0,0,0},            2}, // #4

void Buzzer::beep(uint8_t n, uint16_t onMs, uint16_t offMs) {
  if (n == 0) return;
  _p = BP::NONE;                     // legacy beep owns the driver
  _bTot = n; _bDone = 0;
  _bOnMs = onMs; _bOffMs = offMs;
  _bOn = true;
  _tEdge = millis();
  drive(true);
}

void Buzzer::playPat(BP p) {
  const Pat &pat = P[(uint8_t)p];
  if (pat.n == 0) return;
  _bTot = 0;                         // pattern preempts any legacy beep
  if (_p != BP::NONE && P[(uint8_t)_p].prio > pat.prio) return; // quieter wins
  _p = p; _seg = 0; _on = true; _tEdge = millis();
  drive(true);
}

void Buzzer::addRepeat(BP p, uint32_t periodMs) {
  for (auto &r : _r)
    if (r.p == p) { r.period = periodMs; return; }        // already on
  for (auto &r : _r)
    if (r.p == BP::NONE) { r.p = p; r.period = periodMs; r.last = millis(); return; }
  uint8_t low = 0;                                        // table full:
  for (uint8_t i = 1; i < 4; i++)                         // drop the quietest
    if (P[(uint8_t)_r[i].p].prio < P[(uint8_t)_r[low].p].prio) low = i;
  _r[low] = {p, periodMs, millis()};
}

void Buzzer::delRepeat(BP p) {
  for (auto &r : _r) if (r.p == p) r.p = BP::NONE;
}

void Buzzer::stopAll() {
  for (auto &r : _r) r.p = BP::NONE;
}

bool Buzzer::repeating(BP p) {
  for (auto &r : _r) if (r.p == p) return true;
  return false;
}

void Buzzer::pump() {
  if (_p != BP::NONE) return;                             // busy
  R *best = nullptr;
  uint32_t now = millis();
  for (auto &r : _r) {
    if (r.p == BP::NONE || now - r.last < r.period) continue;
    if (!best || P[(uint8_t)r.p].prio > P[(uint8_t)best->p].prio) best = &r;
  }
  if (best) { best->last = now; playPat(best->p); }
}

void Buzzer::update() {
  if (_bTot) {                                  // legacy beep() cadence
    uint32_t nowB = millis();
    uint16_t phase = _bOn ? _bOnMs : _bOffMs;
    if (nowB - _tEdge >= phase) {
      if (_bOn) {                               // on-phase over
        drive(false); _bOn = false; _tEdge = nowB;
        if (++_bDone >= _bTot) _bTot = 0;       // cadence finished
      } else {                                  // gap over -> next beep
        _bOn = true; _tEdge = nowB; drive(true);
      }
    }
    return;
  }
  pump();
  if (_p == BP::NONE) return;
  const Pat &pat = P[(uint8_t)_p];
  uint32_t now = millis();
  uint16_t phase = _on ? pat.on[_seg] : pat.off[_seg];
  if (now - _tEdge >= phase) {
    if (_on) {
      drive(false); _on = false; _tEdge = now;
      if (pat.off[_seg] == 0) _p = BP::NONE;   // last segment ends pattern
    } else {
      if (_seg + 1 < pat.n) { _seg++; _on = true; _tEdge = now; drive(true); }
      else _p = BP::NONE;
    }
  }
}

#else
Buzzer buzzer;                                  // stub class from header
#endif

// ---- public API --------------------------------------------------------
namespace bz {
  void play(BP p)                { buzzer.playPat(p); }
  void startRepeat(BP p, uint32_t periodMs) { buzzer.addRepeat(p, periodMs); }
  void stopRepeat(BP p)          { buzzer.delRepeat(p); }
  void stopAllRepeats()          { buzzer.stopAll(); }
  bool repeating(BP p)           { return buzzer.repeating(p); }
  // classic law
  void powerOn()    { play(BP::POWER_ON); }
  void cycleStart() { play(BP::CYCLE_START); }
  void cycleDone()  { play(BP::CYCLE_DONE); }
  void error()      { play(BP::ERROR); }
  void door()       { play(BP::DOOR); }
  void modeChange() { play(BP::MODE_CHANGE); }
}
  /*COOL_DONE*/   {1, {200,0,0,0,0,0},         {0,0,0,0,0,0},            2}, // #35
  /*SHUTDOWN*/    {1, {1000,0,0,0,0,0},        {0,0,0,0,0,0},            9}, // #36
  /*FACT_RESET*/  {4, {1500,100,100,100,0,0},  {200,100,100,0,0,0},      9}, // #37
  /*POWER_ON*/    {6, {250,250,250,250,250,250},{250,250,250,250,250,0}, 4},
  /*CYCLE_START*/ {1, {3000,0,0,0,0,0},        {0,0,0,0,0,0},            9},
  /*CYCLE_DONE*/  {1, {5000,0,0,0,0,0},        {0,0,0,0,0,0},            9},
  /*ERROR*/       {1, {5000,0,0,0,0,0},        {0,0,0,0,0,0},           10},
  /*DOOR*/        {1, {1000,0,0,0,0,0},        {0,0,0,0,0,0},            5},
  /*MODE_CHANGE*/ {1, {3000,0,0,0,0,0},        {0,0,0,0,0,0},            8},
};

#if BUZZER_ENABLED
#include "config.h"

Buzzer buzzer;

void Buzzer::drive(bool on) {
#if BUZZER_ACTIVE_HIGH
  digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
#else
  digitalWrite(PIN_BUZZER, on ? LOW : HIGH);
#endif
}

void Buzzer::begin() {
  pinMode(PIN_BUZZER, OUTPUT);
  drive(false);
}

void Buzzer::playPat(BP p) {
  const Pat &pat = P[(uint8_t)p];
  if (pat.n == 0) return;
  if (_p != BP::NONE && P[(uint8_t)_p].prio > pat.prio) return; // quieter wins
  _p = p; _seg = 0; _on = true; _tEdge = millis();
  drive(true);
}

void Buzzer::addRepeat(BP p, uint32_t periodMs) {
  for (auto &r : _r)
    if (r.p == p) { r.period = periodMs; return; }        // already on
  for (auto &r : _r)
    if (r.p == BP::NONE) { r.p = p; r.period = periodMs; r.last = millis(); return; }
  uint8_t low = 0;                                        // table full:
  for (uint8_t i = 1; i < 4; i++)                         // drop the quietest
    if (P[(uint8_t)_r[i].p].prio < P[(uint8_t)_r[low].p].prio) low = i;
  _r[low] = {p, periodMs, millis()};
}

void Buzzer::delRepeat(BP p) {
  for (auto &r : _r) if (r.p == p) r.p = BP::NONE;
}

void Buzzer::stopAll() {
  for (auto &r : _r) r.p = BP::NONE;
}

bool Buzzer::repeating(BP p) {
  for (auto &r : _r) if (r.p == p) return true;
  return false;
}

void Buzzer::pump() {
  if (_p != BP::NONE) return;                             // busy
  R *best = nullptr;
  uint32_t now = millis();
  for (auto &r : _r) {
    if (r.p == BP::NONE || now - r.last < r.period) continue;
    if (!best || P[(uint8_t)r.p].prio > P[(uint8_t)best->p].prio) best = &r;
  }
  if (best) { best->last = now; playPat(best->p); }
}

void Buzzer::update() {
  pump();
  if (_p == BP::NONE) return;
  const Pat &pat = P[(uint8_t)_p];
  uint32_t now = millis();
  uint16_t phase = _on ? pat.on[_seg] : pat.off[_seg];
  if (now - _tEdge >= phase) {
    if (_on) {
      drive(false); _on = false; _tEdge = now;
      if (pat.off[_seg] == 0) _p = BP::NONE;   // last segment ends pattern
    } else {
      if (_seg + 1 < pat.n) { _seg++; _on = true; _tEdge = now; drive(true); }
      else _p = BP::NONE;
    }
  }
}

#else
Buzzer buzzer;                                  // stub class from header
#endif

// ---- public API --------------------------------------------------------
namespace bz {
  void play(BP p)                { buzzer.playPat(p); }
  void startRepeat(BP p, uint32_t periodMs) { buzzer.addRepeat(p, periodMs); }
  void stopRepeat(BP p)          { buzzer.delRepeat(p); }
  void stopAllRepeats()          { buzzer.stopAll(); }
  bool repeating(BP p)           { return buzzer.repeating(p); }
  // classic law
  void powerOn()    { play(BP::POWER_ON); }
  void cycleStart() { play(BP::CYCLE_START); }
  void cycleDone()  { play(BP::CYCLE_DONE); }
  void error()      { play(BP::ERROR); }
  void door()       { play(BP::DOOR); }
  void modeChange() { play(BP::MODE_CHANGE); }
}