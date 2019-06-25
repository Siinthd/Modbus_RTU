
#include "aSerial.h"

int main()
{
	char num[8];
	char str1[15] = "\\\\.\\COM";
	cin >> num;
	strncat_s(str1, num, 2);
	char str2[] = "666";

	aSerial Comport(str1);
	Comport.send(str2);
	Comport.recieve();

	system("pause");
	return 0;
}