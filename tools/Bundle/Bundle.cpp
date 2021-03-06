/* Copyright (C) 2009 Mobile Sorcery AB

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2, as published by
the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.
*/

// Bundle.cpp : Defines the entry point for the console application.
//

#include "File.h"
#include <vector>
#include <cstring>
#include <stdlib.h>

std::vector<std::string> inFiles;

#define MAX_DATA_SIZE 65536*1024
unsigned char fileData[MAX_DATA_SIZE];
int fileDataPtr = 0;
int startOfData = 0;
FILE *outFile = 0;

#define START_OF_VOLUME_ENTRIES 12


int changeCase = 0; // 1 upper case, 2 lower case	

/* format
struct BundleHeader {
	int magic;
	int startOfVolumes;
	int startOfData;
}

struct VolumeEntryFile {
	byte volumeEntryType; // 0 = directory, 1 = file
	char *name;
	int dataOffset;
	int dataLength;
};

struct VolumeEntryDirectory {
	byte volumeEntryType; // 0 = directory, 1 = file
	char *name;
	int numVolumeEntries;
};
*/
static char to_upper(char c) {

	if(c>='a' && c<='z') 
		return c-'a'+'A';
	else 
		return c;
}

static char to_lower(char c) {
	if(c>='A' && c <='Z') 
		return c-'A'+'a';
	else 
		return c;
}

struct VolumeEntry {
	unsigned char type;
	std::string name;
	std::vector<VolumeEntry*> children;
	int dataOffset;
	int dataLength;
};

VolumeEntry *g_root;

static int readFile(std::string name) {
	FILE *file = fopen(name.c_str(), "rb");
	if(!file) {
		printf("failure reading '%s'\n", name.c_str());
		exit(1);
	}

	fseek(file, 0, SEEK_END);
	int len = ftell(file);
	fseek(file, 0, SEEK_SET);
	int res = fread(&fileData[fileDataPtr], 1, len, file);
	if(res != len) {
		printf("failure reading '%s'\n", name.c_str());
		exit(1);
	}
	fileDataPtr+=len;
	return len;
}

static void saveFileData() {
	startOfData = ftell(outFile);
	fwrite(fileData, 1, fileDataPtr, outFile);
}

static void writeHeader() {
	fseek(outFile, 0, SEEK_SET);
	int magic = 0x12345678;
	fwrite(&magic, 4, 1, outFile);
	int startOfVolumes = START_OF_VOLUME_ENTRIES; // sizeof(BundleHeader);
	fwrite(&startOfVolumes, 4, 1, outFile); 
	fwrite(&startOfData, 4, 1, outFile); 
}

static void saveVolumeEntries(VolumeEntry *root) {
	fwrite(&root->type, 1, 1, outFile);	
	int sizeOfName = root->name.size()+1;
	fwrite(root->name.c_str(), 1, sizeOfName, outFile);
	int numVolumeEntries;

	switch(root->type) {
		case 0:	
			numVolumeEntries = root->children.size();
			fwrite(&numVolumeEntries, 4, 1, outFile);
			for(size_t i = 0; i < root->children.size(); i++) {
				saveVolumeEntries(root->children[i]);
			}
			break;
		case 1:
			int dataOffset = root->dataOffset;
			fwrite(&dataOffset, 4, 1, outFile);
			int dataLength = root->dataLength;
			fwrite(&dataLength, 4, 1, outFile);
			break;
	}	
}

void parse(File file, VolumeEntry *vol);

static void parseDirectory(File file, VolumeEntry *vol)  
{
	std::list<File> l = file.listFiles( );
	
	// Go through file and directories	
	for ( std::list<File>::iterator it = l.begin( ); it != l.end( ); ++it )
	{
		// Skip self & backward references
		if ( it->isSelfOrBackRef( ) == true )
			continue;
		
		VolumeEntry *childVol = new VolumeEntry;
		vol->children.push_back(childVol);
		parse( *it, childVol );		
	}
	
	if(file.isDirectory()) {
		printf("- %s\n", file.getName( ).c_str( ));
	}
}

void parse(File file, VolumeEntry *vol) {
	if ( file.isDirectory( ) == true ) 
	{
		vol->name = file.getName();

		for(size_t i = 0; i < vol->name.size(); i++) 
		{
			if ( changeCase == 1 ) 
				vol->name[i] = to_upper( vol->name[i] );
			else if(changeCase == 2) 
				vol->name[i] = to_lower( vol->name[i] );
		}

		printf("+ %s\n", vol->name.c_str());

		vol->type = 0;
		parseDirectory(file, vol);
	}
	else 
	{
		//printf("\"%s\"\n", file.getName().c_str());		
		vol->name = file.getName();

		for(size_t i = 0; i < vol->name.size(); i++) {
			if ( changeCase == 1 ) 
				vol->name[i] = to_upper( vol->name[i] );
			else if ( changeCase == 2 ) 
				vol->name[i] = to_lower( vol->name[i] );
		}

		printf("++ %s\n", vol->name.c_str());

		vol->type = 1;
		vol->dataOffset = fileDataPtr;
		vol->dataLength = readFile(file.getAbsolutePath().c_str());
		return;
	}
}



int main(int argc, char **argv)
{
	if(argc<2) {
		printf(	"MAUtil::MAFS Bundle tool\n\n"
				"This tool is used to build a binary image of a folder on a desktop computer.\n\n"
				"Usage:\n"
				"bundle <parameters>\n\n"
				"Parameters:\n"
				"  -in <input file or folder> the input files or folders to add to the\n" 
				"                             image (multiple -in directives may be added).\n"
				"  -out <output file>         the name of the image to be created (only one).\n"
				"  -toUpper/-toLower          change case of all file names to upper or lower\n"
				"                             case.\n\n"
				"Example:\n"
				"  bundle -in data -out anotherworld.bun -toLower\n"
				);
	}

	for(int i = 1; i < argc; i++) {
		if(strcmp(argv[i], "-in")==0) {
			i++;
			if(i>=argc) {
				printf("not enough parameters for -in");			
				return 1;
			}
			inFiles.push_back(argv[i]);
		}
		else if(strcmp(argv[i], "-out")==0) {
			if(outFile) {
				printf("cannot have multiple out files.");
				return 1;
			}

			i++;
			if(i>=argc) {
				printf("not enough parameters for -out");			
				return 1;
			}
			outFile = fopen(argv[i], "wb");
		}
		else if(strcmp(argv[i], "-toUpper")==0) {
			changeCase = 1;
		}
		else if(strcmp(argv[i], "-toLower")==0) {
			changeCase = 2;
		} else {
			printf("invalid argument");
			return 1;
		}
	}
	if(inFiles.size()==0) return 1;
	if(!outFile) return 1;

	VolumeEntry* root = new VolumeEntry;
	root->name = "Root";
	root->type = 0; // directory

	for(size_t i = 0; i < inFiles.size(); i++) {
		File file(inFiles[i]);
		if(file.isDirectory()) {
			parseDirectory(file, root);
		} else {
			VolumeEntry *child = new VolumeEntry;
			root->children.push_back(child);
			parse(file, child);
		}
	}

	fseek(outFile, START_OF_VOLUME_ENTRIES, SEEK_SET);
	saveVolumeEntries(root);
	saveFileData();
	writeHeader();
	fclose(outFile);
	
	return 0;
}
