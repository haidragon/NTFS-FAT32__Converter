#include "FATWrite.h"
#include <Windows.h>
#include <stdio.h>

FATWrite::FATWrite (const std::string& partitionName)
{
	partition.open (partitionName, std::ios::binary | std::ios::out | std::ios::in);
}

FATWrite::~FATWrite ()
{
	partition.close ();
}

void FATWrite::readBPB ()
{
	partition.read (reinterpret_cast<char*>(&bpb), sizeof (BiosParameterBlock));

	fatOffset = bpb.reservedSectors * bpb.bytesPerSector;
	uint32_t fatSize = bpb.tableSize * bpb.bytesPerSector;
	copyOffset = fatOffset + fatSize;

	dataOffset = fatOffset + fatSize * bpb.fatCopies;
	bytesPerCluster = bpb.sectorsPerCluster * bpb.bytesPerSector;

	entryClusters[1] = bpb.rootCluster;
	offsetOfEntries[1] = 0;
}

int32_t FATWrite::searchCluster ()				// returning first empty cluster number
{
	partition.seekg (fatOffset + (5 * sizeof uint32_t));
	uint32_t value;
	int32_t index = 4;
	do
	{
		partition.read (reinterpret_cast<char *>(&value), sizeof uint32_t);
		index++;
	} while (value != 0x00000000);
	
	return index;
}

void FATWrite::addToDirectoryEntry (const FileName& fName, char* name)
{
	dEntry = {};
	setName (fName, name);
	setClusterEntry (fName);
	setSize (fName);
	setCDateCTime (fName);
	setMDateMTime (fName);
	setLADate (fName);
	setAttributes (fName);
}

void FATWrite::setName (const FileName& fName, char * name)
{
	if (fName.flags == 0x10000000)					// if dir
		for (int i{}; i < fName.filenameLength && i < 8; i++)		// copy name
			dEntry.name[i] = name[i * 2];
	else
	{
		for (int i{}; name[i * 2] != '.' && i < 8; i++)	// copy name
			dEntry.name[i] = name[i * 2];

		for (int i{ fName.filenameLength - 3 }, j{}; i < fName.filenameLength; i++, j++)	// and ext
			dEntry.ext[j] = name[i * 2];
	}
}

void FATWrite::setClusterEntry (const FileName& fName)
{
	int32_t clusterNbr = searchCluster ();						// find first cluster
	dEntry.firstClusterHi = static_cast<uint16_t>((clusterNbr & 0xFFFF0000) >> 16);
	dEntry.firstClusterLow = static_cast<uint16_t>(clusterNbr & 0x0000FFFF);
	writeToFAT (0x0FFFFFFF, clusterNbr);
	clearCluster (clusterNbr);
}

void FATWrite::setSize (const FileName& fName)
{
	dEntry.size = static_cast<uint32_t>(fName.realFileSize);
}

void FATWrite::setCDateCTime (const FileName& fName)
{
	FILETIME ftCreate;
	SYSTEMTIME stUTC;
	ftCreate.dwLowDateTime = DWORD (fName.creationTime & 0x00000000FFFFFFFF);
	ftCreate.dwHighDateTime = DWORD ((fName.creationTime & 0xFFFFFFFF00000000) >> 32);
	FileTimeToSystemTime (&ftCreate, &stUTC);
	//printf ("Created on: %02d/%02d/%d %02d:%02d\n", stUTC.wDay, stUTC.wMonth, stUTC.wYear, stUTC.wHour, stUTC.wMinute);

	dEntry.cDate |= stUTC.wDay & 0x001F;
	dEntry.cDate |= stUTC.wMonth & 0x01E0;
	dEntry.cDate |= stUTC.wYear & 0xFE00;

	dEntry.cTime |= (stUTC.wSecond / 2) & 0x001F;
	dEntry.cTime |= stUTC.wMinute & 0x07E0;
	dEntry.cTime |= stUTC.wHour & 0xF800;
}

void FATWrite::setMDateMTime (const FileName& fName)
{
	FILETIME ftCreate;
	SYSTEMTIME stUTC;
	ftCreate.dwLowDateTime = DWORD (fName.modificationTime & 0x00000000FFFFFFFF);
	ftCreate.dwHighDateTime = DWORD ((fName.modificationTime & 0xFFFFFFFF00000000) >> 32);
	FileTimeToSystemTime (&ftCreate, &stUTC);

	dEntry.wDate |= stUTC.wDay & 0x001F;
	dEntry.wDate |= stUTC.wMonth & 0x01E0;
	dEntry.wDate |= stUTC.wYear & 0xFE00;

	dEntry.wTime |= (stUTC.wSecond / 2) & 0x001F;
	dEntry.wTime |= stUTC.wMinute & 0x07E0;
	dEntry.wTime |= stUTC.wHour & 0xF800;
}

void FATWrite::setLADate (const FileName& fName)
{
	FILETIME ftCreate;
	SYSTEMTIME stUTC;
	ftCreate.dwLowDateTime = DWORD (fName.readTime & 0x00000000FFFFFFFF);
	ftCreate.dwHighDateTime = DWORD ((fName.readTime & 0xFFFFFFFF00000000) >> 32);
	FileTimeToSystemTime (&ftCreate, &stUTC);

	dEntry.aTime |= stUTC.wDay & 0x001F;
	dEntry.aTime |= stUTC.wMonth & 0x01E0;
	dEntry.aTime |= stUTC.wYear & 0xFE00;
}

void FATWrite::setAttributes (const FileName& fName)
{
	// READ_ONLY = 0x01, HIDDEN = 0x02, SYSTEM = 0x04, DIRECTORY = 0x10, ARCHIVE = 0x20

	if (fName.flags & 0x0001)
		dEntry.attributes |= 0x01;
	if (fName.flags & 0x0002)
		dEntry.attributes |= 0x02;
	if (fName.flags & 0x0004)
		dEntry.attributes |= 0x04;
	if (fName.flags & 0x0020)
		dEntry.attributes |= 0x20;
	if (fName.flags & 0x10000000)
		dEntry.attributes |= 0x10;
}

void FATWrite::writeEntry (const uint32_t& depth)
{
	setEntryPointer (depth);
	partition.write (reinterpret_cast<char*>(&dEntry), sizeof dEntry);
}

void FATWrite::writeData (char* buffer, const uint32_t& size, int64_t& fileSize)			// max size of buffer is bytesPerCluster
{
	int32_t clusterNumber;
	if (dEntry.size == fileSize)
		clusterNumber = calculateClusterNumber ();
	else
		clusterNumber = searchCluster ();

	partition.seekg (dataOffset + bytesPerCluster * (clusterNumber - 2));
	partition.write (buffer, size);
	fileSize -= bytesPerCluster;
	
	if (fileSize <= 0)
		writeToFAT (0x0FFFFFFF, clusterNumber);
	else
		writeToFAT (searchCluster (), clusterNumber);
}

void FATWrite::writeToFAT (const uint32_t& value, int32_t& clusterNumber)
{
	uint32_t p = value;
	partition.seekg (fatOffset + clusterNumber * sizeof int32_t);
	partition.write (reinterpret_cast<const char*>(&p), sizeof int32_t);

	partition.seekg (copyOffset + clusterNumber * sizeof int32_t);
	partition.write (reinterpret_cast<const char*>(&p), sizeof int32_t);
}

void FATWrite::setEntryPointer (const uint32_t& parentNumber)
{
	int32_t cluster;
	auto clusterPointer = entryClusters.find (parentNumber);
	
	if (clusterPointer != entryClusters.end ())					// check if exist
		cluster = clusterPointer->second;
	else
	{
		cluster = calculateClusterNumber ();
		writeToFAT (0x0FFFFFFF, cluster);
		entryClusters[parentNumber] = cluster;
		offsetOfEntries[parentNumber] = 0;
	}

	if (offsetOfEntries[parentNumber] + sizeof DirectoryEntry >  bytesPerCluster)
	{
		offsetOfEntries[parentNumber] = 0;
		entryClusters[parentNumber] = searchCluster ();
		writeToFAT (entryClusters[parentNumber], cluster);
		cluster = entryClusters[parentNumber];
	}

	partition.seekg (dataOffset + offsetOfEntries[parentNumber] + bytesPerCluster * (cluster - 2));
	std::cout << offsetOfEntries[parentNumber] << " " << partition.tellg ();

	offsetOfEntries[parentNumber] += sizeof DirectoryEntry;
}

void FATWrite::addToMap (const MFTHeader& mft)
{
	entryClusters[mft.MFTRecordNumber] = calculateClusterNumber ();
	offsetOfEntries[mft.MFTRecordNumber] = 0;
}

uint32_t FATWrite::calculateClusterNumber ()
{
	return (static_cast<uint32_t>(dEntry.firstClusterHi)) << 16 | (static_cast<uint32_t>(dEntry.firstClusterLow));
}

void FATWrite::clearCluster (const int32_t& cluusterNumber)
{
	char empty[512]{};

	partition.seekg (dataOffset + bytesPerCluster * (cluusterNumber - 2));
	for (int i{}; i < bytesPerCluster; i += 512)
		partition.write (empty, 512);
}


int32_t FATWrite::nextCluster ()
{
	int32_t first = searchCluster();
	int32_t next;

	writeToFAT (0x0FFFFFFF, first);
	next = searchCluster ();
	writeToFAT (0x00000000, first);

	return next;
}