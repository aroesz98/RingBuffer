#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "RingBuffer.h"

#define TIMEOUT_DEF 500
uint16_t timeout;

using namespace std;

void RingBuffer::ISR_Handler(void)
{
	if(!objInited) return;

	uint32_t isrflags   = READ_REG(__huart->Instance->SR);
	uint32_t cr1its     = READ_REG(__huart->Instance->CR1);

	/* if DR is not empty and the Rx Int is enabled */
	if (((isrflags & USART_SR_RXNE) != RESET) && ((cr1its & USART_CR1_RXNEIE) != RESET))
	{
		__huart->Instance->SR;                       /* Read status register */
		unsigned char c = __huart->Instance->DR;     /* Read data register */
		storeCharacter (c, __rx_buffer_ptr);  // store data in buffer
		return;
	}

	/*If interrupt is caused due to Transmit Data Register Empty */
	if (((isrflags & USART_SR_TXE) != RESET) && ((cr1its & USART_CR1_TXEIE) != RESET))
	{
		if(__tx_buffer.head == __tx_buffer.tail)
		{
			// Buffer empty, so disable interrupts
			__HAL_UART_DISABLE_IT(__huart, UART_IT_TXE);
		}

		else
		{
			// There is more data in the output buffer. Send the next byte
			unsigned char c = __tx_buffer.buffer[__tx_buffer.tail];
			__tx_buffer.tail = (__tx_buffer.tail + 1) % __buffer_size;

			__huart->Instance->SR;
			__huart->Instance->DR = c;
		}
		return;
	}
}

void RingBuffer::storeCharacter(unsigned char c, __ring_buffer_t *buffer)
{
  int i = (unsigned int)(buffer->head + 1) % __buffer_size;

	if(i != static_cast<int>(buffer->tail))
	{
		buffer->buffer[buffer->head] = c;
		buffer->head = i;
	}
}

void RingBuffer::init(UART_HandleTypeDef& huart, uint16_t buffer_size)
{
	__rx_buffer_ptr = &__rx_buffer;
	__tx_buffer_ptr = &__tx_buffer;

	__huart = &huart;
	__buffer_size = buffer_size;

	__rx_buffer.buffer = (uint8_t*)malloc(__buffer_size);
	__tx_buffer.buffer = (uint8_t*)malloc(__buffer_size);

	// Enable the UART Error Interrupt
	__HAL_UART_ENABLE_IT(__huart, UART_IT_ERR);

	// Enable the UART Data Register not empty Interrupt
	__HAL_UART_ENABLE_IT(__huart, UART_IT_RXNE);

	objInited = true;

	sendString("AT+RST\r\n");
	while(!(waitFor("ready")));
}

void RingBuffer::flush (void)
{
	memset(__rx_buffer_ptr->buffer,'\0', __buffer_size);
	__rx_buffer_ptr->head = 0;
	__rx_buffer_ptr->tail = 0;
}

int RingBuffer::peek()
{
	if(__rx_buffer_ptr->head == __rx_buffer_ptr->tail)
	{
		return -1;
	}
	else
	{
		return __rx_buffer_ptr->buffer[__rx_buffer_ptr->tail];
	}
}

int RingBuffer::read(void)
{
	if(__rx_buffer_ptr->head == __rx_buffer_ptr->tail)
	{
		return -1;
	}
	else
	{
		unsigned char c = __rx_buffer_ptr->buffer[__rx_buffer_ptr->tail];
		__rx_buffer_ptr->tail = (unsigned int)(__rx_buffer_ptr->tail + 1) % __buffer_size;
		return c;
	}
}

void RingBuffer::write(int c)
{
	if (c >= 0)
	{
		int i = (__tx_buffer_ptr->head + 1) % __buffer_size;
		while (i == static_cast<int>(__tx_buffer_ptr->tail));

		__tx_buffer_ptr->buffer[__tx_buffer_ptr->head] = (uint8_t)c;
		__tx_buffer_ptr->head = i;

		__HAL_UART_ENABLE_IT(__huart, UART_IT_TXE); // Enable UART transmission interrupt
	}
}

void RingBuffer::sendString (const char *s)
{
	while(*s) write(*s++);
}

int RingBuffer::isDataAvailable(void)
{
	return (uint16_t)(__buffer_size + __rx_buffer_ptr->head - __rx_buffer_ptr->tail) % __buffer_size;
}

int RingBuffer::waitFor (char *string)
{
	int so_far =0;
	int len = strlen (string);

again:
	timeout = TIMEOUT_DEF;
	while ((!isDataAvailable())&&timeout);  // let's wait for the data to show up
	if (timeout == 0) return 0;
	while (peek() != string[so_far])
	{
		if (__rx_buffer_ptr->tail != __rx_buffer_ptr->head)
		{
			__rx_buffer_ptr->tail = (unsigned int)(__rx_buffer_ptr->tail + 1) % __buffer_size;  // increment the tail
		}

		else
		{
			return 0;
		}
	}

	while (peek() == string [so_far]) // if we got the first letter
	{
		so_far++;
		__rx_buffer_ptr->tail = (unsigned int)(__rx_buffer_ptr->tail + 1) % __buffer_size;  // increment the tail
		if (so_far == len) return 1;
		timeout = TIMEOUT_DEF;
		while ((!isDataAvailable())&&timeout);
		if (timeout == 0) return 0;
	}

	if (so_far != len)
	{
		so_far = 0;
		goto again;
	}

	if (so_far == len) return 1;
	else return 0;
}

int RingBuffer::lookFor (char *str, char *buffertolookinto)
{
	int stringlength = strlen (str);
	int bufferlength = strlen (buffertolookinto);
	int so_far = 0;
	int indx = 0;
repeat:
	while (str[so_far] != buffertolookinto[indx]) indx++;
	if (str[so_far] == buffertolookinto[indx])
	{
		while (str[so_far] == buffertolookinto[indx])
		{
			so_far++;
			indx++;
		}
	}

	else
	{
		so_far =0;
		if (indx >= bufferlength) return -1;
		goto repeat;
	}

	if (so_far == stringlength) return 1;
	else return -1;
}

int RingBuffer::copyUpTo (char *string, char *buffertocopyinto)
{
	int so_far =0;
	int len = strlen (string);
	int indx = 0;

again:
	while (!isDataAvailable());
	while (peek() != string[so_far])
		{
			buffertocopyinto[indx] = __rx_buffer_ptr->buffer[__rx_buffer_ptr->tail];
			__rx_buffer_ptr->tail = (unsigned int)(__rx_buffer_ptr->tail + 1) % __buffer_size;
			indx++;
			while (!isDataAvailable());

		}
	while (peek() == string [so_far])
	{
		so_far++;
		buffertocopyinto[indx++] = read();
		if (so_far == len) return 1;
		while (!isDataAvailable());
	}

	if (so_far != len)
	{
		so_far = 0;
		goto again;
	}

	if (so_far == len) return 1;
	else return 0;
}

bool RingBuffer::scan(const char *token) {
	uint32_t	start;
	int		c;
	const char	*sp;

	sp = token;								// Save comparison source
	// Save starting time,
	// until even the longest reach in the time-out.
	start = HAL_GetTick();
	while (!isDataAvailable());
	while (*sp) {
		if ((c = peek()) >= 0) {
			// Verify the read characters
			if ((char)c == *sp)
				sp++;
			else
				sp = token;

		}
		// Scan time-out
		if (HAL_GetTick() - start > 2000)
			break;
		__rx_buffer_ptr->tail = (unsigned int)(__rx_buffer_ptr->tail + 1) % __buffer_size;
		while (!isDataAvailable());
	}
	return (*sp == '\0');
}

int8_t RingBuffer::readUntil(uint8_t *result, uint8_t terminator)
{
	uint32_t	start;
	int			c;
	int8_t		count = 0;

	while (!isDataAvailable());
	// Save starting time, Start scanning.
	start = HAL_GetTick();
	// Start scanning
	c = read();
	while ((uint8_t)c != terminator) {
		// Save available reading character
		if (c >= 0) {
			*result++ = (uint8_t)c;
			count++;
		}
		// until even the longest reach in the time-out.
		if (HAL_GetTick() - start > 2000)
			break;
		// Read next
		//__rx_buffer_ptr->tail = (unsigned int)(__rx_buffer_ptr->tail + 1) % __buffer_size;
		while (!isDataAvailable());
		c = read();
	}
	*result = '\0';
	return count;
}

int RingBuffer::getAfter (char *string, uint8_t numberofchars, char *buffertosave)
{
	for (int indx=0; indx<numberofchars; indx++)
	{
		uint32_t start = HAL_GetTick();
		while ((!isDataAvailable()) && HAL_GetTick() - start < TIMEOUT_DEF);
		if (timeout == 0) return 0;
		buffertosave[indx] = read();
	}
	return 1;
}

WIFI_STATUS RingBuffer::checkStatus(void)
{
	WIFI_STATUS	sta = WIFI_STATUS_UNKNOWN;

	char readbuff[8];

	sendString("AT+CIPSTATUS\r\n");
	while(!(waitFor("STATUS:")));
	while (!(copyUpTo("\r\n", readbuff)));
	while (!(waitFor("OK\r\n")));

	int len = strlen (readbuff);
	readbuff[len-1] = '\0';

	switch (readbuff[0])
	{
		case '2' :
			sta = WIFI_STATUS_GOTIP;
		break;

		case '3' :
			sta = WIFI_STATUS_CONN;
		break;

		case '4' :
			sta = WIFI_STATUS_DISCONN;
		break;

		case '5' :
			sta = WIFI_STATUS_NOTCONN;
		break;
	}

	flush();
	return sta;
}

WIFI_ERR RingBuffer::join(const char *ssid, const char *pwd) {
	WIFI_STATUS status = checkStatus();
	WIFI_ERR err = WIFI_ERR_ERROR;
	char tmp_buff[128];
	flush();
//
//	switch (status)
//	{
//		case WIFI_STATUS_GOTIP:
//			err = WIFI_ERR_CONNECT;
//		break;
//
//		case WIFI_STATUS_CONN:
//			err = WIFI_ERR_CONNECT;
//		break;
//
//		case WIFI_STATUS_DISCONN:
//			sendString("AT+CWQAP\r\n");
//			while (!(waitFor("OK\r\n")));
//
//			sendString("AT+CWMODE=1\r\n");
//			while (!(waitFor("AT+CWMODE=1\r\n\r\nOK\r\n")));
//
//			sprintf(tmp_buff, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pwd);
//			sendString(tmp_buff);
//			while (!(waitFor("OK\r\n")));
//			flush();
//			err = WIFI_ERR_OK;
//		break;
//
//		case WIFI_STATUS_NOTCONN:
//			sendString("AT+CWMODE=1\r\n");
//			while (!(waitFor("AT+CWMODE=1\r\n\r\nOK\r\n")));
//			sprintf(tmp_buff, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pwd);
//			sendString(tmp_buff);
//			while (!(waitFor("OK\r\n")));
//			flush();
//			err = WIFI_ERR_OK;
//		break;
//
//		case WIFI_STATUS_UNKNOWN:
//			err = WIFI_ERR_ERROR;
//		break;
//	}
//	return err;
	switch (status)
	{
		case WIFI_STATUS_GOTIP:
			err = WIFI_ERR_CONNECT;
		break;

		case WIFI_STATUS_CONN:
			err = WIFI_ERR_CONNECT;
		break;

		case WIFI_STATUS_DISCONN:
			sendString("AT+CWQAP\r\n");
			while (!(waitFor("OK\r\n")));

			sendString("AT+CWMODE=1\r\n");
			while (!(waitFor("AT+CWMODE=1\r\n\r\nOK\r\n")));

			sprintf(tmp_buff, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pwd);
			sendString(tmp_buff);
			err = response(10000);

			flush();
		break;

		case WIFI_STATUS_NOTCONN:
			sendString("AT+CWMODE=1\r\n");
			while (!(waitFor("AT+CWMODE=1\r\n\r\nOK\r\n")));

			sprintf(tmp_buff, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pwd);
			sendString(tmp_buff);
			err = response(10000);

			flush();
		break;

		case WIFI_STATUS_UNKNOWN:
			err = WIFI_ERR_ERROR;
		break;
	}
	return err;
}

char* RingBuffer::getStaticIP(void)
{
	flush();
	memset(staIP, '\0', 32);
	sendString("AT+CIFSR\r\n");

	if(scan("+CIFSR:STAIP,\""))
	{
		readUntil((uint8_t *)staIP, (uint8_t)'"');
	}

	return staIP;
}

struct {
	const char	*term;						// The term to be detected
	uint8_t		state;						// Byte offset which
	WIFI_ERR	condition;					// Return condition when the term detected
} static _FIND_STATE[] = {
	{ "CONNECT\r\n", 0, WIFI_ERR_CONNECT },
	{ "SEND OK\r\n", 0, WIFI_ERR_SENDOK },
	{ "SEND FAIL",   0, WIFI_ERR_SENDFAIL },
	{ "CLOSED",		 0, WIFI_ERR_CLOSED },
	{ "busy",		 0, WIFI_ERR_BUSY },
	{ "\nERROR",	 0, WIFI_ERR_ERROR },
	{ "\nOK\r\n",	 0, WIFI_ERR_OK },
	{ NULL,			 0, WIFI_ERR_TIMEOUT }
};

WIFI_ERR RingBuffer::response(uint32_t timeOut)
{
	WIFI_ERR	err;
	uint32_t	start;
	int16_t		c;
	uint8_t	iNode, state;

	// Initialize the state number that must be positioned at
	// start of the term.
	for (iNode = 0; _FIND_STATE[iNode].term != NULL; iNode++)
	_FIND_STATE[iNode].state = 0;

	// Save start time, start scan of receiving stream.
	err = WIFI_ERR_TIMEOUT;
	start = HAL_GetTick();
	do {
		// During the period of time following a state transition.
		if ((c = peek()) >= 0)
		{
			// Start comparison of phrase of the term with receiving stream.
			for (iNode = 0; _FIND_STATE[iNode].term != NULL; iNode++)
			{
				state = _FIND_STATE[iNode].state;
				// The state number reaches at end of the term, scan process
				// should be ended and the 'err' is set by the enumeration
				// value named as 'condition'.
				if ((char)c == _FIND_STATE[iNode].term[state])
				{
					state++;

					if (_FIND_STATE[iNode].term[state] == '\0')
					{
						err = _FIND_STATE[iNode].condition;
						break;
					}

					else
					{
						// If a receiving character matches the current
						// phrase of the term, state number would be increased.
						_FIND_STATE[iNode].state = state;
					}
				}
			}
			__rx_buffer_ptr->tail = (unsigned int)(__rx_buffer_ptr->tail + 1) % __buffer_size;
		}
	// At some term detection, escape from scanning.
	} while (err == WIFI_ERR_TIMEOUT && (HAL_GetTick() - start < timeOut));
	return err;
}
