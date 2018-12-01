#include "Converter.h"
#include "FATRead.h"
#include <map>

int main ()
{
	FATRead *d = new FATRead ("fatfinal.img");
	d->readBPB ();
	d->printAll ();
	delete d;

	NTFS *f = new NTFS ("ntfs.img", "fatfinal.img");
	f->readPartitionBootSector ();
	f->getMFTChain ();
	f->readMFT (5, 0);
	delete f;


	return 0;
}