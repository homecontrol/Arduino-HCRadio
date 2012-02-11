#include <HCRadio.h>

static HCRadioResult hcradio_result;

HCRadioResult::HCRadioResult()
:decimal(-1),length(-1),delay(-1)
{};

void HCRadioResult::clear()
{
	decimal = -1;
	length = -1;
	delay = -1;
}

bool HCRadioResult::is_ready()
{
	return decimal != -1;
}

String HCRadioResult::get_json()
{
	String json = "{";

	json += "\"type\": ";
	json += "\"RF\", ";

	if (decimal == 0)
	{
		json += "\"error\": \"unkown_encoding\"";
		return json;
	}

	json += "\"decimal\": ";
	json += "\"" + String(decimal) + "\", ";

	json += "\"bit_length\": ";
	json += "\"" + String(length) + "\", ";

	char* b = HCRadioResult::dec2bin(decimal, length);

	json += "\"binary\": ";
	json += "\"" + String(decimal) + "\", ";

	char* t = HCRadioResult::bin2tristate(b);

	json += "\"tristate\": ";
	json += "\"" + String(t) + "\", ";

	json += "\"pulse_length\":";
	json += "\"" + String(delay) + "\",";
//
//	if(length > 0)
//	{
////		json += "\"raw\": [\"" + String(raw[0], DEC) + "\"";
////		for (int i = 1; i <= length * 2; i++)
////			json += ",\"" + String(raw[i], DEC) + "\"";
////		json += "]";
//		Serial.print("Raw data: ");
//		for (int i = 0; i < min(HCRADIO_MAX_CHANGES, length * 2); i++)
//		{
//			Serial.print(raw[i]);
//			Serial.print(",");
//		}
//		Serial.println();
//	}

	json += "}";

	return json;
}

char* HCRadioResult::bin2tristate(char* bin)
{
	static const int max_length = 16 + 1; // 32 bits / 2 + newline
	static char tristate[max_length];

	int i = 0, j = 0;

	for(i = 0; i < (max_length - 1); i ++)
	{
		j = 2 * i;

		if(bin[j] == '\0' || bin[j + 1] == '\0')
			break;

		if(bin[j] == '0' && bin[j + 1] == '0')
			tristate[i] = '0';

		else if(bin[j] == '1' && bin[j + 1] == '1')
			tristate[i] = '1';

		else if(bin[j] == '0' && bin[j + 1] == '1')
			tristate[i] = 'F';

		else return "invalid";
	}

	tristate[i + 1] = '\0';
	return tristate;
}

char* HCRadioResult::dec2bin(unsigned long dec, unsigned int bit_length)
{
	static const int max_length = 32 + 1; // 32 bit + newline
	static char bin[max_length];

	bit_length = min(max_length - 1, bit_length);

	for(int i = 0; i < bit_length; i ++)
	{
		if(dec == 0) bin[bit_length - 1 - i] = '0';
		else
		{
			bin[bit_length - 1 - i] = (dec & 1 > 0) ? '1' : '0';
			dec = dec >> 1;
		}
	}

	bin[bit_length + 1] = '\0';
	return bin;
}

HCRadio::HCRadio()
:send_pin(-1),irq(-1),pulse_length(-1),send_repeat(-1),status_pin(-1)
{}

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
	static unsigned int duration;
	static unsigned int change_count;
	static unsigned int timings[HCRADIO_MAX_CHANGES];
	static unsigned long last_time;
	static unsigned int repeat_count;

	noInterrupts();
	cli();

	long time = micros();
	duration = time - last_time;

	if (duration > 5000 && duration > timings[0] - 200 && duration < timings[0] + 200)
	{
		repeat_count++;
		change_count--;
		if (repeat_count == 2)
		{
			unsigned long code = 0;
			unsigned long delay = timings[0] / 31;
			unsigned long delay_tolerance = delay * 0.3;
			for (int i = 1; i < change_count; i = i + 2)
			{
				if (timings[i] > delay - delay_tolerance && timings[i]
						< delay + delay_tolerance && timings[i + 1] > delay
						* 3 - delay_tolerance && timings[i + 1] < delay * 3
						+ delay_tolerance) {
					code = code << 1;
				}
				else if (timings[i] > delay * 3 - delay_tolerance
						&& timings[i] < delay * +delay_tolerance
						&& timings[i + 1] > delay - delay_tolerance
						&& timings[i + 1] < delay + delay_tolerance)
				{
					code += 1;
					code = code << 1;
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
			//hcradio_result.raw = timings;
			//output(code, change_count / 2, delay, timings);

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
	timings[change_count++] = duration;
	last_time = time;

	interrupts();
	sei();
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

HCRadioResult HCRadio::get_result()
{
	noInterrupts();
	cli();

	HCRadioResult r = hcradio_result;
	hcradio_result.clear();

	interrupts();
	sei();

	return r;
}
