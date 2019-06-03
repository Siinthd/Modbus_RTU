#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <iomanip>
#include <math.h>
#include <malloc.h>

struct requestSingle {
	byte Slave_code[8];
};

void printPackage(requestSingle* data, int size, int isin)
{
	printf("%s bytes: %d\n\r\t", (isin) ? "Received" : "Sent", size);
	for (int i = 0; i < size; i++)
		printf("%02X ", data->Slave_code[i]);
	printf("\n\r");
}

void printPackage(char* data, int size, int isin)
{
	printf("%s bytes: %d\n\r\t", (isin) ? "Received" : "Sent", size);
	for (int i = 0; i < size; i++)
		printf("%02X ", (byte)data[i]);
	printf("\n\r");

}

uint16_t ModRTU_CRC(byte* buf, int len)
{
	uint16_t crc = 0xFFFF;

	for (int pos = 0; pos < len; pos++) {
		crc ^= (uint16_t)buf[pos];          // XOR byte into least sig. byte of crc

		for (int i = 8; i != 0; i--) {    // Loop over each bit
			if ((crc & 0x0001) != 0) {      // If the LSB is set
				crc >>= 1;                    // Shift right and XOR 0xA001
				crc ^= 0xA001;
			}
			else                            // Else LSB is not set
				crc >>= 1;                    // Just shift right
		}
	}
	// Note, this number has low and high bytes swapped, so use it accordingly (or swap bytes)
	return crc;
}