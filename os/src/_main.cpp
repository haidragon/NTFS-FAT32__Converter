#include "NTFS.h"
#include "FATRead.h"
#include <map>

int main ()
{
	NTFS *f = new NTFS ("ntfs.img", "emptyfat.img");
	f->readMFT (5, 0);
	delete f;

	FATRead *d = new FATRead ("emptyfat.img");
	d->printAll ();
	delete d;

	return 0;
}