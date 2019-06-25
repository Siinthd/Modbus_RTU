#pragma once

#include <windows.h>
#include <iostream>
#include <cstdlib>


#define WAIT_COM 7000

using namespace std;

class aSerial
{
public:
	aSerial(char* str);
	~aSerial();
	void send(char*);
	void recieve();

private:
	DCB dcb;

	HANDLE hSerial;
	COMSTAT comstat;
	COMMTIMEOUTS timeouts;
	OVERLAPPED overlapped;
	OVERLAPPED overlappedwr;
};

