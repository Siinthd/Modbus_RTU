#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <cstdlib>
#include <math.h>
#include <malloc.h>

struct requestSingle {
	byte Slave_code[8];
};

HANDLE hComm;       //���������� �����
COMSTAT comstat;    //��������� �������� ��������� �����, � ������ ��������� ������������ ��� �����������
					//���������� �������� � ���� ������
COMMTIMEOUTS timeouts;
const char* pcComPort = "\\\\.\\COM15";
DCB dcb;

//overload debug info
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

void request_Read(requestSingle* send, int ID, int function, int address, int value)
{
	//����� ����������
	send->Slave_code[0] = ID;
	//�������������� ���
	send->Slave_code[1] = function;
	//����� ������� �������� HI-Lo //2 �����
	send->Slave_code[2] = address >> 8;
	send->Slave_code[3] = address & 0x00ff;
	//���������� ��������� Hi-Lo //2 �����
	send->Slave_code[4] = value >> 8;
	send->Slave_code[5] = value & 0x00ff;
	//CRC
	unsigned int CRC = ModRTU_CRC(send->Slave_code, 6); //2 �����

	send->Slave_code[6] = CRC;
	send->Slave_code[7] = CRC >> 8;

}
//convert byte into digital value array
void readBinary(char* number, int response_lenght)
{
	int *arr = (int*)malloc(response_lenght * 8 * sizeof(int));

	for (int j = 0; j < response_lenght; j++)
	{
		for (int i = 7, k = 0; i >= 0; i--, k++) {
			if ((number[j] & (1 << i)) != 0) {
				arr[j * 8 + k] = 1;
			}
			else {
				arr[j * 8 + k] = 0;
			}
		}
		printf("\n");
	}

	for (int i = 0; i < response_lenght; i++) {
		for (int j = 0; j < 8; j++)
			printf("%d", arr[i * 8 + j]);
		printf("\n");
	}
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

bool ReadCoilStatus();
bool ReadInputStatus()
{
	char buf[128] = { 0 };
	requestSingle request;
	DWORD bytesRead, dwEventMask, bytesWritten, temp;

	int* bus;
	float* test;
	long* rdLng;
	double* rdDbl;

	request_Read(&request, 1, 4, 0, 20);
	PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);
	//test loop
	while (1) {

		if (!WriteFile(hComm, &request, sizeof(request), &bytesWritten, NULL)) {
			perror("error: ");
			return FALSE;
		}
		printPackage(&request, bytesWritten, 0);
		SetCommMask(hComm, EV_RXCHAR);
		WaitCommEvent(hComm, &dwEventMask, NULL);

		Sleep(500);

		if ((dwEventMask & EV_RXCHAR) != 0)
		{
			ClearCommError(hComm, NULL, &comstat);
			bytesRead = comstat.cbInQue;
			if (bytesRead)
			{
				if (!ReadFile(hComm, buf, bytesRead, &temp, NULL))
				{
					printf("\nA timeout occured.\n");
				}
				printPackage(buf, bytesRead, 1);
				int response_lenght = buf[2];

				printf("\nCRC sum:\n");
				printf("%02X %02X\n\n", (byte)ModRTU_CRC((byte*)buf, bytesRead - 2), (byte)(ModRTU_CRC((byte*)buf, bytesRead - 2) >> 8));

				bus = readInt(buf, bytesRead);
				test = readInverseFloat(buf, response_lenght); // responce_lenght possible vulnerability
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
				free(rdLng);
				free(bus);
				free(test);
			}
		}
		PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);
		memset(buf, 0, sizeof(buf));
		getchar();
	}
	return TRUE;
}
bool ReadHoldingRegisters();
bool ReadInputRegisters();


bool OpenPort()
{
	hComm = CreateFile(pcComPort,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0, 
		NULL);
	if (hComm != INVALID_HANDLE_VALUE)
	{
		if (!GetCommState(hComm, &dcb))
		{
			printf("GetCommState error\n");
			return false;
		}
		dcb.BaudRate = CBR_9600;
		dcb.ByteSize = 8;
		dcb.Parity = NOPARITY;
		dcb.StopBits = ONESTOPBIT;
		if (SetCommState(hComm, &dcb))
			printf("%s baud rate is %d\n", pcComPort, (int)dcb.BaudRate);

		if (!GetCommTimeouts(hComm, &timeouts))
		{
			printf("GetCommState error\n");
			return false;
		}

		double dblBitsPerByte = 1 + dcb.ByteSize + dcb.StopBits + (dcb.Parity ? 1 : 0);
		timeouts.ReadIntervalTimeout = (DWORD)ceil((3.5f*dblBitsPerByte / (double)dcb.BaudRate * 1000.0f));
		timeouts.ReadTotalTimeoutMultiplier = 0;
		timeouts.ReadTotalTimeoutConstant = 0;
		timeouts.WriteTotalTimeoutMultiplier = 500;
		timeouts.WriteTotalTimeoutConstant = 500;
		SetCommTimeouts(hComm, &timeouts);

		return true;
	}
	else return false;
}


int main()
{

	switch (OpenPort())
	{
	case TRUE:
		if (!ReadInputStatus())
		{
			printf("can't execute Read Input Status.\n");
			return EXIT_FAILURE;
		}
		break;

	default:
	{
		printf("%s opening error\n", pcComPort);
		break;
	}


	}
	CloseHandle(hComm);
	return 0;

}
