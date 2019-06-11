#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <cstdlib>
#include <math.h>
#include <malloc.h>
#include <assert.h>
#include <conio.h>



struct requestSingle {
	byte Slave_code[8];
};

HANDLE hComm;       //дескриптор порта
COMSTAT comstat;    //структура текущего состояния порта, в данной программе используется для определения
					//количества принятых в порт байтов
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

bool CRC_Check(byte* buf, int bytesRead)
{
	unsigned int source;
	unsigned int CRC = ModRTU_CRC(buf, bytesRead - 2);
	source = (byte)buf[bytesRead - 1] << 8 | (byte)buf[bytesRead - 2];
	if (source == CRC) {
		printf("CRC check!\n");
		return 1;
	}
	else {
		printf("wrong CRC (%02X vs %02X)!\n", source, CRC);
		return 0;
	}
}

void request_Read(requestSingle* send, int ID, int function, int address, int value)
{
	//адрес устройства
	send->Slave_code[0] = ID;
	//функциональный код
	send->Slave_code[1] = function;
	//адрес первого регистра HI-Lo //2 байта
	send->Slave_code[2] = address >> 8;
	send->Slave_code[3] = address & 0x00ff;
	//количество регистров Hi-Lo //2 байта
	send->Slave_code[4] = value >> 8;
	send->Slave_code[5] = value & 0x00ff;
	//CRC
	unsigned int CRC = ModRTU_CRC(send->Slave_code, 6); //2 байта

	send->Slave_code[6] = CRC;
	send->Slave_code[7] = CRC >> 8;

}


bool ModbussErrorCheck(byte* buffer, byte function)
{
	if (buffer[1] == (function ^ 0x80)) {
		printf("\nError:");
		switch (buffer[2]) {
		case 0x01:printf("Illegal Function\n");
			break;
		case 0x02:printf("Illegal Data Address\n");
			break;
		case 0x03:printf("Illegal Data Value\n");
			break;
		case 0x04:printf("Slave Device Failure\n");
			break;
		case 0x05:printf("Acknowledge\n");
			break;
		case 0x06:printf("Slave Device Busy\n");
			break;
		case 0x07:printf("Negative Acknowledge\n");
			break;
		case 0x08:printf("Memory Parity Error\n");
			break;
		default:
			printf("Error %d\n", buffer[2]);
			break;
		}
		return FALSE;
	}
	return TRUE;
}

bool nb_read_impl(char* buf, requestSingle request)
{
	DWORD bytesRead, dwEventMask, bytesWritten, temp;
	PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);
	//test loop
	if (!WriteFile(hComm, &request, sizeof(request), &bytesWritten, NULL)) {
		perror("error: ");
		return FALSE;
	}
	printPackage(&request, bytesWritten, 0);
	SetCommMask(hComm, EV_RXCHAR);
	WaitCommEvent(hComm, &dwEventMask, NULL);
	Sleep(50);
	if ((dwEventMask & EV_RXCHAR) != 0) {
		ClearCommError(hComm, NULL, &comstat);
		bytesRead = comstat.cbInQue;
		if (bytesRead) {
			ReadFile(hComm, buf, bytesRead, &temp, NULL);
			printPackage(buf, bytesRead, 1);
			CRC_Check((byte*)buf, (int)bytesRead);  //make assert
		}
	}
	assert(ModbussErrorCheck((byte*)buf, request.Slave_code[1]));
	return TRUE;
}

//convert byte into digital value array
int* readBinary(char* number, int response_lenght)
{
	int *arr = (int*)malloc(response_lenght * 8 * sizeof(int));

	for (int j = 0; j < response_lenght; j++) {
		for (int i = 7, k = 0; i >= 0; i--, k++) {
			if ((number[j + 3] & (1 << i)) != 0) {
				arr[j * 8 + k] = 1;
			}
			else {
				arr[j * 8 + k] = 0;
			}
		}
	}
	//debug info
	printf("\n\n*******readBinary function*******\n");
	for (int i = 0; i < response_lenght; i++) {
		for (int j = 0; j < 8; j++)
			printf("%d", arr[i * 8 + j]);
		printf("\n\n");
	}
	printf("*******End function*******\n\n\n");
	return arr;
}

int* readInt(char* buf, int response_lenght)   //Convert to INT 2 bytes
{
	int *intarray = (int*)malloc((response_lenght) / 2 * sizeof(int));
	for (int i = 3, j = 0; j < (response_lenght) / 2; i += 2, j++)
	{
		intarray[j] = ((byte)buf[i] << 8) | (byte)buf[i + 1];
	}
	return intarray;
}

float* readInverseFloat(char* buf, int response_lenght)  //Convert to Float IEEE 754 4 bytes
{
	float *farray = (float*)malloc((response_lenght / 4) * sizeof(float));
	BYTE ui[4];
	for (int i = 3, j = 0; j < (response_lenght / 4); i += 4, j++)
	{
		ui[0] = buf[i];
		ui[1] = buf[i + 1];
		ui[2] = buf[i + 2];
		ui[3] = buf[i + 3];
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
	for (int i = 3, j = 0; j < (response_lenght / 4); i += 4, j++) {
		rdLng[j] = ((byte)buf[i] << 24 | (byte)buf[i + 1] << 16 | (byte)buf[i + 2] << 8 | (byte)buf[i + 3]);
	}
	return rdLng;
}

bool ReadStatus(int function)       //0x01 - 0x02  Read D0 - D1
{
	if (function == 1)
		printf("\n*********Read Coil Status function**********\n");
	else if (function == 2)
		printf("\n*********Read Input Status function**********\n");
	else
		return false;

	const int value = 37;               //debug info
	int address = 19;                   //real address - 1
	int ID = 11;

	struct Coil {                       //store values
		int Coil_value;
		int address;
	}Coli_1[value];
	char buf[128] = { 0 };
	requestSingle request;
	request_Read(&request, ID, function, address, value);

	int* arr;
	assert(nb_read_impl(buf, request));

	int lenght = buf[2];
	arr = readBinary(buf, lenght);
	//coils
	int i = 0;
	int Newadd = (function == 1) ? (address + 1) : (address + 10001);
	//debug info
	printf("  address |   Value    \n"
		"----------+----------\n");
	for (int j = 0; j < lenght; j++) {
		for (int k = 7; k >= 0; k--) {
			if (i < value) {
				Coli_1[i].address = Newadd + i;
				Coli_1[i].Coil_value = arr[j * 8 + k];
				printf("%9d | %9d\n", Coli_1[i].address, Coli_1[i].Coil_value);
				i++;
			}
		}
		printf("----------+----------\n");
	}
	memset(buf, 0, sizeof(buf));
	printf("\n*********end function**********\n");
	return TRUE;
}

bool ReadRegisters(int function)      //0x03-0x04 read A0 -A1
{
	if (function == 3)
		printf("\n*********Read Hold Registers function**********\n");
	else if (function == 4)
		printf("\n*********Read Input Registers function**********\n");
	else
		return false;

	const int value = 3;             //debug info
	int address = 107;           //real address - 1
	int ID = 17;

	char buf[128] = { 0 };
	requestSingle request;
	//select output mode -- integer
	int* bus;
	float* test;
	long* rdLng;
	double* rdDbl;

	request_Read(&request, ID, function, address, value);
	assert(nb_read_impl(buf, request));
	int response_lenght = buf[2];

	bus = readInt(buf, response_lenght);
	test = readInverseFloat(buf, response_lenght);  // responce_lenght possible vulnerability
	rdLng = readLong(buf, response_lenght);
	rdDbl = readDouble(buf, response_lenght);
	//show result
	int Newadd = (function == 3) ? (address + 40001) : (address + 30001);
	printf("  address |   Value    \n"
		"----------+----------\n");
	//UINT16 - Big Endian (AB)
	for (int j = 0; j < response_lenght / 2; j++) {
		printf("%9d | %9d\n", Newadd + j, bus[j]);
	}
	printf("----------+----------\n");
	//Float - Little Endian (DCBA)
	for (int i = 0; i < (response_lenght / 4); i++)
		printf("\n%9d | %9.2f\n", Newadd + i, test[i]);
	printf("----------+----------\n");
	//Long - Big Endian (ABCD)
	for (int i = 0, j = 0; j < (response_lenght / 4); i += 4, j++) {
		printf("\n%9d | %9lu\n", Newadd + i, rdLng[j]);
	}
	printf("----------+----------\n");
	//Double
	for (int i = 0; i < (response_lenght / 8); i++)
		printf("\n%9d | %9f\n", Newadd + i, rdDbl[i]);
	//Free memory
	free(rdDbl);
	free(rdLng);
	free(bus);
	free(test);
	memset(buf, 0, sizeof(buf));
	printf("\n*********end function**********\n");
	return TRUE;
}


bool WriteRegisters(int function)      //0x05-0x06 write D0 -A0
{
	if (function == 5)
		printf("\n*********Force Single Coil function**********\n");
	else if (function == 6)
		printf("\n*********Preset Single Registers function**********\n");
	else
		return false;

	int value = 994;             //debug info
	int address = 107;           //real address - 1
	int ID = 17;

	char buf[128] = { 0 };
	requestSingle request;
	//using Read function for Writing 1 byte
	if (function == 5) {
		(value > 1) ? value = 1 : value = 0;
		request_Read(&request, ID, function, address, value * 0xFF);
	}
	else {
		request_Read(&request, ID, function, address, value);
	}

	assert(nb_read_impl(buf, request));
	if (!(ModRTU_CRC(request.Slave_code, 6) == ModRTU_CRC((byte*)buf, 6))) {
		printf("\nattempt write cluster %d set to %d failed.\n", address + 1, value);
		return false;
	}
	printf("\nCluster %d set to %d.\n", address + 1, value);
	printf("\n*********end function**********\n");
	return TRUE;
}

bool OpenPort(/*int baudrate, int bytesize, int parity, int stopbits*/)
{
	hComm = CreateFile(pcComPort,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0, //для Modbus ставить FILE_FLAG_OVERLAPPED // для теста 0
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
		timeouts.ReadTotalTimeoutMultiplier = 10;
		timeouts.ReadTotalTimeoutConstant = 20;
		timeouts.WriteTotalTimeoutMultiplier = 2000;
		timeouts.WriteTotalTimeoutConstant = 1;
		SetCommTimeouts(hComm, &timeouts);
		return true;
	}
	else return false;
}

int main()
{
	if (!OpenPort()) {
		printf("%s opening error\n", pcComPort);
		return 0;
	}
	else {
		assert(ReadStatus(1));
		assert(ReadStatus(2));
		assert(ReadRegisters(3));
		assert(ReadRegisters(4));
		assert(WriteRegisters(5));
		assert(WriteRegisters(6));
	}


	CloseHandle(hComm);
	getchar();
	return 0;
}
