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

	void printPackage(requestSingle, int, int);

	void printPackage(char data[], int, int);

	void request_Read(int ID, int function, int address, int value);

	int * readInt(char * buf, int response_lenght);

	float * readInverseFloat(char * buf, int response_lenght);

	double * readDouble(char * buf, int response_lenght);

	long * readLong(char * buf, int response_lenght);

	bool nb_read_impl();

	bool send();

	bool recieve();

	void close();

	bool ModbussErrorCheck(byte * buffer, byte function);

	bool ReadRegisters(int function);

	bool WriteRegisters(int function);

	uint16_t ModRTU_CRC(byte * buf, int len);

	bool CRC_Check(byte * buf, int bytesRead);


private:
	DCB dcb;
	HANDLE hSerial;
	DWORD btr, temp, mask, signal;
	DWORD bytesRead, dwEventMask;
	DWORD dwBytesWritten; // amount written bytes 
	COMSTAT comstat;
	COMMTIMEOUTS timeouts;
	OVERLAPPED overlapped;
	OVERLAPPED overlappedwr;
	requestSingle pack;
	char sReceivedChar[255];


	int* bus;
	float* test;
	long* rdLng;
	double* rdDbl;


};


