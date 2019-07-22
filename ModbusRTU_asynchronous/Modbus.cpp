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

bool Modbus::send()
{
	overlappedwr.hEvent = CreateEvent(NULL, true, true, NULL);
	BOOL iRet = WriteFile(hSerial, &pack, sizeof(pack), &dwBytesWritten, &overlappedwr);

	signal = WaitForSingleObject(overlappedwr.hEvent, INFINITE);	//pause stream, while begin WriteFile
																	//if success, set fl to '1'
	if ((signal == WAIT_OBJECT_0) && (GetOverlappedResult(hSerial, &overlappedwr, &dwBytesWritten, true))) 
		return true;
	else 
		return false;
}

bool Modbus::recieve()
{
	
	bool event = false;
	memset(sReceivedChar, 0, 255 * (sizeof sReceivedChar[0]));

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
					bytesRead = comstat.cbInQue;
					if (bytesRead)
					{
						ReadFile(hSerial, &sReceivedChar, bytesRead, &temp, &overlapped); // get  byte
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
			CloseHandle(overlapped.hEvent);
			return 0;
		}
	}
	CloseHandle(overlapped.hEvent);
	return 1;
}


bool Modbus::nb_read_impl()
{
	
	PurgeComm(hSerial, PURGE_RXCLEAR | PURGE_TXCLEAR);
	//test loop
	if (!send()) {
		perror("error: ");
		return FALSE;
	}

	printPackage(this->pack, dwBytesWritten, 0);

	if (!recieve()) {
		perror("error: ");
		return FALSE;
	}
	printPackage(sReceivedChar, bytesRead, 1);
	CRC_Check((byte*)sReceivedChar, (int)bytesRead);  
	ModbussErrorCheck((byte*)sReceivedChar, this->pack.Slave_code[1]);

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

bool Modbus::ReadRegisters(int function)      //0x03-0x04 read A0 -A1
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

	
	//select output mode
	int* bus;
	float* test;
	long* rdLng;
	double* rdDbl;

	request_Read(ID, function, address, value);
	
	if (nb_read_impl())
		int response_lenght = sReceivedChar[2];

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



bool Modbus::WriteRegisters(int function)      //0x05-0x06 write D0 -A0
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

	//using Read function for Writing 1 byte
	if (function == 5) {
		(value > 1) ? value = 1 : value = 0;
		request_Read(ID, function, address, value * 0xFF);
	}
	else {
		request_Read(ID, function, address, value);
	}

	
	if (!nb_read_impl() || !(ModRTU_CRC(pack.Slave_code, 6) == ModRTU_CRC((byte*)sReceivedChar, 6))) {
		printf("\nattempt write cluster %d set to %d failed.\n", address + 1, value);
		return false;
	}
	printf("\nCluster %d set to %d.\n", address + 1, value);
	printf("\n*********end function**********\n");
	return TRUE;
}



void Modbus::printPackage(requestSingle data, int size, int isin)
{
	printf("%s bytes: %d\n\r\t", (isin) ? "Received" : "Sent", size);
	for (int i = 0; i < size; i++)
		printf("%02X ", data.Slave_code[i]);
	printf("\n\r");
}

void Modbus::printPackage(char data[], int size, int isin)
{
	printf("%s bytes: %d\n\r\t", (isin) ? "Received" : "Sent", size);
	for (int i = 0; i < size; i++)
		printf("%02X ", (byte)data[i]);
	printf("\n\r");
}


void Modbus::request_Read(int ID, int function, int address, int value)
{
	//адрес устройства
	pack.Slave_code[0] = ID;
	//функциональный код
	pack.Slave_code[1] = function;
	//адрес первого регистра HI-Lo //2 байта
	pack.Slave_code[2] = address >> 8;
	pack.Slave_code[3] = address & 0x00ff;
	//количество регистров Hi-Lo //2 байта
	pack.Slave_code[4] = value >> 8;
	pack.Slave_code[5] = value & 0x00ff;
	//CRC
	unsigned int CRC = ModRTU_CRC(pack.Slave_code, 6); //2 байта

	pack.Slave_code[6] = CRC >> 8;
	pack.Slave_code[7] = CRC;
}

int Modbus::request_Write(byte* send, int ID, int function, int address, int bytes, int* value)
{
	//адрес устройства
	send[0] = ID;
	//функциональный код
	send[1] = function;
	//адрес первого регистра HI-Lo //2 байта
	send[2] = address >> 8;
	send[3] = address & 0x00ff;
	send[4] = bytes * 2;
	int i = 5;
	//количество регистров Hi-Lo //2 байта
	for (int k = 0; k < bytes; k++, i += 2)
	{
		send[i] = value[k] >> 8;
		send[i + 1] = value[k] & 0x00ff;
	}
	//CRC
	unsigned int CRC = ModRTU_CRC(send, i); //2 байта
	send[i + 1] = CRC;
	send[i] = CRC >> 8;
	return i + 2;
}

int* Modbus::readInt(char* buf, int response_lenght)   //Convert to INT 2 bytes
{
	int *intarray = (int*)malloc((response_lenght) / 2 * sizeof(int));
	for (int i = 3, j = 0; j < (response_lenght) / 2; i += 2, j++)
	{
		intarray[j] = ((byte)buf[i] << 8) | (byte)buf[i + 1];
	}
	return intarray;
}