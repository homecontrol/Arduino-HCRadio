#include <HCRadio.h>

static HCRadioResult hcradio_result;

HCRadioResult::HCRadioResult()
:decimal(-1),length(-1),delay(-1),raw(NULL)
{};

void HCRadioResult::clear()
{
	decimal = -1;
	length = -1;
	delay = -1;
	raw = NULL;
}

bool HCRadioResult::is_ready()
{
	return decimal != -1;
}

String HCRadioResult::get_json()
{
	// TODO: Generate json description of result.

	if (decimal == 0)
		return "{\"error\":\"unknown encoding\"}";

	String json = "";
	char* b = dec2bin(decimal, length);

	json += "\"decimal\":";
	json += "\"" + String(decimal) + "\",";

	json += "\"binary\":";
	json += "\"" + String(b) + "\",";

	json += "\"tristate\":";
	json += "\"" + String(HCRadioResult::bin2tristate(b)) + "\",";

	json += "\"pulse_length\":";
	json += "\"" + String(delay) + "\",";

	if(length > 0)
	{
		json += "\"raw\": [\"" + String(raw[0]) + "\"";
		for (int i = 1; i <= length * 2; i++)
			json += ",\"" + String(raw[i]) + "\"";
	}

	return "{" + json + "}";
}

char* HCRadioResult::bin2tristate(char* bin)
{
	char returnValue[50];
	int pos = 0;
	int pos2 = 0;

	while (bin[pos] != '\0' && bin[pos + 1] != '\0')
	{
		if (bin[pos] == '0' && bin[pos + 1] == '0')
		{
			returnValue[pos2] = '0';
		}
		else if (bin[pos] == '1' && bin[pos + 1] == '1')
		{
			returnValue[pos2] = '1';
		}
		else if (bin[pos] == '0' && bin[pos + 1] == '1')
		{
			returnValue[pos2] = 'F';
		}
		else
		{
			return "not applicable";
		}
		pos = pos + 2;
		pos2++;
	}

	returnValue[pos2] = '\0';
	return returnValue;
}

char* HCRadioResult::dec2bin(unsigned long dec, unsigned int bit_length)
{
	static char bin[64];
	unsigned int i = 0;

	while (dec > 0)
	{
		bin[32 + i++] = (dec & 1 > 0) ? '1' : '0';
		dec = dec >> 1;
	}

	for (unsigned int j = 0; j < bit_length; j++)
	{
		if (j >= bit_length - i)
		{
			bin[j] = bin[31 + i - (j - (bit_length - i))];
		}
		else
		{
			bin[j] = '0';
		}
	}
	bin[bit_length] = '\0';

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
			unsigned long delayTolerance = delay * 0.3;
			for (int i = 1; i < change_count; i = i + 2)
			{
				if (timings[i] > delay - delayTolerance && timings[i]
						< delay + delayTolerance && timings[i + 1] > delay
						* 3 - delayTolerance && timings[i + 1] < delay * 3
						+ delayTolerance) {
					code = code << 1;
				}
				else if (timings[i] > delay * 3 - delayTolerance
						&& timings[i] < delay * +delayTolerance
						&& timings[i + 1] > delay - delayTolerance
						&& timings[i + 1] < delay + delayTolerance)
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

			HCRadioResult &result = HCRadio::get_result();
			result.decimal = code;
			result.length = change_count / 2;
			result.delay = delay;
			result.raw = timings;

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

HCRadioResult& HCRadio::get_result()
{
	return hcradio_result;
}
