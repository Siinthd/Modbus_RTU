HANDLE hSerial;
COMSTAT comstat;
OVERLAPPED overlapped;
OVERLAPPED overlappedwr;
#define WAIT_COM 7000

void initializeCom(char *str)
{
	LPCTSTR sPortName = str;
	hSerial = CreateFile(sPortName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

	DCB dcb;
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
	cout << "Port:" << str << " open." << endl;
}

int ReadCOM() {

	DWORD btr, temp, mask, signal;
	bool event = false;
	char sReceivedChar;

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
							cout << "value" << ":" << sReceivedChar;
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
	return (int)sReceivedChar;
}

void WriteCom(char number)
{

	DWORD dwSize = sizeof(number); // size of string 
	bool fl; //writing flag
	overlappedwr.hEvent = CreateEvent(NULL, true, true, NULL);
	DWORD signal;
	DWORD dwBytesWritten; // amount written bytes 
	BOOL iRet = WriteFile(hSerial, &number, dwSize, &dwBytesWritten, &overlappedwr);

	signal = WaitForSingleObject(overlappedwr.hEvent, INFINITE);	//pause stream, while begin WriteFile
																	//if success, set fl to '1'
	if ((signal == WAIT_OBJECT_0) && (GetOverlappedResult(hSerial, &overlappedwr, &dwBytesWritten, true))) fl = true;
	else fl = false;

	//cout << endl;
	//cout<<"write: "<<fl<<endl;
}

void ClosePort()
{
	CloseHandle(hSerial);
}