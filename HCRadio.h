#ifndef HCRADIO_H_
#define HCRADIO_H_

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

// Number of maximum High/Low changes per packet.
// We can handle up to (unsigned long) => 32 bit * 2 H/L changes per bit + 2 for sync
#define HCRADIO_MAX_TIMINGS 67

class HCRadioResult
{
    public:

        bool ready;
        unsigned int pulse_length;
        unsigned int timings[HCRADIO_MAX_TIMINGS];
        unsigned int len_timings;
        unsigned long last_time;
        unsigned int repeat_count;
        unsigned int change_count;
};

class HCRadio
{
    public:

        HCRadio();

        void enable_status(int pin);
        void disable_status();
        void enable_receive(int irq);
        void disable_receive();
        void enable_send(int pin, int repeat = 10, int pulse_length = 380);
        void disable_send();
        void set_pulse_length(int pulse_length);
        void set_send_repeat(int repeat);
        void send_0();
        void send_1();
        void send_f();
        void sync();
        bool send_tristate(char* code);
        bool send_raw(unsigned long* timings, unsigned int len_timings);
        bool send_raw(const char* c);

        bool decode(HCRadioResult* result);

    private:

        int status_pin;
        int irq;
        int pulse_length;
        int send_pin;
        int send_repeat;

        void transmit(int high, int low);
        static void receive_interrupt();
};

#endif
