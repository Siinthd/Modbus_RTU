#include "aSerial.h"



aSerial::aSerial(char* str)
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
		printf("GetCommState error\n");
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


aSerial::~aSerial()
{
	if (CloseHandle(hSerial))
		cout << "port closed." << endl;
}

void aSerial::send(char* number)
{
	bool fl; //writing flag
	overlappedwr.hEvent = CreateEvent(NULL, true, true, NULL);
	DWORD signal;
	DWORD dwBytesWritten; // amount written bytes 
	BOOL iRet = WriteFile(hSerial, number, strlen(number), &dwBytesWritten, &overlappedwr);

	signal = WaitForSingleObject(overlappedwr.hEvent, INFINITE);	//pause stream, while begin WriteFile
																	//if success, set fl to '1'
	if ((signal == WAIT_OBJECT_0) && (GetOverlappedResult(hSerial, &overlappedwr, &dwBytesWritten, true))) fl = true;
	else fl = false;

	//cout << endl;
	//cout<<"write: "<<fl<<endl;
}


void aSerial::recieve()
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
							cout << "value" << ":" << sReceivedChar<<endl;
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