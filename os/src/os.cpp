#include "Fat.h"

int main ()
{
	Fat *f = new Fat ("\\\\.\\I:");
	f->readPartitionBootSector ();

	return 0;
}