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

//------------request: 0x01 - 0x04

void request_Read(requestSingle* send, int ID, int function, int address, int value)
{
	//device address
	send->Slave_code[0] = ID;
	//functional code
	send->Slave_code[1] = function;
	//1 register address HI-Lo //2 bytes
	send->Slave_code[2] = address >> 8;
	send->Slave_code[3] = address & 0x00ff;
	//number of registers Hi-Lo //2 байта
	send->Slave_code[4] = value >> 8;
	send->Slave_code[5] = value & 0x00ff;
	//CRC
	unsigned int CRC = ModRTU_CRC(send->Slave_code, 6); //2 bytes

	send->Slave_code[6] = CRC;
	send->Slave_code[7] = CRC >> 8;
}


//------------request: 0x01 - 0x04 (draft)
void request_WRITE(requestSingle* send, int ID, int function, int address, int value)
{
	send->Slave_code[0] = ID;
	//functional code
	send->Slave_code[1] = function;
	//1 register address HI-Lo //2 bytes
	send->Slave_code[2] = address >> 8;
	send->Slave_code[3] = address & 0x00ff;
	//value Hi-Lo byte
	send->Slave_code[4] = value >> 8;
	send->Slave_code[5] = value & 0x00ff;
	//CRC
	unsigned int CRC = ModRTU_CRC(send->Slave_code, 6); //2 bytes

	send->Slave_code[6] = CRC;
	send->Slave_code[7] = CRC >> 8;
}


int* readInt(char* buf, int response_lenght)   //Convert to INT 2 bytes
{
	int *intarray = (int*)malloc((response_lenght / 2) * sizeof(int));

	for (unsigned int i = 3, j = 0; i < response_lenght - 2; i += 2, j++)
		intarray[j] = ((byte)buf[i] << 8) | (byte)buf[i + 1];

	return intarray;
}


float* readInverseFloat(char* buf, int response_lenght)  //Convert to Float IEEE 754 4 bytes
{
	float *farray = (float*)malloc((response_lenght / 4) * sizeof(float));

	BYTE ui[4];
	for (int i = 3, j = 0; j < (response_lenght / 4) - 1; i += 4, j++)
	{
		ui[3] = buf[i];
		ui[2] = buf[i + 1];
		ui[1] = buf[i + 2];
		ui[0] = buf[i + 3];
		memcpy(&farray[j], ui, 4);
	}
	return farray;
}

double* readDouble(char* buf, int response_lenght) //Convert to  Double IEEE 754 8 bytes
{
	BYTE ul[8];
	double* rdDbl = (double*)malloc((response_lenght / 8) * sizeof(double));

	for (int i = 3, j = 0; j < (response_lenght / 8); i += 8, j++) { //Convert to double
		for (int k = 7; k >= 0; k--)
			ul[k] = buf[i + k];
		memcpy(&rdDbl[j], ul, 8);
	}
	return rdDbl;
}

long* readLong(char* buf, int response_lenght) //Convert to long IEEE 754 4 bytes
{
	long* rdLng = (long*)malloc((response_lenght / 4) * sizeof(long));
	for (unsigned int i = 3, j = 0; j <= (response_lenght / 4) - 1; i += 4, j++) {
		rdLng[j] = ((byte)buf[i] << 24 | (byte)buf[i + 1] << 16 | (byte)buf[i + 2] << 8 | (byte)buf[i + 3]);
	}
	return rdLng;
}


int main(void)
{
	requestSingle test;
	request_Read(&test, 1, 4, 0, 20);

	HANDLE hComm;       //port handle
	COMSTAT comstat;    //structure of the current state of the port, in this program is used to determine
						//the number of bytes received at the port
	const char* pcComPort = "\\\\.\\COM6";
	DCB dcb;
	DWORD bytesRead, dwEventMask, bytesWritten, temp;
	char buf[128] = { 0 };

	hComm = CreateFile(pcComPort,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0, //for Modbus set FILE_FLAG_OVERLAPPED, for tests 0
		NULL);
	if (hComm == INVALID_HANDLE_VALUE)
	{
		printf("%s opening error\n", pcComPort);
		return 0;
	}
	GetCommState(hComm, &dcb);

	dcb.BaudRate = CBR_9600;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;

	SetCommState(hComm, &dcb);

	COMMTIMEOUTS timeouts;
	GetCommTimeouts(hComm, &timeouts);


	double dblBitsPerByte = 1 + dcb.ByteSize + dcb.StopBits + (dcb.Parity ? 1 : 0);
	timeouts.ReadIntervalTimeout = (DWORD)ceil((3.5f*dblBitsPerByte / (double)dcb.BaudRate * 1000.0f));
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.ReadTotalTimeoutConstant = 0;
	timeouts.WriteTotalTimeoutMultiplier = 500;
	timeouts.WriteTotalTimeoutConstant = 500;
	SetCommTimeouts(hComm, &timeouts);


	printf("%s baud rate is %d\n", pcComPort, (int)dcb.BaudRate);
												//synchronous connect
	do
	{
		WriteFile(hComm, &test, sizeof(test), &bytesWritten, NULL);
		printPackage(&test, bytesWritten, 0);

		SetCommMask(hComm, EV_RXCHAR);

		WaitCommEvent(hComm, &dwEventMask, NULL);
		Sleep(500);
		if ((dwEventMask & EV_RXCHAR) != 0)
		{
			ClearCommError(hComm, NULL, &comstat);
			bytesRead = comstat.cbInQue;
			if (bytesRead)
			{
				ReadFile(hComm, buf, bytesRead, &temp, NULL);
				if (temp < 1)
				{
					printf("\nA timeout occured.\n");
					break;
				}
				printPackage(buf, bytesRead, 1);
				int response_lenght = buf[2];

				printf("\nCRC sum:\n");
				printf("%02X %02X\n\n", (byte)ModRTU_CRC((byte*)buf, bytesRead - 2), (byte)(ModRTU_CRC((byte*)buf, bytesRead - 2) >> 8));


				int* bus = (int*)malloc((response_lenght / 2) * sizeof(int));
				float* test = (float*)malloc((response_lenght / 4) * sizeof(float));
				long* rdLng = (long*)malloc((response_lenght / 4) * sizeof(long));
				double* rdDbl = (double*)malloc((response_lenght / 8) * sizeof(double));

				bus = readInt(buf, bytesRead);
				test = readInverseFloat(buf, response_lenght);
				rdLng = readLong(buf, response_lenght);
				rdDbl = readDouble(buf, response_lenght);


				printf("\n\n Hex:\t\tIntegers:");  
				for (unsigned int i = 3, j = 0; i < bytesRead - 2; i += 2, j++)
				{
					printf("\n%04X\t\t%d", bus[j], bus[j]);
				}

				printf("\n\n\n Inverse Floats:");            
				for (int i = 0; i < (response_lenght / 4) - 1; i++)
					printf("\n%f", test[i]);

				printf("\n\n\n Hex(long):\t\t long:");       
				for (unsigned int i = 3, j = 0; j <= (response_lenght / 4) - 1; i += 4, j++)
				{
					printf("\n%08X\t\t%ld", rdLng[j], rdLng[j]);
				}

				printf("\n\n\n double:");
				for (int i = 0; i < (response_lenght / 8); i++)
					printf("\n%e", rdDbl[i]);

													//Free memory
				free(rdDbl);
				rdDbl = NULL;
				free(rdLng);
				rdLng = NULL;
				free(bus);
				bus = NULL;
				free(test);
				test = NULL;
			}
		}
													//Purge port and buffer
		PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);
		memset(buf, 0, sizeof(buf));

		getchar();
	} while (1);

	CloseHandle(hComm);

	return 0;
}
