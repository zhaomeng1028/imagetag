/*
* File:	exif.cpp
* Purpose:	cpp EXIF reader
* 16/Mar/2003 <ing.davide.pizzolato@libero.it>
* based on jhead-1.8 by Matthias Wandel <mwandel(at)rim(dot)net>
*/

#include "exif.h"


using namespace std;

//#define PRINT_LOG


#ifdef PRINT_LOG

#ifndef LINUX
#include<process.h>
#include<windows.h>
#else
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/unistd.h>
#include <errno.h>

#endif
#endif

#define _Check

// Allen add the pointer validate function ,check the address
inline int validatePointer(unsigned char *start_pointer,int nLength, unsigned char *Cur_pointer,int Offset)
{
	if(Cur_pointer + Offset  >= (start_pointer+nLength)  )
		return 0; 
	return 1;
}
inline int validatePointer_check(unsigned char *start_pointer,unsigned char *end_pointer,unsigned char *Cur_pointer)
{
	 if(Cur_pointer <= end_pointer &&  Cur_pointer >= start_pointer )
		  return 1;
	 return 0;
}
////////////////////////////////////////////////////////////////////////////////
Cexif::Cexif(EXIFINFO* info)
{
	if (info) {
		m_exifinfo = info;
		freeinfo = false;
	} else {
		m_exifinfo = new EXIFINFO;
		memset(m_exifinfo,0,sizeof(EXIFINFO));
		freeinfo = true;
	}

	m_szLastError[0]='\0';
	ExifImageWidth = MotorolaOrder = 0;
	SectionsRead=0;
	memset(&Sections, 0, MAX_SECTIONS * sizeof(Section_t));

	m_pstart_pointer = NULL;
	m_pBegin = NULL;
	m_pEnd = NULL;
}
////////////////////////////////////////////////////////////////////////////////
Cexif::~Cexif()
{
	for(int i=0;i<MAX_SECTIONS;i++) if(Sections[i].Data) free(Sections[i].Data);
	if (freeinfo) 
	{
		delete m_exifinfo;
		m_exifinfo = NULL;
	}
}
////////////////////////////////////////////////////////////////////////////////
bool Cexif::DecodeExif(FILE * hFile)
{
	int nres = 0;
	int a;
	int HaveCom = 0;

	a = fgetc(hFile);

	if (a != 0xff || fgetc(hFile) != M_SOI){
		return 0;
	}

	for(;;){
		int itemlen;
		int marker = 0;
		int ll,lh, got;
		unsigned char * Data;

		if (SectionsRead >= MAX_SECTIONS){
			strcpy(m_szLastError,"Too many sections in jpg file");
			return 0;
		}

		for (a=0;a<7;a++){
			marker = fgetc(hFile);
			if (marker != 0xff) break;

			if (a >= 6){
				printf("too many padding unsigned chars\n");
				return 0;
			}
		}

		if (marker == 0xff){
			// 0xff is legal padding, but if we get that many, something's wrong.
			strcpy(m_szLastError,"too many padding unsigned chars!");
			return 0;
		}

		Sections[SectionsRead].Type = marker;

		// Read the length of the section.
		lh = fgetc(hFile);
		ll = fgetc(hFile);

		itemlen = (lh << 8) | ll;

		if (itemlen < 2){
			strcpy(m_szLastError,"invalid marker");
			return 0;
		}

		Sections[SectionsRead].Size = itemlen;

		Data = (unsigned char *)malloc(itemlen);
		if (Data == NULL){
			strcpy(m_szLastError,"Could not allocate memory");
			return 0;
		}
// add  
		m_pBegin = Data;
		m_pEnd = Data + itemlen - 1;
//   

		Sections[SectionsRead].Data = Data;

		// Store first two pre-read unsigned chars.
		Data[0] = (unsigned char)lh;
		Data[1] = (unsigned char)ll;

		got = fread(Data+2, 1, itemlen-2,hFile); // Read the whole section.
		if (got != itemlen-2){
			strcpy(m_szLastError,"Premature end of file?");
			return 0;
		}
		SectionsRead += 1;

		switch(marker){

		case M_SOS:   // stop before hitting compressed data 
			// If reading entire image is requested, read the rest of the data.
			/*if (ReadMode & READ_IMAGE){
			int cp, ep, size;
			// Determine how much file is left.
			cp = ftell(infile);
			fseek(infile, 0, SEEK_END);
			ep = ftell(infile);
			fseek(infile, cp, SEEK_SET);

			size = ep-cp;
			Data = (uchar *)malloc(size);
			if (Data == NULL){
			strcpy(m_szLastError,"could not allocate data for entire image");
			return 0;
			}

			got = fread(Data, 1, size, infile);
			if (got != size){
			strcpy(m_szLastError,"could not read the rest of the image");
			return 0;
			}

			Sections[SectionsRead].Data = Data;
			Sections[SectionsRead].Size = size;
			Sections[SectionsRead].Type = PSEUDO_IMAGE_MARKER;
			SectionsRead ++;
			HaveAll = 1;
			}*/
			return 1;

		case M_EOI:   // in case it's a tables-only JPEG stream
			printf("No image in jpeg!\n");
			return 0;

		case M_COM: // Comment section
			if (HaveCom){
				// Discard this section.
				free(Sections[--SectionsRead].Data);
				Sections[SectionsRead].Data=0;
			}else{
				process_COM(Data, itemlen);
				HaveCom = 1;
			}
			break;

		case M_JFIF:
			// Regular jpegs always have this tag, exif images have the exif
			// marker instead, althogh ACDsee will write images with both markers.
			// this program will re-create this marker on absence of exif marker.
			// hence no need to keep the copy from the file.
			free(Sections[--SectionsRead].Data);
			Sections[SectionsRead].Data=0;
			break;

		case M_EXIF:
			// Seen files from some 'U-lead' software with Vivitar scanner
			// that uses marker 31 for non exif stuff.  Thus make sure 
			// it says 'Exif' in the section before treating it as exif.
			if (memcmp(Data+2, "Exif", 4) == 0)
			{
				// wendi modify
				m_exifinfo->IsExif = process_EXIF((unsigned char *)Data+2, itemlen-2);
			}
			else
			{
				// Discard this section.
				free(Sections[--SectionsRead].Data);
				Sections[SectionsRead].Data=0;
			}
			break;

		case M_SOF0: 
		case M_SOF1: 
		case M_SOF2: 
		case M_SOF3: 
		case M_SOF5: 
		case M_SOF6: 
		case M_SOF7: 
		case M_SOF9: 
		case M_SOF10:
		case M_SOF11:
		case M_SOF13:
		case M_SOF14:
		case M_SOF15:

			process_SOFn(Data, marker);

			break;
		default:
			// Skip any other sections.
			//if (ShowTags) printf("Jpeg section marker 0x%02x size %d\n",marker, itemlen);
			break;
		}
	}
	return 1;
}

bool Cexif::DecodeExif(const unsigned char *pExifBuf,int nLen,ofstream &ofs_log)
{
	m_pstart_pointer = (unsigned char *)pExifBuf;
	m_nLength = nLen;

	int nres;

#ifdef PRINT_LOG
	ofs_log<<"DecodeExif step into 1"<<endl;
#endif
	int index_k=0 ;
	int a;
	int HaveCom = 0;

	if (!validatePointer(m_pstart_pointer,m_nLength, (unsigned char *)pExifBuf,index_k))
		return 0;
	a = pExifBuf[index_k++];
#ifdef PRINT_LOG
	ofs_log<<"DecodeExif step into 2  a="<<a<<"    index_k="<<index_k<<endl;
#endif
	if (!validatePointer(m_pstart_pointer,m_nLength, (unsigned char *)pExifBuf,index_k))
		return 0;
	if (a != 0xff || pExifBuf[index_k++] != M_SOI)
	{
#ifdef PRINT_LOG
	ofs_log<<"DecodeExif unormal return"<<endl;
#endif
		return 0;
	}
#ifdef PRINT_LOG
	ofs_log<<"DecodeExif step into 3"<<endl;
#endif
	for(;;)
	{
		int itemlen;
		int marker = 0;
		int ll,lh, got;
		unsigned char * Data;

		if (SectionsRead >= MAX_SECTIONS)
		{
			strcpy(m_szLastError,"Too many sections in jpg file");
			return 0;
		}

		for (a=0;a<7;a++){
			if (!validatePointer(m_pstart_pointer,m_nLength, (unsigned char *)pExifBuf,index_k))
				return 0;
			marker = pExifBuf[index_k++];
			if (marker != 0xff) break;

			if (a >= 6 || index_k >= nLen){
				printf("too many padding unsigned chars\n");
				return 0;
			}
		}
		if (marker == 0xff){
			// 0xff is legal padding, but if we get that many, something's wrong.
			strcpy(m_szLastError,"too many padding unsigned chars!");
			return 0;
		}
		Sections[SectionsRead].Type = marker;
		// Read the length of the section.
		if (!validatePointer(m_pstart_pointer,m_nLength, (unsigned char *)pExifBuf,index_k))
			return 0;
		lh = pExifBuf[index_k++];
		if (!validatePointer(m_pstart_pointer,m_nLength, (unsigned char *)pExifBuf,index_k))
			return 0;
		ll = pExifBuf[index_k++];
#ifdef PRINT_LOG
		ofs_log<<"exif lh="<<lh<<"exif ll="<<ll<<endl;
#endif
		itemlen = (lh << 8) | ll;

		if (itemlen < 2 || itemlen >= nLen){
			strcpy(m_szLastError,"invalid marker");
			return 0;
		}

		Sections[SectionsRead].Size = itemlen;

		Data = (unsigned char *)malloc(itemlen);
		if (Data == NULL){
			strcpy(m_szLastError,"Could not allocate memory");
			return 0;
		}
		Sections[SectionsRead].Data = Data;

		// Store first two pre-read unsigned chars.
		Data[0] = (unsigned char)lh;
		Data[1] = (unsigned char)ll;

	    if ((index_k+itemlen-2)>=nLen)
        {
            free(Sections[SectionsRead].Data);
			Sections[SectionsRead].Data=0;
            return 0;
        }
// add
		m_pBegin = Data;
		m_pEnd = Data + itemlen - 1;
// add

		memcpy(Data+2,pExifBuf+index_k,itemlen-2);
		index_k +=(itemlen-2);
		//got = fread(Data+2, 1, itemlen-2,hFile); // Read the whole section.


		SectionsRead += 1;

		switch(marker){

		case M_SOS:   // stop before hitting compressed data 
			// If reading entire image is requested, read the rest of the data.
			return 1;

		case M_EOI:   // in case it's a tables-only JPEG stream
			printf("No image in jpeg!\n");
			return 0;

		case M_COM: // Comment section
			if (HaveCom){
				// Discard this section.
				free(Sections[--SectionsRead].Data);
				Sections[SectionsRead].Data=0;
			}
			else
			{
#ifdef PRINT_LOG
				ofs_log<<"process_COM aaa"<<endl;
#endif
				process_COM(Data, itemlen);
#ifdef PRINT_LOG
				ofs_log<<"process_COM sss"<<endl;
#endif
				HaveCom = 1;
			}
			break;

		case M_JFIF:
			// Regular jpegs always have this tag, exif images have the exif
			// marker instead, althogh ACDsee will write images with both markers.
			// this program will re-create this marker on absence of exif marker.
			// hence no need to keep the copy from the file.
			free(Sections[--SectionsRead].Data);
			Sections[SectionsRead].Data=0;
			break;

		case M_EXIF:
			// Seen files from some 'U-lead' software with Vivitar scanner
			// that uses marker 31 for non exif stuff.  Thus make sure 
			// it says 'Exif' in the section before treating it as exif.
			if (memcmp(Data+2, "Exif", 4) == 0)
			{
#ifdef PRINT_LOG
				ofs_log<<"process_EXIF aaa"<<endl;
#endif
//wendy modify itemlen to itemlen - 2 and the begin address
				m_address = (long)(Data + 2); 

				m_exifinfo->IsExif = process_EXIF((unsigned char *)Data+2, itemlen - 2);

#ifdef PRINT_LOG
				ofs_log<<"process_EXIF sss"<<endl;
#endif
			}
			else
			{
				// Discard this section.
				free(Sections[--SectionsRead].Data);
				Sections[SectionsRead].Data=0;
			}
			break;

		case M_SOF0: 
		case M_SOF1: 
		case M_SOF2: 
		case M_SOF3: 
		case M_SOF5: 
		case M_SOF6: 
		case M_SOF7: 
		case M_SOF9: 
		case M_SOF10:
		case M_SOF11:
		case M_SOF13:
		case M_SOF14:
		case M_SOF15:
			{
#ifdef PRINT_LOG
				ofs_log<<"process_SOFn aaa"<<endl;
#endif
				nres = process_SOFn(Data, marker);
				if (!nres)
				   return nres;
#ifdef PRINT_LOG
				ofs_log<<"process_SOFn sss"<<endl;
#endif
				break;

			}

		default:
			// Skip any other sections.
			//if (ShowTags) printf("Jpeg section marker 0x%02x size %d\n",marker, itemlen);
			break;
		}
	}
#ifdef PRINT_LOG
	ofs_log<<"DecodeExif return "<<endl;
#endif

	printf("index_k=%d\n",index_k);
	return 1;
}



bool Cexif::DecodeExif(const unsigned char *pExifBuf)
{


	int index_k=0 ;
	int a;
	int HaveCom = 0;
	a = pExifBuf[index_k++];

	if (a != 0xff || pExifBuf[index_k++] != M_SOI)
	{

		return 0;
	}

	for(;;)
	{
		int itemlen;
		int marker = 0;
		int ll,lh, got;
		unsigned char * Data;

		if (SectionsRead >= MAX_SECTIONS)
		{
			strcpy(m_szLastError,"Too many sections in jpg file");
			return 0;
		}

		for (a=0;a<7;a++){
			marker = pExifBuf[index_k++];
			if (marker != 0xff) break;

			if (a >= 6){
				printf("too many padding unsigned chars\n");
				return 0;
			}
		}

		if (marker == 0xff){
			// 0xff is legal padding, but if we get that many, something's wrong.
			strcpy(m_szLastError,"too many padding unsigned chars!");
			return 0;
		}

		Sections[SectionsRead].Type = marker;

		// Read the length of the section.
		lh = pExifBuf[index_k++];
		ll = pExifBuf[index_k++];

		itemlen = (lh << 8) | ll;

		if (itemlen < 2){
			strcpy(m_szLastError,"invalid marker");
			return 0;
		}

		Sections[SectionsRead].Size = itemlen;

		Data = (unsigned char *)malloc(itemlen);
		if (Data == NULL){
			strcpy(m_szLastError,"Could not allocate memory");
			return 0;
		}
		Sections[SectionsRead].Data = Data;

		// Store first two pre-read unsigned chars.
		Data[0] = (unsigned char)lh;
		Data[1] = (unsigned char)ll;

		memcpy(Data+2,pExifBuf+index_k,itemlen-2);
		index_k +=(itemlen-2);
		//got = fread(Data+2, 1, itemlen-2,hFile); // Read the whole section.

		SectionsRead += 1;

		switch(marker){

		case M_SOS:   // stop before hitting compressed data 
			// If reading entire image is requested, read the rest of the data.
			return 1;

		case M_EOI:   // in case it's a tables-only JPEG stream
			printf("No image in jpeg!\n");
			return 0;

		case M_COM: // Comment section
			if (HaveCom){
				// Discard this section.
				free(Sections[--SectionsRead].Data);
				Sections[SectionsRead].Data=0;
			}
			else
			{

				process_COM(Data, itemlen);

				HaveCom = 1;
			}
			break;

		case M_JFIF:
			// Regular jpegs always have this tag, exif images have the exif
			// marker instead, althogh ACDsee will write images with both markers.
			// this program will re-create this marker on absence of exif marker.
			// hence no need to keep the copy from the file.
			free(Sections[--SectionsRead].Data);
			Sections[SectionsRead].Data=0;
			break;

		case M_EXIF:
			// Seen files from some 'U-lead' software with Vivitar scanner
			// that uses marker 31 for non exif stuff.  Thus make sure 
			// it says 'Exif' in the section before treating it as exif.
			if (memcmp(Data+2, "Exif", 4) == 0)
			{

				m_exifinfo->IsExif = process_EXIF((unsigned char *)Data+2, itemlen - 2);

			}
			else
			{
				// Discard this section.
				free(Sections[--SectionsRead].Data);
				Sections[SectionsRead].Data=0;
			}
			break;

		case M_SOF0: 
		case M_SOF1: 
		case M_SOF2: 
		case M_SOF3: 
		case M_SOF5: 
		case M_SOF6: 
		case M_SOF7: 
		case M_SOF9: 
		case M_SOF10:
		case M_SOF11:
		case M_SOF13:
		case M_SOF14:
		case M_SOF15:
			{

				process_SOFn(Data, marker);

				break;

			}

		default:
			// Skip any other sections.
			//if (ShowTags) printf("Jpeg section marker 0x%02x size %d\n",marker, itemlen);
			break;
		}
	}


	printf("index_k=%d\n",index_k);
	return 1;
}
////////////////////////////////////////////////////////////////////////////////
/*--------------------------------------------------------------------------
Process a EXIF marker
Describes all the drivel that most digital cameras include...
--------------------------------------------------------------------------*/
bool Cexif::process_EXIF(unsigned char * CharBuf, unsigned int length)
{
	int nres = 0;
	m_exifinfo->FlashUsed = 0; 
	/* If it's from a digicam, and it used flash, it says so. */
	m_exifinfo->Comments[0] = '\0';  /* Initial value - null string */

	ExifImageWidth = 0;

	{   /* Check the EXIF header component */
		static const unsigned char ExifHeader[] = "Exif\0\0";
		nres = validatePointer_check( m_pBegin,m_pEnd, CharBuf + 5);
		if (!nres)
	       return nres;
		if (memcmp(CharBuf+0, ExifHeader,6)){
			strcpy(m_szLastError,"Incorrect Exif header");
			return 0;
		}
	}
	nres = validatePointer_check( m_pBegin,m_pEnd, CharBuf + 7);
	if (!nres)
       return nres;
	if (memcmp(CharBuf+6,"II",2) == 0){
		MotorolaOrder = 0;
	}else{
		if (memcmp(CharBuf+6,"MM",2) == 0){
			MotorolaOrder = 1;
		}else{
			strcpy(m_szLastError,"Invalid Exif alignment marker.");
			return 0;
		}
	}

	/* Check the next two values for correctness. */
	int nValue;
#ifdef _Check
	nres = Get16u(CharBuf+8,nValue);
	if (!nres)
		return nres ;
#else
	Get16u_NoCheck(CharBuf+8,nValue);
#endif
	if (nValue != 0x2a)
	{
		strcpy(m_szLastError,"Invalid Exif start (1)");
		return 0;
	}
	unsigned long FirstOffset ;
#ifdef _Check
	nres = Get32u(CharBuf+10, FirstOffset);
	if (!nres)
		return nres ;
#else
	Get32u_NoCheck(CharBuf+10, FirstOffset);
#endif
	if (FirstOffset < 8 || FirstOffset > 16){
		// I used to ensure this was set to 8 (website I used indicated its 8)
		// but PENTAX Optio 230 has it set differently, and uses it as offset. (Sept 11 2002)
		strcpy(m_szLastError,"Suspicious offset of first IFD value");
		return 0;
	}
	unsigned char * LastExifRefd = CharBuf;

	/* First directory starts 16 unsigned chars in.  Offsets start at 8 unsigned chars in. */
	if (!ProcessExifDir(CharBuf+14, CharBuf+6, length-6, m_exifinfo, &LastExifRefd))
		return 0;

	/* This is how far the interesting (non thumbnail) part of the exif went. */
	// int ExifSettingsLength = LastExifRefd - CharBuf;

	/* Compute the CCD width, in milimeters. */
	if (m_exifinfo->FocalplaneXRes != 0){
		m_exifinfo->CCDWidth = (float)(ExifImageWidth * m_exifinfo->FocalplaneUnits / m_exifinfo->FocalplaneXRes);
	}

	return 1;
}
//--------------------------------------------------------------------------
// Get 16 bits motorola order (always) for jpeg header stuff.
//--------------------------------------------------------------------------
int Cexif::Get16m(void * Short,int &nValue)
{
	int nres = validatePointer_check( m_pBegin,m_pEnd,((unsigned char *)Short) + 1);
	if (!nres)
		return nres;
	nValue = (((unsigned char *)Short)[0] << 8) | ((unsigned char *)Short)[1];
	return 1;
}
int Cexif::Get16m_NoCheck(void * Short,int &nValue)
{
	nValue = (((unsigned char *)Short)[0] << 8) | ((unsigned char *)Short)[1];
	return 1;
}
////////////////////////////////////////////////////////////////////////////////
/*--------------------------------------------------------------------------
Convert a 16 bit unsigned value from file's native unsigned char order
--------------------------------------------------------------------------*/
int Cexif::Get16u(void * Short,int &nValue)
{
	int nres = validatePointer_check( m_pBegin,m_pEnd,((unsigned char *)Short) + 1);
	if (!nres)
		return nres;
	if (MotorolaOrder)
		nValue= (((unsigned char *)Short)[0] << 8) | ((unsigned char *)Short)[1];
	else
		nValue= (((unsigned char *)Short)[1] << 8) | ((unsigned char *)Short)[0];
	return 1;
}
int Cexif::Get16u_NoCheck(void * Short,int &nValue)
{
	if (MotorolaOrder)
		nValue= (((unsigned char *)Short)[0] << 8) | ((unsigned char *)Short)[1];
	else
		nValue= (((unsigned char *)Short)[1] << 8) | ((unsigned char *)Short)[0];
	return 1;
}
////////////////////////////////////////////////////////////////////////////////
/*--------------------------------------------------------------------------
Convert a 32 bit signed value from file's native unsigned char order
--------------------------------------------------------------------------*/
int Cexif::Get32s(void * Long, long& nValue)
{
	int nres = validatePointer_check( m_pBegin,m_pEnd,((unsigned char *)Long) + 3);
	if (!nres)
		return nres;
	if (MotorolaOrder){
		nValue =  ((( char *)Long)[0] << 24) | (((unsigned char *)Long)[1] << 16)
			| (((unsigned char *)Long)[2] << 8 ) | (((unsigned char *)Long)[3] << 0 );
	}else{
		nValue=  ((( char *)Long)[3] << 24) | (((unsigned char *)Long)[2] << 16)
			| (((unsigned char *)Long)[1] << 8 ) | (((unsigned char *)Long)[0] << 0 );
	}
	return 1;
}
int Cexif::Get32s_NoCheck(void * Long,long &nValue)
{
	if (MotorolaOrder){
		nValue =  ((( char *)Long)[0] << 24) | (((unsigned char *)Long)[1] << 16)
			| (((unsigned char *)Long)[2] << 8 ) | (((unsigned char *)Long)[3] << 0 );
	}else{
		nValue=  ((( char *)Long)[3] << 24) | (((unsigned char *)Long)[2] << 16)
			| (((unsigned char *)Long)[1] << 8 ) | (((unsigned char *)Long)[0] << 0 );
	}
	return 1;
}
////////////////////////////////////////////////////////////////////////////////
/*--------------------------------------------------------------------------
Convert a 32 bit unsigned value from file's native unsigned char order
--------------------------------------------------------------------------*/
int Cexif::Get32u(void * Long,unsigned long &nValue)
{
	long nab ;
	int nres = Get32s(Long,nab);
	if (!nres)
		return nres;
	nValue = nab;
	nValue = nValue& 0xffffffff;
	return 1 ;
}
int Cexif::Get32u_NoCheck(void * Long,unsigned long &nValue)
{
	long nab ;
	Get32s_NoCheck(Long,nab);
	nValue = nab;
	nValue = nValue& 0xffffffff;
	return 1;
}
////////////////////////////////////////////////////////////////////////////////

/* Describes format descriptor */
static const int BytesPerFormat[] = {0,1,1,2,4,8,1,1,2,4,8,4,8};
#define NUM_FORMATS 12

#define FMT_BYTE       1 
#define FMT_STRING     2
#define FMT_USHORT     3
#define FMT_ULONG      4
#define FMT_URATIONAL  5
#define FMT_SBYTE      6
#define FMT_UNDEFINED  7
#define FMT_SSHORT     8
#define FMT_SLONG      9
#define FMT_SRATIONAL 10
#define FMT_SINGLE    11
#define FMT_DOUBLE    12

/* Describes tag values */

#define TAG_EXIF_VERSION      0x9000
#define TAG_EXIF_OFFSET       0x8769
#define TAG_INTEROP_OFFSET    0xa005

#define TAG_MAKE              0x010F
#define TAG_MODEL             0x0110

#define TAG_ORIENTATION       0x0112
#define TAG_XRESOLUTION       0x011A
#define TAG_YRESOLUTION       0x011B
#define TAG_RESOLUTIONUNIT    0x0128

#define TAG_EXPOSURETIME      0x829A
#define TAG_FNUMBER           0x829D

#define TAG_SHUTTERSPEED      0x9201
#define TAG_APERTURE          0x9202
#define TAG_BRIGHTNESS        0x9203
#define TAG_MAXAPERTURE       0x9205
#define TAG_FOCALLENGTH       0x920A

#define TAG_DATETIME_ORIGINAL 0x9003
#define TAG_USERCOMMENT       0x9286

#define TAG_SUBJECT_DISTANCE  0x9206
#define TAG_FLASH             0x9209

#define TAG_FOCALPLANEXRES    0xa20E
#define TAG_FOCALPLANEYRES    0xa20F
#define TAG_FOCALPLANEUNITS   0xa210
#define TAG_EXIF_IMAGEWIDTH   0xA002
#define TAG_EXIF_IMAGELENGTH  0xA003

/* the following is added 05-jan-2001 vcs */
#define TAG_EXPOSURE_BIAS     0x9204
#define TAG_WHITEBALANCE      0x9208
#define TAG_METERING_MODE     0x9207
#define TAG_EXPOSURE_PROGRAM  0x8822
#define TAG_ISO_EQUIVALENT    0x8827
#define TAG_COMPRESSION_LEVEL 0x9102

#define TAG_THUMBNAIL_OFFSET  0x0201
#define TAG_THUMBNAIL_LENGTH  0x0202



//---------------��������----------------
#define TAG_GPS_VERSIONID       0x0000  //GPS �汾
#define TAG_GPS_LATITUDEREF     0x0001  //γ�Ȳο���������γ
#define TAG_GPS_LATITUDE        0x0002  //γ��ֵ
#define TAG_GPS_LONGITUDEREF    0x0003  //���Ȳο������綫��
#define TAG_GPS_LONGITUDE       0x0004  //����ֵ
#define TAG_GPS_ALTITUDEREF     0x0005  //���θ߶Ȳο�
#define TAG_GPS_ALTITUDE        0x0006  //����
#define TAG_GPS_TIMESTAMP       0x0007  //ʱ���
#define TAG_GPS_SATELLITES      0x0008  //����
#define TAG_GPS_STATUS          0x0009  //״̬
#define TAG_GPS_MEASUREMODE     0x000A  //
#define TAG_GPS_DOP             0x000B  //
#define TAG_GPS_SPEEDREF        0x000C  //
#define TAG_GPS_SPEED           0x000D  //
#define TAG_GPS_TRACKREF        0x000E  //
#define TAG_GPS_TRACK           0x000F  //
#define TAG_GPS_IMGDIRECTIONREF 0x0010  //
#define TAG_GPS_IMGDIRECTION    0x0011  //
#define TAG_GPS_MAPDATUM        0x0012  //
#define TAG_GPS_DESTLATITUDEREF 0x0013  //
#define TAG_GPS_DESTLATITUDE    0x0014  //
#define TAG_GPS_DESTLONGITUDEREF  0x0015//
#define TAG_GPS_DESTLONGITUDE   0x0016  //
#define TAG_GPS_DESTBEARINGREF  0x0017  //
#define TAG_GPS_DESTBEARING     0x0018  //
#define TAG_GPS_DESTDISTANCEREF 0x0019  //
#define TAG_GPS_DESTDISTANCE    0x001A  //
//-----------------------------------
#define TAG_GPS_INFO 0x8825




#ifndef _WIN32
/*--------------------------------------------------------------------------
Process one of the nested EXIF directories.
--------------------------------------------------------------------------*/
bool Cexif::ProcessExifDir(unsigned char * DirStart, unsigned char * OffsetBase, unsigned ExifLength,
						   EXIFINFO * const m_exifinfo, unsigned char ** const LastExifRefdP )
{
// add wendy 
    long curr_Address = long(DirStart);
	if (curr_Address <= m_address)
	    return 0;
	else
		m_address = curr_Address;
// add wendy 

	int nres;
	int de;
	int a;
	int NumDirEntries;

	unsigned ThumbnailOffset = 0;
	unsigned ThumbnailSize = 0;

	double gps ;
#ifdef _Check
	nres = Get16u(DirStart,NumDirEntries);
	if (!nres)
		return nres ;
#else
	Get16u_NoCheck(DirStart,NumDirEntries);
#endif
// wendy modify > to >=
	if ((DirStart+2+NumDirEntries*12) >= (OffsetBase+ExifLength)){
		strcpy(m_szLastError,"Illegally sized directory");
		return 0;
	}

	for (de=0;de<NumDirEntries;de++){
		int Tag, Format;
		unsigned long Components;
		unsigned char * ValuePtr;
		/* This actually can point to a variety of things; it must be
		cast to other types when used.  But we use it as a unsigned char-by-unsigned char
		cursor, so we declare it as a pointer to a generic unsigned char here.
		*/
		int BytesCount;
		unsigned char * DirEntry;
		DirEntry = DirStart+2+12*de;
#ifdef _Check
		nres = Get16u(DirEntry,Tag);
		if (!nres)
			return nres;
		nres = Get16u(DirEntry+2,Format);
		if (!nres)
			return nres;
		nres = Get32u(DirEntry+4,Components);
		if (!nres)
			return nres;
#else
        Get16u_NoCheck(DirEntry,Tag);
		Get16u_NoCheck(DirEntry+2,Format);
		Get32u_NoCheck(DirEntry+4,Components);
#endif
        //if(Tag !=TAG_ORIENTATION  && Tag != TAG_DATETIME_ORIGINAL)
        //{
        //    continue;
        //}
		if ((Format-1) >= NUM_FORMATS) {
			/* (-1) catches illegal zero case as unsigned underflows to positive large */
			strcpy(m_szLastError,"Illegal format code in EXIF dir");
			return 0;
		}


		BytesCount = Components * BytesPerFormat[Format];

		if (BytesCount > 4 ){
			unsigned long OffsetVal;
#ifdef _Check
			int nres = Get32u(DirEntry+8,OffsetVal);
			if (!nres)
				return nres;
#else
			Get32u_NoCheck(DirEntry+8,OffsetVal);
#endif
			/* If its bigger than 4 unsigned chars, the dir entry contains an offset.*/
			if (OffsetVal+BytesCount > ExifLength){
				/* Bogus pointer offset and / or unsigned charcount value */
				strcpy(m_szLastError,"Illegal pointer offset value in EXIF.");
				return 0;
			}
			ValuePtr = OffsetBase+OffsetVal;
		}else{
			/* 4 unsigned chars or less and value is in the dir entry itself */
			ValuePtr = DirEntry+8;
		}

		if (*LastExifRefdP < ValuePtr+BytesCount){
			/* Keep track of last unsigned char in the exif header that was
			actually referenced.  That way, we know where the
			discardable thumbnail data begins.
			*/
			*LastExifRefdP = ValuePtr+BytesCount;
		}

		/* Extract useful components of tag */
		switch(Tag){

		case TAG_ORIENTATION:
			//m_exifinfo->Orientation = (int)ConvertAnyFormat(ValuePtr, Format);
			double d_Position;
			nres = ConvertAnyFormat(ValuePtr, Format,d_Position);
			if (!nres)
				return nres;
			else
				m_exifinfo->Orientation = (int)d_Position;
			if (m_exifinfo->Orientation < 1 || m_exifinfo->Orientation > 8){
				strcpy(m_szLastError,"Undefined rotation value");
				m_exifinfo->Orientation = 0;
			}
			break;

		}
		if (Tag == TAG_EXIF_OFFSET || Tag == TAG_INTEROP_OFFSET){
			unsigned char * SubdirStart;
			unsigned long lValue;
#ifdef _Check
		   nres = Get32u(ValuePtr,lValue);
		   if (!nres)
               return nres;
#else
			nres = Get32u_NoCheck(ValuePtr,lValue);
#endif
// wendy modify > to >=
			SubdirStart = OffsetBase + lValue;
			if (SubdirStart < OffsetBase || 
				SubdirStart >= OffsetBase+ExifLength){
					strcpy(m_szLastError,"Illegal subdirectory link");
					return 0;
			}
			ProcessExifDir(SubdirStart, OffsetBase, ExifLength, m_exifinfo, LastExifRefdP);
			continue;
		}
	}


	{
		/* In addition to linking to subdirectories via exif tags,
		there's also a potential link to another directory at the end
		of each directory.  This has got to be the result of a
		committee!  
		*/
		unsigned char * SubdirStart;
		int Offset;
#ifdef _Check
		nres = Get16u(DirStart+2+12*NumDirEntries,Offset);
		if (!nres)
           return nres;
#else
		Get16u_NoCheck(DirStart+2+12*NumDirEntries,Offset);
#endif
//  wendy modify > to >=
		if (Offset){
			SubdirStart = OffsetBase + Offset;
			if (SubdirStart < OffsetBase 
				|| SubdirStart >= OffsetBase+ExifLength){
					strcpy(m_szLastError,"Illegal subdirectory link");
					return 0;
			}
			ProcessExifDir(SubdirStart, OffsetBase, ExifLength, m_exifinfo, LastExifRefdP);
		}
	}
	if (ThumbnailSize && ThumbnailOffset){
		if (ThumbnailSize + ThumbnailOffset <= ExifLength){
			/* The thumbnail pointer appears to be valid.  Store it. */
			m_exifinfo->ThumbnailPointer = OffsetBase + ThumbnailOffset;
			m_exifinfo->ThumbnailSize = ThumbnailSize;
		}
	}

	return 1;
}

#else  // WIN32
bool Cexif::ProcessExifDir(unsigned char * DirStart, unsigned char * OffsetBase, unsigned ExifLength,
						   EXIFINFO * const m_exifinfo, unsigned char ** const LastExifRefdP )
{
// add wendy
#ifdef _Check
    long curr_Address = long(DirStart);
	if (curr_Address <= m_address)
	    return 0;
	else
		m_address = curr_Address;
#endif

	int nres = 0;
	int de;
	int a;
	int NumDirEntries;
	unsigned ThumbnailOffset = 0;
	unsigned ThumbnailSize = 0;

	double gps ;
#ifdef _Check
	nres = Get16u(DirStart,NumDirEntries);
	if (!nres)
		return nres ;
#else
	Get16u_NoCheck(DirStart,NumDirEntries);
#endif
// wendy modify  > tp >=
	if ((DirStart+2+NumDirEntries*12) >= (OffsetBase+ExifLength)){
		strcpy(m_szLastError,"Illegally sized directory");
		return 0;
	}

	for (de=0;de<NumDirEntries;de++){
		int Tag, Format;
		unsigned long Components;
		unsigned char * ValuePtr;
		/* This actually can point to a variety of things; it must be
		cast to other types when used.  But we use it as a unsigned char-by-unsigned char
		cursor, so we declare it as a pointer to a generic unsigned char here.
		*/
		int BytesCount;
		unsigned char * DirEntry;
		DirEntry = DirStart+2+12*de;
#ifdef _Check
		nres = Get16u(DirEntry,Tag);
		if (!nres)
			return nres;
		nres = Get16u(DirEntry+2,Format);
		if (!nres)
			return nres;
		nres = Get32u(DirEntry+4,Components);
		if (!nres)
			return nres;
#else
		Get16u_NoCheck(DirEntry,Tag);
		Get16u_NoCheck(DirEntry+2,Format);
		Get32u_NoCheck(DirEntry+4,Components);
#endif
		if ((Format-1) >= NUM_FORMATS) {
			/* (-1) catches illegal zero case as unsigned underflows to positive large */
			strcpy(m_szLastError,"Illegal format code in EXIF dir");
			return 0;
		}

		BytesCount = Components * BytesPerFormat[Format];

		if (BytesCount > 4 ){
			unsigned long OffsetVal;
#ifdef _Check
			int nres = Get32u(DirEntry+8,OffsetVal);
			if (!nres)
				return nres;
#else
			Get32u_NoCheck(DirEntry+8,OffsetVal);
#endif
			/* If its bigger than 4 unsigned chars, the dir entry contains an offset.*/
			if (OffsetVal+BytesCount > ExifLength){
				/* Bogus pointer offset and / or unsigned charcount value */
				strcpy(m_szLastError,"Illegal pointer offset value in EXIF.");
				return 0;
			}
			ValuePtr = OffsetBase+OffsetVal;
		}else{
			/* 4 unsigned chars or less and value is in the dir entry itself */
			ValuePtr = DirEntry+8;
		}

		if (*LastExifRefdP < ValuePtr+BytesCount){
			/* Keep track of last unsigned char in the exif header that was
			actually referenced.  That way, we know where the
			discardable thumbnail data begins.
			*/
			*LastExifRefdP = ValuePtr+BytesCount;
		}

		/* Extract useful components of tag */
		switch(Tag){


		case TAG_MAKE:
			strncpy(m_exifinfo->CameraMake, (char*)ValuePtr, 31);
			break;

		case TAG_MODEL:
			strncpy(m_exifinfo->CameraModel, (char*)ValuePtr, 39);
			break;

		case TAG_EXIF_VERSION:
			strncpy(m_exifinfo->Version,(char*)ValuePtr, 4);
			break;

		case TAG_DATETIME_ORIGINAL:
			strncpy(m_exifinfo->DateTime, (char*)ValuePtr, 19);
			// printf("length:%d\n",Format);
			break;

		case TAG_USERCOMMENT:
			// Olympus has this padded with trailing spaces. Remove these first. 
			for (a=BytesCount;;){
				a--;
				if (((char*)ValuePtr)[a] == ' '){
					((char*)ValuePtr)[a] = '\0';
				}else{
					break;
				}
				if (a == 0) break;
			}

			/* Copy the comment */
			if (memcmp(ValuePtr, "ASCII",5) == 0){
				for (a=5;a<10;a++){
					char c;
					c = ((char*)ValuePtr)[a];
					if (c != '\0' && c != ' '){
						strncpy(m_exifinfo->Comments, (char*)ValuePtr+a, 199);
						break;
					}
				}

			}else{
				strncpy(m_exifinfo->Comments, (char*)ValuePtr, 199);
			}
			break;

		case TAG_FNUMBER:
			/* Simplest way of expressing aperture, so I trust it the most.
			(overwrite previously computd value if there is one)
			*/
			m_exifinfo->ApertureFNumber = (float)ConvertAnyFormat(ValuePtr, Format);
			break;

		case TAG_APERTURE:
		case TAG_MAXAPERTURE:
			/* More relevant info always comes earlier, so only
			use this field if we don't have appropriate aperture
			information yet. 
			*/
			if (m_exifinfo->ApertureFNumber == 0){
				m_exifinfo->ApertureFNumber = (float)exp(ConvertAnyFormat(ValuePtr, Format)*log(2.0)*0.5);
			}
			break;

		case TAG_BRIGHTNESS:
			m_exifinfo->Brightness = (float)ConvertAnyFormat(ValuePtr, Format);
			break;

		case TAG_FOCALLENGTH:
			/* Nice digital cameras actually save the focal length
			as a function of how farthey are zoomed in. 
			*/

			m_exifinfo->FocalLength = (float)ConvertAnyFormat(ValuePtr, Format);
			break;

		case TAG_SUBJECT_DISTANCE:
			/* Inidcates the distacne the autofocus camera is focused to.
			Tends to be less accurate as distance increases.
			*/
			m_exifinfo->Distance = (float)ConvertAnyFormat(ValuePtr, Format);
			break;

		case TAG_EXPOSURETIME:
			/* Simplest way of expressing exposure time, so I
			trust it most.  (overwrite previously computd value
			if there is one) 
			*/
			m_exifinfo->ExposureTime = 
				(float)ConvertAnyFormat(ValuePtr, Format);
			break;

		case TAG_SHUTTERSPEED:
			/* More complicated way of expressing exposure time,
			so only use this value if we don't already have it
			from somewhere else.  
			*/
			if (m_exifinfo->ExposureTime == 0){
				m_exifinfo->ExposureTime = (float)
					(1/exp(ConvertAnyFormat(ValuePtr, Format)*log(2.0)));
			}
			break;

		case TAG_FLASH:
			if ((int)ConvertAnyFormat(ValuePtr, Format) & 7){
				m_exifinfo->FlashUsed = 1;
			}else{
				m_exifinfo->FlashUsed = 0;
			}
			break;

		case TAG_ORIENTATION:
			//m_exifinfo->Orientation = (int)ConvertAnyFormat(ValuePtr, Format);
		    double d_Position;
			nres = ConvertAnyFormat(ValuePtr, Format,d_Position);
			if (!nres)
				return nres;
			else
				m_exifinfo->Orientation = (int)d_Position;
			if (m_exifinfo->Orientation < 1 || m_exifinfo->Orientation > 8){
				strcpy(m_szLastError,"Undefined rotation value");
				m_exifinfo->Orientation = 0;
			}
			break;

		case TAG_EXIF_IMAGELENGTH:
		case TAG_EXIF_IMAGEWIDTH:
			/* Use largest of height and width to deal with images
			that have been rotated to portrait format.  
			*/
			a = (int)ConvertAnyFormat(ValuePtr, Format);
			if (ExifImageWidth < a) ExifImageWidth = a;
			break;

		case TAG_FOCALPLANEXRES:
			m_exifinfo->FocalplaneXRes = (float)ConvertAnyFormat(ValuePtr, Format);
			break;

		case TAG_FOCALPLANEYRES:
			m_exifinfo->FocalplaneYRes = (float)ConvertAnyFormat(ValuePtr, Format);
			break;

		case TAG_RESOLUTIONUNIT:
			switch((int)ConvertAnyFormat(ValuePtr, Format)){
			case 1: m_exifinfo->ResolutionUnit = 1.0f; break; /* 1 inch */
			case 2:	m_exifinfo->ResolutionUnit = 1.0f; break;
			case 3: m_exifinfo->ResolutionUnit = 0.3937007874f;    break;  /* 1 centimeter*/
			case 4: m_exifinfo->ResolutionUnit = 0.03937007874f;   break;  /* 1 millimeter*/
			case 5: m_exifinfo->ResolutionUnit = 0.00003937007874f;  /* 1 micrometer*/
			}
			break;

		case TAG_FOCALPLANEUNITS:
			switch((int)ConvertAnyFormat(ValuePtr, Format)){
			case 1: m_exifinfo->FocalplaneUnits = 1.0f; break; /* 1 inch */
			case 2:	m_exifinfo->FocalplaneUnits = 1.0f; break;
			case 3: m_exifinfo->FocalplaneUnits = 0.3937007874f;    break;  /* 1 centimeter*/
			case 4: m_exifinfo->FocalplaneUnits = 0.03937007874f;   break;  /* 1 millimeter*/
			case 5: m_exifinfo->FocalplaneUnits = 0.00003937007874f;  /* 1 micrometer*/
			}
			break;

			// Remaining cases contributed by: Volker C. Schoech <schoech(at)gmx(dot)de>

		case TAG_EXPOSURE_BIAS:
			m_exifinfo->ExposureBias = (float) ConvertAnyFormat(ValuePtr, Format);
			break;

		case TAG_WHITEBALANCE:
			m_exifinfo->Whitebalance = (int)ConvertAnyFormat(ValuePtr, Format);
			break;

		case TAG_METERING_MODE:
			m_exifinfo->MeteringMode = (int)ConvertAnyFormat(ValuePtr, Format);
			break;

		case TAG_EXPOSURE_PROGRAM:
			m_exifinfo->ExposureProgram = (int)ConvertAnyFormat(ValuePtr, Format);
			break;

		case TAG_ISO_EQUIVALENT:
			m_exifinfo->ISOequivalent = (int)ConvertAnyFormat(ValuePtr, Format);
			if ( m_exifinfo->ISOequivalent < 50 ) m_exifinfo->ISOequivalent *= 200;
			break;

		case TAG_COMPRESSION_LEVEL:
			m_exifinfo->CompressionLevel = (int)ConvertAnyFormat(ValuePtr, Format);
			break;

		case TAG_XRESOLUTION:
			m_exifinfo->Xresolution = (float)ConvertAnyFormat(ValuePtr, Format);
			break;
		case TAG_YRESOLUTION:
			m_exifinfo->Yresolution = (float)ConvertAnyFormat(ValuePtr, Format);
			break;

		case TAG_THUMBNAIL_OFFSET:
			ThumbnailOffset = (unsigned)ConvertAnyFormat(ValuePtr, Format);
			break;

		case TAG_THUMBNAIL_LENGTH:
			ThumbnailSize = (unsigned)ConvertAnyFormat(ValuePtr, Format);
			break;
			//----add by chenzhijun-----------
		case TAG_GPS_INFO:
			{
				//GPSINFO is  linking to subdirectories via exif tags,
				unsigned char * SubdirStart;
				unsigned Offset = ConvertAnyFormat(ValuePtr, Format);

				m_exifinfo->isHaveGPSInfo = true;

				if (Offset){
					SubdirStart = OffsetBase + Offset;
					if (SubdirStart < OffsetBase 
						|| SubdirStart >= OffsetBase+ExifLength){
							strcpy(m_szLastError,"Illegal subdirectory link");
							return 0;
					}
					ProcessExifDir(SubdirStart, OffsetBase, ExifLength, m_exifinfo, LastExifRefdP);
				}
			}
			break;
			// 
		case TAG_GPS_VERSIONID:
// wendy modify
			if (Components <= 4)
			{
			   for (int k = 0;k<Components;++k)
			   {
				   m_exifinfo->GPSVersionID[k]=(unsigned) ConvertAnyFormat(ValuePtr+k*Format, Format);
			    }
//wendy modify
			}
			break;
		case TAG_GPS_LATITUDEREF:
			if (m_exifinfo->isHaveGPSInfo)  
			{
				strncpy( m_exifinfo->GPSLattitudeRef, (char*)ValuePtr, Format);
			}
			break;
		case TAG_GPS_LATITUDE:
			//ExifInteroperabilityOffset Currently there are 2 directory entries, 
			//first one is Tag0x0001, value is "R98", next is Tag0x0002, value is "0100".
			// need check
			if (m_exifinfo->isHaveGPSInfo)   
			{
				for (int k=0;k<Components;++k)
				{
					double denominator,numerator;

					numerator = ConvertAnyFormat(ValuePtr+k*8,4);
					denominator = ConvertAnyFormat(ValuePtr+k*8+4,4);
					m_exifinfo->GPSLattitude[k] = numerator/denominator;
				}
			}

			break;
		case TAG_GPS_LONGITUDEREF:
			strncpy( m_exifinfo->GPSLongitudeRef, (char*)ValuePtr, Format);

			break;
		case TAG_GPS_LONGITUDE:
			for (int k=0;k<Components;++k)
			{
				double denominator,numerator;

				numerator = ConvertAnyFormat(ValuePtr+k*8,4);
				denominator = ConvertAnyFormat(ValuePtr+k*8+4,4);
				m_exifinfo->GPSLongitude[k] = numerator/denominator;
			}
			break;
			//----------------end add by chenzhijun-------------------

		}

		if (Tag == TAG_EXIF_OFFSET || Tag == TAG_INTEROP_OFFSET){
			unsigned long lValue;
#ifdef _Check
			nres = Get32u(ValuePtr,lValue);
			if (!nres)
				return nres;
#else
			Get32u_NoCheck(ValuePtr,lValue);
#endif
			unsigned char * SubdirStart;
			SubdirStart = OffsetBase + lValue;
			if (SubdirStart < OffsetBase || 
				SubdirStart >= OffsetBase+ExifLength){
					strcpy(m_szLastError,"Illegal subdirectory link");
					return 0;
			}
			ProcessExifDir(SubdirStart, OffsetBase, ExifLength, m_exifinfo, LastExifRefdP);
			continue;
		}
	}


	{
		/* In addition to linking to subdirectories via exif tags,
		there's also a potential link to another directory at the end
		of each directory.  This has got to be the result of a
		committee!  
		*/
		unsigned char * SubdirStart;
		//unsigned  Offset;
		//Offset = Get16u(DirStart+2+12*NumDirEntries);
		int Offset;
#ifdef _Check
		int nres = Get16u(DirStart+2+12*NumDirEntries,Offset);
		if (!nres)
           return nres;
#else
		Get16u_NoCheck(DirStart+2+12*NumDirEntries,Offset);
#endif
		if (Offset){
			SubdirStart = OffsetBase + Offset;
			if (SubdirStart < OffsetBase 
				|| SubdirStart >= OffsetBase+ExifLength){
					strcpy(m_szLastError,"Illegal subdirectory link");
					return 0;
			}
			ProcessExifDir(SubdirStart, OffsetBase, ExifLength, m_exifinfo, LastExifRefdP);
		}
	}


	if (ThumbnailSize && ThumbnailOffset){
		if (ThumbnailSize + ThumbnailOffset <= ExifLength){
			/* The thumbnail pointer appears to be valid.  Store it. */
			m_exifinfo->ThumbnailPointer = OffsetBase + ThumbnailOffset;
			m_exifinfo->ThumbnailSize = ThumbnailSize;
		}
	}

	return 1;
}
#endif //_WIN32
////////////////////////////////////////////////////////////////////////////////
/*--------------------------------------------------------------------------
Evaluate number, be it int, rational, or float from directory.
--------------------------------------------------------------------------*/
double Cexif::ConvertAnyFormat(void * ValuePtr, int Format)
{
	int nres = 0;
	double Value = 0;

	switch(Format){
	case FMT_SBYTE:    
#ifdef _Check
		nres = validatePointer_check(m_pBegin,m_pEnd,(unsigned char *)ValuePtr);
		if (!nres)
            return nres;
#endif
	    Value = *(signed char *)ValuePtr; 
	    break;
	case FMT_BYTE:  
#ifdef _Check
		nres = validatePointer_check(m_pBegin,m_pEnd,(unsigned char *)ValuePtr);
		if (!nres)
		    return nres;
#endif
		Value = *(unsigned char *)ValuePtr;       
		break;
	case FMT_USHORT: 
	    int iValue;
#ifdef _Check
		nres = Get16u(ValuePtr,iValue);  
		if (!nres)
		   return nres;
#else
		Get16u_NoCheck(ValuePtr,iValue); 
#endif
		Value = iValue;
		break;
	case FMT_ULONG:     
		unsigned long ulValue;
#ifdef _Check
		nres = Get32u(ValuePtr,ulValue);
		if (!nres)
			return nres;
#else
		Get32u_NoCheck(ValuePtr,ulValue);
#endif
		Value = ulValue;
		break;
	case FMT_URATIONAL:
	case FMT_SRATIONAL: 
		{
		   	long Num,Den;
#ifdef _Check
		    nres = Get32s(ValuePtr,Num);
			if (!nres)
				 return nres;
			nres =  Get32s(4+(char *)ValuePtr,Den);
			if (!nres)
				 return nres;
#else
		    Get32s_NoCheck(ValuePtr,Num);
			Get32s_NoCheck(4+(char *)ValuePtr,Den);
#endif
			if (Den == 0){
				Value = 0;
			}else{
				Value = (double)Num/Den;
			}
			break;
		}

	case FMT_SSHORT:    
		int siValue;
#ifdef _Check
		nres = Get16u(ValuePtr,siValue);  
		if (!nres)
		   return nres;
#else
        Get16u_NoCheck(ValuePtr,siValue);
#endif
		Value = (signed short)siValue;
		break;
	case FMT_SLONG:   
		long slValue;
#ifdef _Check
		nres = Get32s(ValuePtr,slValue);
		if (!nres)
			return nres;
#else
		Get32s_NoCheck(ValuePtr,slValue);
#endif
		Value = slValue;
		break;
		/* Not sure if this is correct (never seen float used in Exif format)
		*/
	case FMT_SINGLE:   
#ifdef _Check
		nres = validatePointer_check(m_pBegin,m_pEnd,(unsigned char *)ValuePtr + sizeof(float) / sizeof(char));
		if (!nres)
            return nres;
#endif
		Value = (double)*(float *)ValuePtr;    
		break;
	case FMT_DOUBLE:   
#ifdef _Check
		nres = validatePointer_check(m_pBegin,m_pEnd,(unsigned char *)ValuePtr + sizeof(double) / sizeof(char));
		if (!nres)
          return nres;
#endif
		 Value = *(double *)ValuePtr;        
		 break;
	}
	return Value;
}
int Cexif::ConvertAnyFormat(void *ValuePtr,int Format,double &Value)
{
	int nres = 0;
	Value = 0;
	switch(Format){
	case FMT_SBYTE:    
#ifdef _Check
		nres = validatePointer_check(m_pBegin,m_pEnd,(unsigned char *)ValuePtr);
		if (!nres)
            return nres;
#endif
	    Value = *(signed char *)ValuePtr; 
	    break;
	case FMT_BYTE:  
#ifdef _Check
		nres = validatePointer_check(m_pBegin,m_pEnd,(unsigned char *)ValuePtr);
		if (!nres)
		    return nres;
#endif
		Value = *(unsigned char *)ValuePtr;       
		break;
	case FMT_USHORT: 
	    int iValue;
#ifdef _Check
		nres = Get16u(ValuePtr,iValue);  
		if (!nres)
		   return nres;
#else
		Get16u_NoCheck(ValuePtr,iValue); 
#endif
		Value = iValue;
		break;
	case FMT_ULONG:     
		unsigned long ulValue;
#ifdef _Check
		nres = Get32u(ValuePtr,ulValue);
		if (!nres)
			return nres;
#else
		Get32u_NoCheck(ValuePtr,ulValue);
#endif
		Value = ulValue;
		break;
	case FMT_URATIONAL:
	case FMT_SRATIONAL: 
		{
		   	long Num,Den;
#ifdef _Check
		    nres = Get32s(ValuePtr,Num);
			if (!nres)
				 return nres;
			nres =  Get32s(4+(char *)ValuePtr,Den);
			if (!nres)
				 return nres;
#else
		    Get32s_NoCheck(ValuePtr,Num);
			Get32s_NoCheck(4+(char *)ValuePtr,Den);
#endif
			if (Den == 0){
				Value = 0;
			}else{
				Value = (double)Num/Den;
			}
			break;
		}

	case FMT_SSHORT:    
		int siValue;
#ifdef _Check
		nres = Get16u(ValuePtr,siValue);  
		if (!nres)
		   return nres;
#else
        Get16u_NoCheck(ValuePtr,siValue);
#endif
		Value = (signed short)siValue;
		break;
	case FMT_SLONG:   
		long slValue;
#ifdef _Check
		nres = Get32s(ValuePtr,slValue);
		if (!nres)
			return nres;
#else
		Get32s_NoCheck(ValuePtr,slValue);
#endif
		Value = slValue;
		break;
		/* Not sure if this is correct (never seen float used in Exif format)
		*/
	case FMT_SINGLE:   
#ifdef _Check
		nres = validatePointer_check(m_pBegin,m_pEnd,(unsigned char *)ValuePtr + sizeof(float) / sizeof(char) - 1);
		if (!nres)
            return nres;
#endif
		Value = (double)*(float *)ValuePtr;    
		break;
	case FMT_DOUBLE:   
#ifdef _Check
		nres = validatePointer_check(m_pBegin,m_pEnd,(unsigned char *)ValuePtr + sizeof(double) / sizeof(char) - 1);
		if (!nres)
          return nres;
#endif
		 Value = *(double *)ValuePtr;        
		 break;
	default:
		break;
	}
	return 1;
}
////////////////////////////////////////////////////////////////////////////////
void Cexif::process_COM (const unsigned char * Data, int length)
{
	int ch;
	char Comment[MAX_COMMENT+1];
	int nch;
	int a;

	nch = 0;

	if (length > MAX_COMMENT) length = MAX_COMMENT; // Truncate if it won't fit in our structure.

	for (a=2;a<length;a++){
		ch = Data[a];

		if (ch == '\r' && Data[a+1] == '\n') continue; // Remove cr followed by lf.

		if ((ch>=0x20) || ch == '\n' || ch == '\t'){
			Comment[nch++] = (char)ch;
		}else{
			Comment[nch++] = '?';
		}
	}

	Comment[nch] = '\0'; // Null terminate

	//if (ShowTags) printf("COM marker comment: %s\n",Comment);

	strcpy(m_exifinfo->Comments,Comment);
}
////////////////////////////////////////////////////////////////////////////////
int Cexif::process_SOFn (const unsigned char * Data, int marker)
{
	int nres = 1;
	int data_precision, num_components;
#ifdef _Check
	nres = validatePointer_check(m_pBegin,m_pEnd,(unsigned char *)Data + 2);
	if (!nres)
       return nres;
#endif
	data_precision = Data[2];
#ifdef _Check
	nres = Get16m((void*)(Data+3),m_exifinfo->Height);
	if (!nres)
		return nres;
	nres = Get16m((void*)(Data+5),m_exifinfo->Width);
	if (!nres)
		return nres;
#else
	Get16m_NoCheck((void*)(Data+3),m_exifinfo->Height);
	Get16m_NoCheck((void*)(Data+5),m_exifinfo->Width);
#endif
#ifdef _Check
	nres = validatePointer_check( m_pBegin,m_pEnd,(unsigned char *)Data + 7);
	if (!nres)
	   return nres;
#endif
	num_components = Data[7];

	if (num_components == 3){
		m_exifinfo->IsColor = 1;
	}else{
		m_exifinfo->IsColor = 0;
	}

	m_exifinfo->Process = marker;

	return nres;

	//if (ShowTags) printf("JPEG image is %uw * %uh, %d color components, %d bits per sample\n",
	//               ImageInfo.Width, ImageInfo.Height, num_components, data_precision);
}
////////////////////////////////////////////////////////////////////////////////
