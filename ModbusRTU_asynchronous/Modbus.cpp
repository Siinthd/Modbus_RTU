#include "Modbus.h"



Modbus::Modbus(char* str)
{
	LPCTSTR sPortName = str;

	hSerial = CreateFile(sPortName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

	dcb.DCBlength = sizeof(DCB);
	if (!GetCommState(hSerial, &dcb)) {
		cout << "getting state error\n";
		system("pause");
		exit(0);
	}
	//COM Settings
	dcb.BaudRate = CBR_9600;
	dcb.ByteSize = 8;
	dcb.StopBits = ONESTOPBIT;
	dcb.Parity = NOPARITY;
	if (!SetCommState(hSerial, &dcb)) {
		cout << "error setting serial port state\n";
		system("pause");
		exit(0);
	}

	if (!GetCommTimeouts(hSerial, &timeouts))
	{
		cout << "GetCommState error\n" << endl;
		system("pause");
		exit(0);
	}
	double dblBitsPerByte = 1 + dcb.ByteSize + dcb.StopBits + (dcb.Parity ? 1 : 0);
	timeouts.ReadIntervalTimeout = (DWORD)ceil((3.5f*dblBitsPerByte / (double)dcb.BaudRate * 1000.0f));
	timeouts.ReadTotalTimeoutMultiplier = 10;
	timeouts.ReadTotalTimeoutConstant = 20;
	timeouts.WriteTotalTimeoutMultiplier = 2000;
	timeouts.WriteTotalTimeoutConstant = 1;
	SetCommTimeouts(hSerial, &timeouts);

	cout << "Port:" << sPortName << " open." << endl;
}

bool Modbus::ReadRegisters()
{
	return false;
}

bool Modbus::WriteRegisters()
{
	return false;
}

bool Modbus::ForceMuiltipleReg()
{
	return false;
}

uint16_t Modbus::ModRTU_CRC(byte* buf, int len)
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

bool Modbus::CRC_Check(byte* buf, int bytesRead)
{
	unsigned int source;
	unsigned int CRC = ModRTU_CRC(buf, bytesRead - 2);
	source = (byte)buf[bytesRead - 1] | (byte)buf[bytesRead - 2] << 8;
	if (source == CRC) {
		printf("CRC check!\n");
		return 1;
	}
	else {
		printf("wrong CRC (%02X vs %02X)!\n", source, CRC);
		return 0;
	}
}


Modbus::~Modbus()		//if didn't call close()
{
	if (hSerial != INVALID_HANDLE_VALUE)
		this->close();
}


void Modbus::close()
{
	if (CloseHandle(hSerial))
		cout << "port closed." << endl;
}


void Modbus::send(requestSingle request)
{
	bool fl; //writing flag
	overlappedwr.hEvent = CreateEvent(NULL, true, true, NULL);
	DWORD signal;
	DWORD dwBytesWritten; // amount written bytes 
	BOOL iRet = WriteFile(hSerial, &request, sizeof(request), &dwBytesWritten, &overlappedwr);

	signal = WaitForSingleObject(overlappedwr.hEvent, INFINITE);	//pause stream, while begin WriteFile
																	//if success, set fl to '1'
	if ((signal == WAIT_OBJECT_0) && (GetOverlappedResult(hSerial, &overlappedwr, &dwBytesWritten, true))) fl = true;
	else fl = false;
}


void Modbus::recieve()
{
	DWORD btr, temp, mask, signal;
	bool event = false;
	char sReceivedChar[255] = { 0 };

	overlapped.hEvent = CreateEvent(NULL, true, true, NULL);  //creat event and mask
	SetCommMask(hSerial, EV_RXCHAR);
	while (!event)
	{
		WaitCommEvent(hSerial, &mask, &overlapped);			//wait byte

		signal = WaitForSingleObject(overlapped.hEvent, WAIT_COM);
		if (signal == WAIT_OBJECT_0)
		{
			if (GetOverlappedResult(hSerial, &overlapped, &temp, true))

				if ((mask&EV_RXCHAR) != 0)
				{
					ClearCommError(hSerial, &temp, &comstat);			//fill Comsat
					btr = comstat.cbInQue;
					if (btr)
					{
						ReadFile(hSerial, &sReceivedChar, btr, &temp, &overlapped); // get 1 byte
						if (temp > 0)
						{
							cout << "bytes: " << temp << endl;
							cout << "value" << ":" << sReceivedChar << endl;
							event = true;
						}
					}
				}
		}
		else
		{
			cout << "time out.Programm will close" << endl;
			system("pause");
			exit(0);
		}
	}
	CloseHandle(overlapped.hEvent);
}


template <typename T>
bool Modbus::nb_read_impl(char* buf, T request)
{
	DWORD bytesRead, dwEventMask, bytesWritten, temp;
	PurgeComm(hSerial, PURGE_RXCLEAR | PURGE_TXCLEAR);
	//test loop
	if (!WriteFile(hSerial, &request, sizeof(request), &bytesWritten, NULL)) {
		perror("error: ");
		return FALSE;
	}
	printPackage(&request, bytesWritten, 0);
	send(request);


	if (bytesRead) {
		recieve();
		printPackage(buf, bytesRead, 1);
		CRC_Check((byte*)buf, (int)bytesRead);  //make assert
	}
	ModbussErrorCheck((byte*)buf, request.Slave_code[1]);

	return TRUE;
}

bool Modbus::ModbussErrorCheck(byte* buffer, byte function)
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

//bool Modbus::ReadRegisters(int function)      //0x03-0x04 read A0 -A1
//{
//	if (function == 3)
//		printf("\n*********Read Hold Registers function**********\n");
//	else if (function == 4)
//		printf("\n*********Read Input Registers function**********\n");
//	else
//		return false;
//
//	const int value = 3;             //debug info
//	int address = 107;           //real address - 1
//	int ID = 17;
//
//	char buf[128] = { 0 };
//	requestSingle request;
//	//select output mode -- integer
//	int* bus;
//	float* test;
//	long* rdLng;
//	double* rdDbl;
//
//	request_Read(&request, ID, function, address, value);
//	assert(nb_read_impl(buf, request));
//	int response_lenght = buf[2];
//
//	bus = readInt(buf, response_lenght);
//	test = readInverseFloat(buf, response_lenght);  // responce_lenght possible vulnerability
//	rdLng = readLong(buf, response_lenght);
//	rdDbl = readDouble(buf, response_lenght);
//	//show result
//	int Newadd = (function == 3) ? (address + 40001) : (address + 30001);
//	printf("  address |   Value    \n"
//		"----------+----------\n");
//	//UINT16 - Big Endian (AB)
//	for (int j = 0; j < response_lenght / 2; j++) {
//		printf("%9d | %9d\n", Newadd + j, bus[j]);
//	}
//	printf("----------+----------\n");
//	//Float - Little Endian (DCBA)
//	for (int i = 0; i < (response_lenght / 4); i++)
//		printf("\n%9d | %9.2f\n", Newadd + i, test[i]);
//	printf("----------+----------\n");
//	//Long - Big Endian (ABCD)
//	for (int i = 0, j = 0; j < (response_lenght / 4); i += 4, j++) {
//		printf("\n%9d | %9lu\n", Newadd + i, rdLng[j]);
//	}
//	printf("----------+----------\n");
//	//Double
//	for (int i = 0; i < (response_lenght / 8); i++)
//		printf("\n%9d | %9f\n", Newadd + i, rdDbl[i]);
//	//Free memory
//	free(rdDbl);
//	free(rdLng);
//	free(bus);
//	free(test);
//	memset(buf, 0, sizeof(buf));
//	printf("\n*********end function**********\n");
//	return TRUE;
//}
//
//
//bool Modbus::WriteRegisters(int function)      //0x05-0x06 write D0 -A0
//{
//	if (function == 5)
//		printf("\n*********Force Single Coil function**********\n");
//	else if (function == 6)
//		printf("\n*********Preset Single Registers function**********\n");
//	else
//		return false;
//
//	int value = 994;             //debug info
//	int address = 107;           //real address - 1
//	int ID = 17;
//
//	char buf[128] = { 0 };
//	requestSingle request;
//	//using Read function for Writing 1 byte
//	if (function == 5) {
//		(value > 1) ? value = 1 : value = 0;
//		request_Read(&request, ID, function, address, value * 0xFF);
//	}
//	else {
//		request_Read(&request, ID, function, address, value);
//	}
//
//	assert(nb_read_impl(buf, request));
//	if (!(ModRTU_CRC(request.Slave_code, 6) == ModRTU_CRC((byte*)buf, 6))) {
//		printf("\nattempt write cluster %d set to %d failed.\n", address + 1, value);
//		return false;
//	}
//	printf("\nCluster %d set to %d.\n", address + 1, value);
//	printf("\n*********end function**********\n");
//	return TRUE;
//}
//
//bool Modbus::ForceMuiltipleReg(int function)    //0x0F-0x10 Write multiple D&A
//{
//	char buf[128] = { 0 };
//	byte data[100];
//	int* to_write;                  //Parcing string to array of int
//	int parcel;                    //amount bytes to packet
//	int bytestowrt;                //amount of sending bytes
//
//	char str[] = "1,0,0,1,0,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,1,0,0,1,0,1,0,1,0,1,0,1";
//	char str2[] = "255,38655,20050,11005,50001,13";
//	//may be?
//	switch (function)
//	{
//	case 0X0F:
//		printf("\n*********Force multiple Registers**********\n");
//		to_write = convertUnionFromString(str, 1, &parcel);
//		bytestowrt = request_Write(data, 1, 0x0F, 100, parcel, to_write);
//		assert(nb_read_impl(buf, data, bytestowrt));
//		break;
//	case 0X10:
//		printf("\n*********Preset multiple Registers**********\n");
//		to_write = convertUnionFromString(str2, 0, &parcel);
//		bytestowrt = request_Write(data, 1, 0x10, 255, parcel, to_write);
//		assert(nb_read_impl(buf, data, bytestowrt));
//		break;
//	default:
//		return FALSE;
//	}
//	printf("\n*********end function**********\n");
//	return TRUE;
//}


template<typename T>
inline void Modbus::printPackage(T * data, int size, int isin)
{
	printf("%s bytes: %d\n\r\t", (isin) ? "Received" : "Sent", size);
	for (int i = 0; i < size; i++)
		printf("%02X ", data->Slave_code[i]);
	printf("\n\r");
}