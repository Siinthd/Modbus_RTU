
#include "Modbus.h"

int main()
{
	char num[8];
	char str1[15] = "\\\\.\\COM";
	cin >> num;
	strncat_s(str1, num, 2);
	system("pause");
	return 0;
}