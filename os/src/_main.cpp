#include <string>
#include "NTFS.h"
#include "FATRead.h"

int main (int argc, char* argv[])
{
	NTFS *ntfs;
	FATRead *fat;
	std::string fn1, fn2;

	if (argc == 1)
	{
		std::cout << "Enter NTFS partition name: ";
		std::cin >> fn1;
		std::cout << "Enter FAT partition name: ";
		std::cin >> fn2;
	}
	else if (argc == 3)
	{
		fn1 = argv[1];
		fn2 = argv[2];
	}
	else
		return 0;

	ntfs = new NTFS (fn1, fn2);
	ntfs->readMFT (0x05, 0);
	delete ntfs;

	fat = new FATRead (fn2);
	std::cout << "Print content of files?  Please type in '0' or '1',\t";
	std::cin >> fat->doPrint;
	fat->printAll ();
	
	delete fat;
	return 0;
}