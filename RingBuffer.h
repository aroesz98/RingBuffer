/*
 * RingBuffer.h
 *
 *  Created on: 14 sty 2023
 *      Author: admin
 */

#ifndef INC_RINGBUFFER_H_
#define INC_RINGBUFFER_H_

#include "usart.h"

typedef enum {
	WIFI_ERR_TIMEOUT = -1,					// Time-out occurred at listen from serial
	WIFI_ERR_OK = 0,						// Command successful
	WIFI_ERR_ERROR = 1,						// Command error
	WIFI_ERR_CONNECT,						// IP connection has been completed
	WIFI_ERR_SENDOK,						// Send successful
	WIFI_ERR_BUSY,							// Transmission busy
	WIFI_ERR_SENDFAIL,						// Sending failed
	WIFI_ERR_CLOSED							// IP connection has been closed
} WIFI_ERR;

typedef enum {								// Return enumeration from AT+CIPSTATUS
	WIFI_STATUS_GOTIP,
	WIFI_STATUS_CONN,
	WIFI_STATUS_DISCONN,
	WIFI_STATUS_NOTCONN,
	WIFI_STATUS_UNKNOWN
} WIFI_STATUS;

class RingBuffer
{
	private:
		UART_HandleTypeDef* __huart;
		bool objInited = 0;

		uint16_t __buffer_size = 256;

		typedef struct
		{
			unsigned char* buffer;
			volatile unsigned int head;
			volatile unsigned int tail;
		} __ring_buffer_t;

		__ring_buffer_t __rx_buffer = { { 0 }, 0, 0};
		__ring_buffer_t __tx_buffer = { { 0 }, 0, 0};

		__ring_buffer_t* __tx_buffer_ptr;
		__ring_buffer_t* __rx_buffer_ptr;

		char staIP[32] = {0};

		void storeCharacter(unsigned char c, __ring_buffer_t *buffer);

	public:
		void ISR_Handler(void);
		void init(UART_HandleTypeDef& huart, uint16_t buffer_size = 256);

		int isDataAvailable(void);
		int peek(void);
		void flush (void);
		int read(void);
		void write(int c);
		void sendString(const char *s);

		bool scan(const char *token);
		int lookFor (char *str, char *buffertolookinto);
		int waitFor (char *string);
		int copyUpTo (char *string, char *buffertocopyinto);
		int8_t readUntil(uint8_t *result, uint8_t terminator);
		int getAfter (char *string, uint8_t numberofchars, char *buffertosave);

		char* getStaticIP(void);
		WIFI_STATUS checkStatus(void);
		WIFI_ERR response(uint32_t timeOut);
		WIFI_ERR join(const char *ssid, const char *pwd);
};

#endif /* INC_RINGBUFFER_H_ */
