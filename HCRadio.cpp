#include <HCRadio.h>

volatile HCRadioResult hcradio_result;

HCRadio::HCRadio()
:status_pin(-1), irq(-1), pulse_length(-1), send_pin(-1), send_repeat(-1)
{
	hcradio_result.ready = false;
	hcradio_result.pulse_length = 0;
	hcradio_result.len_timings = 0;
	hcradio_result.last_time = 0;
	hcradio_result.repeat_count = 0;
	hcradio_result.change_count = 0;
}

void HCRadio::enable_receive(int irq)
{
	this->irq = irq;
	attachInterrupt(this->irq, HCRadio::receive_interrupt, CHANGE);
}

void HCRadio::disable_receive()
{
	detachInterrupt(this->irq);
	this->irq = -1;
}

void HCRadio::enable_send(int pin, int repeat, int pulse_length)
{
	this->send_repeat = repeat;
	this->send_pin = pin;
	this->pulse_length = pulse_length;
}

void HCRadio::disable_send()
{
	this->send_repeat = -1;
	this->send_pin = -1;
	this->pulse_length = -1;
}

void HCRadio::enable_status(int pin)
{
	this->status_pin = pin;
	pinMode(pin, OUTPUT);
}

void HCRadio::disable_status()
{
	status_pin = -1;
}

void HCRadio::set_pulse_length(int pulse_length)
{
	this->pulse_length = pulse_length;
}

void HCRadio::receive_interrupt()
{
	unsigned long time = micros();
	unsigned long duration = time - hcradio_result.last_time;

	if(duration > 5000 &&
	   duration > hcradio_result.timings[0] - 200 &&
	   duration < hcradio_result.timings[0] + 200)
	{
		hcradio_result.repeat_count ++;
		hcradio_result.change_count --; // ??

		if(hcradio_result.repeat_count == 2)
		{
			hcradio_result.ready = true;
			hcradio_result.len_timings = hcradio_result.change_count;
			hcradio_result.repeat_count = 0;
		}

		hcradio_result.change_count = 0;
	}
	else if(duration > 5000)
	{
		hcradio_result.change_count = 0;
	}

	if(hcradio_result.change_count >= HCRADIO_MAX_CHANGES)
	{
		hcradio_result.change_count = 0;
		hcradio_result.repeat_count = 0;
	}

	hcradio_result.timings[hcradio_result.change_count ++] = duration;
	hcradio_result.last_time = time;
}

void HCRadio::send_0()
{
    this->transmit(1,3);
    this->transmit(1,3);
}

void HCRadio::send_1()
{
	this->transmit(3,1);
	this->transmit(3,1);
}

void HCRadio::send_f()
{
	this->transmit(1,3);
	this->transmit(3,1);
}

void HCRadio::sync()
{
	this->transmit(1,31);
}

void HCRadio::transmit(int high, int low)
{
	digitalWrite(send_pin, HIGH);
	delayMicroseconds(pulse_length * high);
	digitalWrite(send_pin, LOW);
	delayMicroseconds(pulse_length * low);
}

bool HCRadio::send_tristate(char* code)
{
	if(status_pin != -1)
		digitalWrite(status_pin, HIGH);

	bool valid_chars = true;

    char* c = code;
    while (*c)
    {
        if (*c != '1' && *c != '0' && *c != 'f' && *c != 'F')
        {
            valid_chars = false;
            break;
        }
        c += 1;
    }

    if (valid_chars)
    {
    	noInterrupts();
    	cli();

    	this->sync();
    	for(int i = 0; i < this->send_repeat; i ++)
    	{
    		c = code;
			while (*c)
			{
				if (*c == '0') this->send_0();
				else if (*c == '1') this->send_1();
				else this->send_f();

				c += 1;
			}

			this->sync();
    	}

    	interrupts();
    	sei();
    }

	if(status_pin != -1)
		digitalWrite(status_pin, LOW);

	return valid_chars;
}

bool HCRadio::decode(HCRadioResult* result)
{
	noInterrupts();
	if(hcradio_result.ready == false)
	{
		interrupts();
		return false;
	}

	result->len_timings = hcradio_result.len_timings;
	result->last_time = hcradio_result.last_time;

	for(unsigned int i = 0; i < result->len_timings; i ++)
		result->timings[i] = hcradio_result.timings[i];

	hcradio_result.ready = false;
	interrupts();

	if(result->len_timings > 0)
		result->pulse_length = result->timings[0] / 31;

	return true;
}
