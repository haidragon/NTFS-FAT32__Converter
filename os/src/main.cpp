#include "Converter.h"

int main ()
{
	Converter *f = new Converter ("ntfs.img");		// \\\\.\\I:
	f->readPartitionBootSector ();
	f->getMFTChain ();
	f->readMFT (67);

	return 0;
}