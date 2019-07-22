#pragma once

#include <windows.h>
#include <iostream>
#include <cstdlib>
#include <vector>

#define WAIT_COM 7000

using namespace std;



class Modbus
{
public:

	struct requestSingle {
		byte Slave_code[8];
	};

	Modbus(char* str);
	~Modbus();


	template <typename T>
	void printPackage(T* data, int size, int isin);

	template <typename T>
	bool nb_read_impl(char * buf, T request);

	void send(requestSingle);
	void recieve();
	void close();
	bool ModbussErrorCheck(byte * buffer, byte function);
	
	bool ReadRegisters();
	bool WriteRegisters();
	bool ForceMuiltipleReg();

	uint16_t ModRTU_CRC(byte * buf, int len);

	bool CRC_Check(byte * buf, int bytesRead);


private:
	DCB dcb;
	HANDLE hSerial;
	COMSTAT comstat;
	COMMTIMEOUTS timeouts;
	OVERLAPPED overlapped;
	OVERLAPPED overlappedwr;


};


