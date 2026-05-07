#pragma once
// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  StepperRMT — Profesionalna kontrola koračnog motora via ESP32 RMT     ║
// ║                                                                          ║
// ║  Prednosti nad digitalWrite():                                           ║
// ║  • Pulzevi generirani HARDVERSKI — CPU slobodan za ostale zadatke        ║
// ║  • Precizno timing bez jittera od interrupta / WiFi stack-a              ║
// ║  • Trapezoidalni profil brzine (ubrzanje + konstantna + usporenje)       ║
// ║  • Non-blocking: moveRel/moveTo vraćaju odmah, motor radi u pozadini    ║
// ║  • Thread-safe pozicija (std::atomic)                                    ║
// ║                                                                          ║
// ║  Kompatibilnost: Arduino ESP32 2.x (ESP-IDF 4.4) — driver/rmt.h API    ║
// ╚══════════════════════════════════════════════════════════════════════════╝

#include <Arduino.h>
#include <atomic>
#include "driver/rmt.h"        // IDF 4.4 legacy RMT API
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// ── Konstante RMT periferije ─────────────────────────────────────────────
// APB clock 80 MHz / clk_div 80 = 1 MHz rezolucija → 1 µs/tick
static constexpr uint32_t RMT_CLK_DIV  = 80;    // 1 MHz timer
static constexpr uint32_t RMT_RES_HZ   = 1000000;
static constexpr uint32_t PULSE_TICKS  = 5;      // PUL aktivan = 5 µs (ZDM min 2.5µs)
static constexpr uint32_t CHUNK_CONST  = 62;     // koraci/poziv u konst. fazi (RMT max 64, -2 za terminator)
static constexpr uint32_t CHUNK_RAMP   = 32;     // koraci/poziv tokom rampe — manji = više tačaka S-krive = glatkije

// ── Paket naredbe (ide u FreeRTOS red) ──────────────────────────────────
struct MotorMove {
    long  steps;   // signed: + = naprijed, - = nazad (relativno od _pos)
    float vMax;    // vršna brzina  [Hz = koraci/s]
    float accel;   // linearno ubrzanje [Hz/s]
};

// ── Klasa ────────────────────────────────────────────────────────────────
class StepperRMT {
public:
    // stepsPerRev: koraka po okretaju NAKON microstepping-a
    // Pr: NEMA23 + P001=16 → 16 × 200 = 3200
    // rmtChannel: RMT_CHANNEL_0 do RMT_CHANNEL_7 (default 0)
    StepperRMT(uint8_t pinPUL, uint8_t pinDIR, uint8_t pinENA,
               uint32_t stepsPerRev = 3200,
               rmt_channel_t rmtChannel = RMT_CHANNEL_0);

    // Inicijalizacija: RMT kanal + FreeRTOS task. Pozovi jednom u setup().
    void begin();

    // Enable/Disable: ENA- signal prema drajveru
    void enable();
    void disable();
    bool isEnabled() const { return _enabled.load(); }
    bool isMoving()  const { return _moving.load();  }

    // Non-blocking pomak za relativni broj koraka
    // accel [Hz/s]: npr. 8000 = za 1 sekundu ubrzaj sa 100 Hz na 8100 Hz
    void moveRel(long steps,  float vMax = 3000.f, float accel = 20000.f);

    // Non-blocking pomak na apsolutnu poziciju (u koracima)
    void moveTo (long absPos, float vMax = 3000.f, float accel = 20000.f);

    // Hitni stop: prazni red, čeka završetak trenutnog chunka (~1–5 ms)
    void stop();

    // Blokira pozivatelja dok red nije prazan i motor nije u mirovanju
    void waitDone();

    // Pozicija u koracima (atomska, thread-safe)
    long getPosition() const { return _pos.load(); }
    void setPosition(long p)  { _pos.store(p);     }

private:
    const uint8_t       _pinPUL, _pinDIR, _pinENA;
    const uint32_t      _stepsPerRev;
    const rmt_channel_t _ch;

    std::atomic<long> _pos    {0};
    std::atomic<bool> _enabled{false};
    std::atomic<bool> _moving {false};
    std::atomic<bool> _stop   {false};

    rmt_item32_t  _buf[64];           // RMT hardverski maksimum (1 mem_block = 64 item-a)

    QueueHandle_t _queue   = nullptr;
    TaskHandle_t  _taskHnd = nullptr;

    // Pošalji n koraka na zadanoj periodi; ažurira _pos
    void _sendChunk(uint32_t n, uint32_t periodTicks, int8_t dir);
    // Izvrši jedan MotorMove s trapezoidalnim profilom
    void _execMove(const MotorMove& m);

    static void _taskFn(void* arg);
};
