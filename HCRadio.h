#ifndef HCRADIO_H_
#define HCRADIO_H_


// TODO: Change return value of HCRadioResult::get_json() into char* to remove this!

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

// Number of maximum High/Low changes per packet.
// We can handle up to (unsigned long) => 32 bit * 2 H/L changes per bit + 2 for sync
#define HCRADIO_MAX_CHANGES 67

class HCRadioResult
{
	public:

		bool ready;
		unsigned long pulse_length;
		unsigned long timings[HCRADIO_MAX_CHANGES];
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
		void send_0();
		void send_1();
		void send_f();
		void sync();
		bool send_tristate(char* code);

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
