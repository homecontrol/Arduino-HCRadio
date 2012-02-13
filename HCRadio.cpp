#include <HCRadio.h>

volatile HCRadioResult hcradio_result;

HCRadio::HCRadio()
:status_pin(-1), irq(-1), pulse_length(-1), send_pin(-1), send_repeat(-1)
{
	hcradio_result.ready = false;
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
	volatile static unsigned int change_count = 0;
	volatile static unsigned int timings[HCRADIO_MAX_CHANGES];
	volatile static unsigned long last_time = micros();
	volatile static unsigned int repeat_count = 0;
	volatile static bool working = false;

	if(working == true || hcradio_result.ready = false)
		return;

	long time = micros();

	noInterrupts();
	cli();

	unsigned long duration = time - last_time;
	if (duration > 5000 && duration > timings[0] - 200 && duration < timings[0] + 200)
	{
		repeat_count ++;
		change_count --;
		if (repeat_count == 2)
		{
			unsigned long code = 0;
			unsigned long delay = timings[0] / 31;
			unsigned long delay_tolerance = delay * 0.3;
			for (int i = 1; i < change_count; i = i + 2)
			{
				if (timings[i] > delay - delay_tolerance &&
					timings[i] < delay + delay_tolerance &&
					timings[i + 1] > delay * 3 - delay_tolerance &&
					timings[i + 1] < delay * 3 + delay_tolerance)
				{
					code = code << 1; // High
				}
				else if (timings[i] > delay * 3 - delay_tolerance &&
						 timings[i] < delay * + delay_tolerance &&
						 timings[i + 1] > delay - delay_tolerance &&
						 timings[i + 1] < delay + delay_tolerance)
				{
					code += 1;
					code = code << 1; // Low
				}
				else
				{
					// Failed
					i = change_count;
					code = 0;
					repeat_count = 0;
				}
			}
			code = code >> 1;

			hcradio_result.decimal = code;
			hcradio_result.length = change_count / 2;
			hcradio_result.delay = delay;
			hcradio_result.ready = true;
			memcpy(hcradio_result.timings, timings, sizeof(unsigned int) * change_count);

			repeat_count = 0;
		}
		change_count = 0;
	}
	else if (duration > 5000)
	{
		change_count = 0;
	}

	if (change_count >= HCRADIO_MAX_CHANGES)
	{
		change_count = 0;
		repeat_count = 0;
	}

	timings[change_count ++] = duration;
	last_time = time;

	working = false;
	interrupts();
	seil();
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
	if(!hcradio_result.ready)
		return false;

	noInterrupts();
	result->decimal = hcradio_result.decimal;
	result->length = hcradio_result.length;
	result->delay = hcradio_result.delay;
	memcpy(result->timings, hcradio_result.timings, 2 * result->length * sizeof(unsigned int));
	hcradio_result.ready = false;
	interrupts();

	// Create json description of the result
	result->json = "{\"type\": \"rf\", ";
	if(result->decimal == 0) result->json += "\"error\": \"unkown_decoding\"";
	else
	{
		result->json += "\"decimal\": \"" + String(result->decimal) + "\", " +
					    "\"length\": \"" + String(result->length) + "\"";

		if(result->length > 0)
		{
			result->json += ",\"timings\": [\"" + String(result->timings[0], DEC) + "\"";
			for(int i = 1; i < 2 * result->length; i ++)
				result->json += ", \"" + String(result->timings[0], DEC) + "\"";
			result->json += "]";
		}
	}
	result->json += "}";

	return true;
}