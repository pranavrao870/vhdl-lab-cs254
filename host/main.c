/* 
 * Copyright (C) 2012-2014 Chris McClelland
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/file.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <makestuff.h>
#include <libfpgalink.h>
#include <libbuffer.h>
#include <liberror.h>
#include <libdump.h>
#include <argtable2.h>
#include <readline/readline.h>
#include <readline/history.h>
#ifdef WIN32
#include <Windows.h>
#else
#include <sys/time.h>
#endif


char key[33] = "10101001010010010100100101001001";
char ack1[33] = "10011100111110011101000100000101";
char ack2[33] = "01001011110101010010111101010101";


int RS232_OpenComport(int *newport)
{
	int error;
	int baudr = B115200;
	int status;
	int cbits = CS8;			 //the number of data bits
	int cpar = 0, ipar = IGNPAR; //no parity bits are to be sent
	int bstop = 0;				 //the stop bit
	int port = open("/dev/ttyXRUSB0", O_RDWR | O_NOCTTY | O_NDELAY);
	if (port == -1)
	{
		perror("unable to open ttyXRUSB0 Trying 1 ");
		port = open("/dev/ttyXRUSB1", O_RDWR | O_NOCTTY | O_NDELAY);
			if (port == -1)
			{
				perror("unable to open ttyXRUSB1 ");
				return (1);
			}
		
	}
	/* lock access so that another process can't also use the port */
	if (flock(port, LOCK_EX | LOCK_NB) != 0)
	{
		close(port);
		perror("Another process has locked the comport.");
		return (1);
	}
	struct termios old_port_settings, new_port_settings;
	error = tcgetattr(port, &old_port_settings);
	if (error == -1)
	{
		close(port);
		flock(port, LOCK_UN); /* free the port so that others can use it. */
		perror("unable to read portsettings ");
		return (1);
	}
	memset(&new_port_settings, 0, sizeof(new_port_settings)); /* clear the new struct */
	new_port_settings.c_cflag = cbits | cpar | bstop | CLOCAL | CREAD;
	new_port_settings.c_iflag = ipar;
	new_port_settings.c_oflag = 0;
	new_port_settings.c_lflag = 0;
	new_port_settings.c_cc[VMIN] = 0;  /* block untill n bytes are received */
	new_port_settings.c_cc[VTIME] = 0; /* block untill a timer expires (n * 100 mSec.) */

	cfsetispeed(&new_port_settings, baudr);
	cfsetospeed(&new_port_settings, baudr);

	error = tcsetattr(port, TCSANOW, &new_port_settings);

	if (error == -1)
	{
		tcsetattr(port, TCSANOW, &old_port_settings);
		close(port);
		flock(port, LOCK_UN); /* free the port so that others can use it. */
		perror("unable to adjust portsettings ");
		return (1);
	}

	if (ioctl(port, TIOCMGET, &status) == -1)
	{
		tcsetattr(port, TCSANOW, &old_port_settings);
		flock(port, LOCK_UN); /* free the port so that others can use it. */
		perror("unable to get portstatus");
		return (1);
	}

	status |= TIOCM_DTR; /* turn on DTR */
	status |= TIOCM_RTS; /* turn on RTS */

	if (ioctl(port, TIOCMSET, &status) == -1)
	{
		tcsetattr(port, TCSANOW, &old_port_settings);
		flock(port, LOCK_UN); /* free the port so that others can use it. */
		perror("unable to set portstatus");
		return (1);
	}
	*newport = port;
	return (0);
}

int RS232_SendByte(int port, unsigned char byte)
{
	int n = write(port, &byte, 1);
	if (n < 0)
	{
		if (errno == EAGAIN)
		{
			return 2;
		}
		else
		{
			return 1;
		}
	}
	return 0;
}

int RS232_PollComport(int port, unsigned char *buf, int size)
{
	int n;
	n = read(port, buf, size);
	if (n < 0)
	{
		if (errno == EAGAIN)
			return 0;
	}
	return (n);
}


struct TrackInfo
{
    int cords[2];  //will store '22'
    int direction;  //will store direction
    int trackok;
    int nexthop;
};


char Xor(char a, char b)
{
    if ((a == '1') & (b == '1'))
    {
        return '0';
    }
    if ((a == '1') || (b == '1'))
    {
        return '1';
    }
    return '0';
}


void copy(char ar1[], char ar2[])
{
    int i = 0;
    while(ar1[i] != '\0')
    {
        ar2[i] = ar1[i];
        i++;
    }
}

void bitXor(char* a, char* b, char* ans)
{
    for(int i = 0; i < 32; i++)
    {
        ans[i] = Xor(a[i], b[i%4]);
    }
}

int toDecimal(char bin[], int len)
{
    int ans = 0;
    for(int i = 0; i < len; i++)
    {
        if(bin[i] == '1')
        {
            ans = 2 * ans + 1;
        }
        else
        {
            ans = 2 * ans;
        }
    }
    return ans;
}

void tochar(int num, char ans[], int len)
{
	// array to store binary number
	int binaryNum[len];
	for (int i = 0; i < len; i++)
	{
		binaryNum[i] = 0;
	}

	// counter for binary array
	int i = 0;
	while ((num > 0) && (i < len))
	{
		binaryNum[i] = num % 2;
		num = num / 2;
		i++;
	}
	for (int j = len - 1; j >= 0; j--)
	{
		ans[len - 1 - j] = binaryNum[j] + '0';
	}
}


void tochar3(int num, char ans[])
{
    char temp[5];
    tochar(num, temp, 4);
    for(int i = 0; i < 3; i++)
    {
        ans[i] = temp[i + 1];
    }
}

void encrypt(char c[], char k[])
{
	int n=0;
	for(int i=0; i<32;i++)
    	{
		if(k[i]=='1')
        {
            n++;
        }
	}
    char r[5];
    for(int i = 0; i < 4; i++){
    	r[i] = '0';
    }
    for(int i = 0; i < 32; i++)
    {
        r[i%4] = Xor(r[i%4], k[i]);
    }
	r[4] = '\0';
    for(int i = 0; i < n; i++)
    {
    	// printf("THE MF is %s %d \n", c, i);
    	// printf("1");
        
        bitXor(c, r, c);
	
        int n1 = toDecimal(r, 4);
        n1 = (n1 + 1) % 16;
        tochar(n1, r, 4);

    }
    printf("\n");
}


void decrypt(char c[], char k[])
{
	int n=0;
	for(int i=0; i<32;i++)
    {
		if(k[i]=='0')
        {
            n++;
        }
	}
    char r[5];
    for(int i = 0; i < 4; i++){
    	r[i] = '0';
    }
    for(int i = 0; i < 32; i++)
    {
        r[i%4] = Xor(r[i%4], k[i]);
    }
	r[4] = '\0';
        int n1 = toDecimal(r, 4);
        n1 = (n1 + 15) % 16;
        tochar(n1, r, 4);
    for(int i = 0; i < n; i++)
    {
    	// printf("NF is %s %d \n", c, i);
        
        bitXor(c, r, c);
        int n1 = toDecimal(r, 4);
        n1 = (n1 + 15) % 16;
        tochar(n1, r, 4);
    }
    printf("\n");
}

//struct TrackInfo database[];
//int LEN = 0;


const char* getfield(char* line, int num)
{
    const char* tok;
    for (tok = strtok(line, ",");
         tok && *tok;
         tok = strtok(NULL, ",\n"))
    {
        if (!--num)
            return tok;
    }
    return NULL;
}

void getData(int x, int y, char* info,int LEN, struct TrackInfo database[])
{
    //scan database and store the corresponding info the an array of TrackInfo
    for(int i = 0; i < LEN; i++)
    {
        if ((database[i].cords[0] == x) & (database[i].cords[1] == y))
        {
            int d = database[i].direction;
            info[8*d] = '1';
            if( database[i].trackok == 1)
            {
                info[8*d+1] = '1';
            }
            char nh[3];
            tochar3(database[i].nexthop, nh);
            for(int i = 0; i < 3; i++)
            {
                info[8*d+5+i] = nh[i];
            }
        }
    }
}


void hex2char(char hexDigit, char *c)
{
    int num;
	if ( hexDigit >= '0' && hexDigit <= '9' ) {
		num = (hexDigit - '0');
	} else if ( hexDigit >= 'a' && hexDigit <= 'f' ) {
		num = (hexDigit - 'a' + 10);
	} else if ( hexDigit >= 'A' && hexDigit <= 'F' ) {
		num = (hexDigit - 'A' + 10);
	}
    printf("%d\n", num);
    tochar(num, c, 4);
}


char char2hex(char *c)
{
    char hexDigit;
    char temp[5];
    for(int i = 0; i < 4; i++)
    {
        temp[i] = c[i];
    }
	temp[4] = '\0';
    int num = toDecimal(temp, 4);
    if (num >= 0 && num <= 9)
    {
        hexDigit = (char)('0' + num);
    }
    else if  (num >= 10 && num <= 15)
    {
        hexDigit = (char)('a' + (num - 10));
    }
    return hexDigit;
}


//converts the set of 32 bin array to one single uint8
void binchar2uint(char binarr[], uint8 *ans)
{
	for (int i = 0; i < 4; i++)
	{
		ans[i] = (uint8)toDecimal(&binarr[8 * i], 8);
	}
}

uint8 DATAPTRFORPARSE[1024];
int LENOFTHIS = 0;
char DATAINCHAR[1024];















bool sigIsRaised(void);
void sigRegisterHandler(void);

static const char *ptr;
static bool enableBenchmarking = false;

static bool isHexDigit(char ch) {
	return
		(ch >= '0' && ch <= '9') ||
		(ch >= 'a' && ch <= 'f') ||
		(ch >= 'A' && ch <= 'F');
}

static uint16 calcChecksum(const uint8 *data, size_t length) {
	uint16 cksum = 0x0000;
	while ( length-- ) {
		cksum = (uint16)(cksum + *data++);
	}
	return cksum;
}

static bool getHexNibble(char hexDigit, uint8 *nibble) {
	if ( hexDigit >= '0' && hexDigit <= '9' ) {
		*nibble = (uint8)(hexDigit - '0');
		return false;
	} else if ( hexDigit >= 'a' && hexDigit <= 'f' ) {
		*nibble = (uint8)(hexDigit - 'a' + 10);
		return false;
	} else if ( hexDigit >= 'A' && hexDigit <= 'F' ) {
		*nibble = (uint8)(hexDigit - 'A' + 10);
		return false;
	} else {
		return true;
	}
}

static int getHexByte(uint8 *byte) {
	uint8 upperNibble;
	uint8 lowerNibble;
	if ( !getHexNibble(ptr[0], &upperNibble) && !getHexNibble(ptr[1], &lowerNibble) ) {
		*byte = (uint8)((upperNibble << 4) | lowerNibble);
		byte += 2;
		return 0;
	} else {
		return 1;
	}
}

static bool checkEqual(char a[], char b[], int n){
	// printf("Trying to match ...\n");
	// printf("%s\n%s\n", a, b);
						
	for(int i = 0; i < n; i++){
		if(a[i] != b[i]){
			return false;
		}
	}
	return true;
}


static const char *const errMessages[] = {
	NULL,
	NULL,
	"Unparseable hex number",
	"Channel out of range",
	"Conduit out of range",
	"Illegal character",
	"Unterminated string",
	"No memory",
	"Empty string",
	"Odd number of digits",
	"Cannot load file",
	"Cannot save file",
	"Bad arguments"
};

typedef enum {
	FLP_SUCCESS,
	FLP_LIBERR,
	FLP_BAD_HEX,
	FLP_CHAN_RANGE,
	FLP_CONDUIT_RANGE,
	FLP_ILL_CHAR,
	FLP_UNTERM_STRING,
	FLP_NO_MEMORY,
	FLP_EMPTY_STRING,
	FLP_ODD_DIGITS,
	FLP_CANNOT_LOAD,
	FLP_CANNOT_SAVE,
	FLP_ARGS
} ReturnCode;

static ReturnCode doRead(
	struct FLContext *handle, uint8 chan, uint32 length, FILE *destFile, uint16 *checksum,
	const char **error)
{
    LENOFTHIS = 0;
	ReturnCode retVal = FLP_SUCCESS;
	uint32 bytesWritten;
	FLStatus fStatus;
	uint32 chunkSize;
	const uint8 *recvData;
	uint32 actualLength;
	const uint8 *ptr;
	uint16 csVal = 0x0000;
	#define READ_MAX 65536

	// Read first chunk
	chunkSize = length >= READ_MAX ? READ_MAX : length;
	fStatus = flReadChannelAsyncSubmit(handle, chan, chunkSize, NULL, error);
	CHECK_STATUS(fStatus, FLP_LIBERR, cleanup, "doRead()");
	length = length - chunkSize;

	while ( length ) {
		// Read chunk N
		chunkSize = length >= READ_MAX ? READ_MAX : length;
		fStatus = flReadChannelAsyncSubmit(handle, chan, chunkSize, NULL, error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup, "doRead()");
		length = length - chunkSize;
		
		// Await chunk N-1
		fStatus = flReadChannelAsyncAwait(handle, &recvData, &actualLength, &actualLength, error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup, "doRead()");

		// Write chunk N-1 to file
        for(int i = 0; i < actualLength; i++)
        {
            DATAPTRFORPARSE[LENOFTHIS] = recvData[i];
            LENOFTHIS++;
        }
		bytesWritten = (uint32)fwrite(recvData, 1, actualLength, destFile);
		CHECK_STATUS(bytesWritten != actualLength, FLP_CANNOT_SAVE, cleanup, "doRead()");

		// Checksum chunk N-1
		chunkSize = actualLength;
		ptr = recvData;
		while ( chunkSize-- ) {
			csVal = (uint16)(csVal + *ptr++);
		}
	}

	// Await last chunk
	fStatus = flReadChannelAsyncAwait(handle, &recvData, &actualLength, &actualLength, error);
	CHECK_STATUS(fStatus, FLP_LIBERR, cleanup, "doRead()");
	
	// Write last chunk to file
    for(int i = 0; i < actualLength; i++)
    {
        DATAPTRFORPARSE[LENOFTHIS] = recvData[i];
        LENOFTHIS++;
    }
	bytesWritten = (uint32)fwrite(recvData, 1, actualLength, destFile);
	CHECK_STATUS(bytesWritten != actualLength, FLP_CANNOT_SAVE, cleanup, "doRead()");

	// Checksum last chunk
	chunkSize = actualLength;
	ptr = recvData;
	while ( chunkSize-- ) {
		csVal = (uint16)(csVal + *ptr++);
	}
	
	// Return checksum to caller
	*checksum = csVal;
    printf("DATAPTRFORPARSE is %s and the len is %d\n", DATAPTRFORPARSE, LENOFTHIS);
cleanup:
	return retVal;
}

static ReturnCode doWrite(
	struct FLContext *handle, uint8 chan, FILE *srcFile, size_t *length, uint16 *checksum,
	const char **error)
{
	ReturnCode retVal = FLP_SUCCESS;
	size_t bytesRead, i;
	FLStatus fStatus;
	const uint8 *ptr;
	uint16 csVal = 0x0000;
	size_t lenVal = 0;
	#define WRITE_MAX (65536 - 5)
	uint8 buffer[WRITE_MAX];

	do {
		// Read Nth chunk
		bytesRead = fread(buffer, 1, WRITE_MAX, srcFile);
		if ( bytesRead ) {
			// Update running total
			lenVal = lenVal + bytesRead;

			// Submit Nth chunk
			fStatus = flWriteChannelAsync(handle, chan, bytesRead, buffer, error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup, "doWrite()");

			// Checksum Nth chunk
			i = bytesRead;
			ptr = buffer;
			while ( i-- ) {
				csVal = (uint16)(csVal + *ptr++);
			}
		}
	} while ( bytesRead == WRITE_MAX );

	// Wait for writes to be received. This is optional, but it's only fair if we're benchmarking to
	// actually wait for the work to be completed.
	fStatus = flAwaitAsyncWrites(handle, error);
	CHECK_STATUS(fStatus, FLP_LIBERR, cleanup, "doWrite()");

	// Return checksum & length to caller
	*checksum = csVal;
	*length = lenVal;
cleanup:
	return retVal;
}


void convert()
{
    for(int j = 0; j < LENOFTHIS; j++)
    {

        uint8 num = DATAPTRFORPARSE[j];
        for(int i = 0; i < 8; i++)
        {
            DATAINCHAR[8*(j+1) - i - 1] = num % 2 + '0';
            num /= 2;

        }
    }
    DATAINCHAR[8 * LENOFTHIS] = '\0';
}


static int parseLine(struct FLContext *handle, const char *line, const char **error) {
	ReturnCode retVal = FLP_SUCCESS, status;
	FLStatus fStatus;
	struct Buffer dataFromFPGA = {0,};
	BufferStatus bStatus;
	uint8 *data = NULL;
	char *fileName = NULL;
	FILE *file = NULL;
	double totalTime, speed;
	#ifdef WIN32
		LARGE_INTEGER tvStart, tvEnd, freq;
		DWORD_PTR mask = 1;
		SetThreadAffinityMask(GetCurrentThread(), mask);
		QueryPerformanceFrequency(&freq);
	#else
		struct timeval tvStart, tvEnd;
		long long startTime, endTime;
	#endif
	bStatus = bufInitialise(&dataFromFPGA, 1024, 0x00, error);
	CHECK_STATUS(bStatus, FLP_LIBERR, cleanup);
	ptr = line;
	do {
		while ( *ptr == ';' ) {
			ptr++;
		}
		switch ( *ptr ) {
		case 'r':{
			uint32 chan;
			uint32 length = 1;
			char *end;
			ptr++;
			
			// Get the channel to be read:
			errno = 0;
			chan = (uint32)strtoul(ptr, &end, 16);
			CHECK_STATUS(errno, FLP_BAD_HEX, cleanup);

			// Ensure that it's 0-127
			CHECK_STATUS(chan > 127, FLP_CHAN_RANGE, cleanup);
			ptr = end;

			// Only three valid chars at this point:
			CHECK_STATUS(*ptr != '\0' && *ptr != ';' && *ptr != ' ', FLP_ILL_CHAR, cleanup);

			if ( *ptr == ' ' ) {
				ptr++;

				// Get the read count:
				errno = 0;
				length = (uint32)strtoul(ptr, &end, 16);
				CHECK_STATUS(errno, FLP_BAD_HEX, cleanup);
				ptr = end;
				
				// Only three valid chars at this point:
				CHECK_STATUS(*ptr != '\0' && *ptr != ';' && *ptr != ' ', FLP_ILL_CHAR, cleanup);
				if ( *ptr == ' ' ) {
					const char *p;
					const char quoteChar = *++ptr;
					CHECK_STATUS(
						(quoteChar != '"' && quoteChar != '\''),
						FLP_ILL_CHAR, cleanup);
					
					// Get the file to write bytes to:
					ptr++;
					p = ptr;
					while ( *p != quoteChar && *p != '\0' ) {
						p++;
					}
					CHECK_STATUS(*p == '\0', FLP_UNTERM_STRING, cleanup);
					fileName = malloc((size_t)(p - ptr + 1));
					CHECK_STATUS(!fileName, FLP_NO_MEMORY, cleanup);
					CHECK_STATUS(p - ptr == 0, FLP_EMPTY_STRING, cleanup);
					strncpy(fileName, ptr, (size_t)(p - ptr));
					fileName[p - ptr] = '\0';
					ptr = p + 1;
				}
			}
			if ( fileName ) {
				uint16 checksum = 0x0000;

				// Open file for writing
				file = fopen(fileName, "wb");
				CHECK_STATUS(!file, FLP_CANNOT_SAVE, cleanup);
				free(fileName);
				fileName = NULL;

				#ifdef WIN32
					QueryPerformanceCounter(&tvStart);
					status = doRead(handle, (uint8)chan, length, file, &checksum, error);
					QueryPerformanceCounter(&tvEnd);
					totalTime = (double)(tvEnd.QuadPart - tvStart.QuadPart);
					totalTime /= freq.QuadPart;
					speed = (double)length / (1024*1024*totalTime);
				#else
					gettimeofday(&tvStart, NULL);
					status = doRead(handle, (uint8)chan, length, file, &checksum, error);
					gettimeofday(&tvEnd, NULL);
					startTime = tvStart.tv_sec;
					startTime *= 1000000;
					startTime += tvStart.tv_usec;
					endTime = tvEnd.tv_sec;
					endTime *= 1000000;
					endTime += tvEnd.tv_usec;
					totalTime = (double)(endTime - startTime);
					totalTime /= 1000000;  // convert from uS to S.
					speed = (double)length / (1024*1024*totalTime);
				#endif
				if ( enableBenchmarking ) {
					printf(
						"Read %d bytes (checksum 0x%04X) from channel %d at %f MiB/s\n",
						length, checksum, chan, speed);
				}
				CHECK_STATUS(status, status, cleanup);

				// Close the file
				fclose(file);
				file = NULL;
			} else {
				size_t oldLength = dataFromFPGA.length;
				bStatus = bufAppendConst(&dataFromFPGA, 0x00, length, error);
				CHECK_STATUS(bStatus, FLP_LIBERR, cleanup);
				#ifdef WIN32
					QueryPerformanceCounter(&tvStart);
					fStatus = flReadChannel(handle, (uint8)chan, length, dataFromFPGA.data + oldLength, error);
					QueryPerformanceCounter(&tvEnd);
					totalTime = (double)(tvEnd.QuadPart - tvStart.QuadPart);
					totalTime /= freq.QuadPart;
					speed = (double)length / (1024*1024*totalTime);
				#else
					gettimeofday(&tvStart, NULL);
					fStatus = flReadChannel(handle, (uint8)chan, length, dataFromFPGA.data + oldLength, error);
					gettimeofday(&tvEnd, NULL);
					startTime = tvStart.tv_sec;
					startTime *= 1000000;
					startTime += tvStart.tv_usec;
					endTime = tvEnd.tv_sec;
					endTime *= 1000000;
					endTime += tvEnd.tv_usec;
					totalTime = (double)(endTime - startTime);
					totalTime /= 1000000;  // convert from uS to S.
					speed = (double)length / (1024*1024*totalTime);
				#endif
				if ( enableBenchmarking ) {
					printf(
						"Read %d bytes (checksum 0x%04X) from channel %d at %f MiB/s\n",
						length, calcChecksum(dataFromFPGA.data + oldLength, length), chan, speed);
				}
				CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			}
			break;
		}
		case 'w':{
			unsigned long int chan;
			size_t length = 1, i;
			char *end, ch;
			const char *p;
			ptr++;
			
			// Get the channel to be written:
			errno = 0;
			chan = strtoul(ptr, &end, 16);
			CHECK_STATUS(errno, FLP_BAD_HEX, cleanup);

			// Ensure that it's 0-127
			CHECK_STATUS(chan > 127, FLP_CHAN_RANGE, cleanup);
			ptr = end;

			// There must be a space now:
			CHECK_STATUS(*ptr != ' ', FLP_ILL_CHAR, cleanup);

			// Now either a quote or a hex digit
		   ch = *++ptr;
			if ( ch == '"' || ch == '\'' ) {
				uint16 checksum = 0x0000;

				// Get the file to read bytes from:
				ptr++;
				p = ptr;
				while ( *p != ch && *p != '\0' ) {
					p++;
				}
				CHECK_STATUS(*p == '\0', FLP_UNTERM_STRING, cleanup);
				fileName = malloc((size_t)(p - ptr + 1));
				CHECK_STATUS(!fileName, FLP_NO_MEMORY, cleanup);
				CHECK_STATUS(p - ptr == 0, FLP_EMPTY_STRING, cleanup);
				strncpy(fileName, ptr, (size_t)(p - ptr));
				fileName[p - ptr] = '\0';
				ptr = p + 1;  // skip over closing quote

				// Open file for reading
				file = fopen(fileName, "rb");
				CHECK_STATUS(!file, FLP_CANNOT_LOAD, cleanup);
				free(fileName);
				fileName = NULL;
				
				#ifdef WIN32
					QueryPerformanceCounter(&tvStart);
					status = doWrite(handle, (uint8)chan, file, &length, &checksum, error);
					QueryPerformanceCounter(&tvEnd);
					totalTime = (double)(tvEnd.QuadPart - tvStart.QuadPart);
					totalTime /= freq.QuadPart;
					speed = (double)length / (1024*1024*totalTime);
				#else
					gettimeofday(&tvStart, NULL);
					status = doWrite(handle, (uint8)chan, file, &length, &checksum, error);
					gettimeofday(&tvEnd, NULL);
					startTime = tvStart.tv_sec;
					startTime *= 1000000;
					startTime += tvStart.tv_usec;
					endTime = tvEnd.tv_sec;
					endTime *= 1000000;
					endTime += tvEnd.tv_usec;
					totalTime = (double)(endTime - startTime);
					totalTime /= 1000000;  // convert from uS to S.
					speed = (double)length / (1024*1024*totalTime);
				#endif
				if ( enableBenchmarking ) {
					printf(
						"Wrote "PFSZD" bytes (checksum 0x%04X) to channel %lu at %f MiB/s\n",
						length, checksum, chan, speed);
				}
				CHECK_STATUS(status, status, cleanup);

				// Close the file
				fclose(file);
				file = NULL;
			} else if ( isHexDigit(ch) ) {
				// Read a sequence of hex bytes to write
				uint8 *dataPtr;
				p = ptr + 1;
				while ( isHexDigit(*p) ) {
					p++;
				}
				CHECK_STATUS((p - ptr) & 1, FLP_ODD_DIGITS, cleanup);
				length = (size_t)(p - ptr) / 2;
				data = malloc(length);
				dataPtr = data;
				for ( i = 0; i < length; i++ ) {
					getHexByte(dataPtr++);
					ptr += 2;
				}
				#ifdef WIN32
					QueryPerformanceCounter(&tvStart);
					fStatus = flWriteChannel(handle, (uint8)chan, length, data, error);
					QueryPerformanceCounter(&tvEnd);
					totalTime = (double)(tvEnd.QuadPart - tvStart.QuadPart);
					totalTime /= freq.QuadPart;
					speed = (double)length / (1024*1024*totalTime);
				#else
					gettimeofday(&tvStart, NULL);
					fStatus = flWriteChannel(handle, (uint8)chan, length, data, error);
					gettimeofday(&tvEnd, NULL);
					startTime = tvStart.tv_sec;
					startTime *= 1000000;
					startTime += tvStart.tv_usec;
					endTime = tvEnd.tv_sec;
					endTime *= 1000000;
					endTime += tvEnd.tv_usec;
					totalTime = (double)(endTime - startTime);
					totalTime /= 1000000;  // convert from uS to S.
					speed = (double)length / (1024*1024*totalTime);
				#endif
				if ( enableBenchmarking ) {
					printf(
						"Wrote "PFSZD" bytes (checksum 0x%04X) to channel %lu at %f MiB/s\n",
						length, calcChecksum(data, length), chan, speed);
				}
				CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
				free(data);
				data = NULL;
			} else {
				FAIL(FLP_ILL_CHAR, cleanup);
			}
			break;
		}
		case '+':{
			uint32 conduit;
			char *end;
			ptr++;

			// Get the conduit
			errno = 0;
			conduit = (uint32)strtoul(ptr, &end, 16);
			CHECK_STATUS(errno, FLP_BAD_HEX, cleanup);

			// Ensure that it's 0-127
			CHECK_STATUS(conduit > 255, FLP_CONDUIT_RANGE, cleanup);
			ptr = end;

			// Only two valid chars at this point:
			CHECK_STATUS(*ptr != '\0' && *ptr != ';', FLP_ILL_CHAR, cleanup);

			fStatus = flSelectConduit(handle, (uint8)conduit, error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			break;
		}
		default:
			FAIL(FLP_ILL_CHAR, cleanup);
		}
	} while ( *ptr == ';' );
	CHECK_STATUS(*ptr != '\0', FLP_ILL_CHAR, cleanup);

	dump(0x00000000, dataFromFPGA.data, dataFromFPGA.length);

cleanup:
	bufDestroy(&dataFromFPGA);
	if ( file ) {
		fclose(file);
	}
	free(fileName);
	free(data);
	if ( retVal > FLP_LIBERR ) {
		const int column = (int)(ptr - line);
		int i;
		fprintf(stderr, "%s at column %d\n  %s\n  ", errMessages[retVal], column, line);
		for ( i = 0; i < column; i++ ) {
			fprintf(stderr, " ");
		}
		fprintf(stderr, "^\n");
	}
	return retVal;
}

static const char *nibbles[] = {
	"0000",  // '0'
	"0001",  // '1'
	"0010",  // '2'
	"0011",  // '3'
	"0100",  // '4'
	"0101",  // '5'
	"0110",  // '6'
	"0111",  // '7'
	"1000",  // '8'
	"1001",  // '9'

	"XXXX",  // ':'
	"XXXX",  // ';'
	"XXXX",  // '<'
	"XXXX",  // '='
	"XXXX",  // '>'
	"XXXX",  // '?'
	"XXXX",  // '@'

	"1010",  // 'A'
	"1011",  // 'B'
	"1100",  // 'C'
	"1101",  // 'D'
	"1110",  // 'E'
	"1111"   // 'F'
};



int updateTable(struct TrackInfo *tarr, char *arr, int cx, int cy, int len)
{
    // if(origlen > 256)
    // {
    //  printf("Length of the array is wrong, please check")
    //  return origlen;
    // }
    char cdir[4];
    for(int i = 0; i < 3; i++)
    {
        cdir[i] = arr[i];
    }
    cdir[3] = '\0';
    int dir = toDecimal(cdir, 3);
    for(int i = 0; i < len; i++)
    {
        if((tarr[i].cords[0] == cx ) && (tarr[i].cords[1] == cy) && (tarr[i].direction == dir))
        {
            printf("Found info in the array,now updating it\n");
            int cnext[4];
            cnext[3] = '\0';
            for(int i = 0; i < 3; i++)
            {
                cnext[i] = arr[5 + i];
            }
            int next = toDecimal(cnext, 3);
            int ntrackok = arr[4] - '0';
            tarr[i].nexthop = next;
            tarr[i].trackok = ntrackok;
            return 1;
        }
    }
    return -1;
}


static int readnBytes(int chan, char binArr[], struct FLContext *handle, const char **error, int nBytes){
	char command[100];
	int ret = sprintf(command, "r%d %d \"sample.out\"", chan, nBytes);
	ReturnCode pStatus = parseLine(handle, command, error);
	char haha[33];
    convert();
    printf("THE DATAINCHAR is %s\n", DATAINCHAR);

    for(int i = 0; i < 8 * nBytes; i++)
    {
        binArr[i] = DATAINCHAR[i];
    }	

	// 



	
	return pStatus;

}


static int write4(int chan, char binArr[], struct FLContext *handle, const char **error){
	char hex[9];
    printf("Writing %s", binArr);
    char* p = binArr;
    for(int i = 0; i < 8; i++)
    {
        hex[i] = char2hex(p);
        p += 4;
    }
    
    hex[8] = '\0';
    printf("the hex is %s\n", hex);
    char command2[20];
    command2[0] = 'w';

    int ind = 1;
    if(chan < 10){
    	command2[ind] = chan + '0';
    }
    else if(chan < 100){
    	command2[ind] =  (chan / 10) + '0';
    	command2[ind + 1] = (chan % 10) + '0';
    	ind++;
    }
    else{
    	command2[ind] =  (chan / 100) + '0';
    	command2[ind + 1] = ((chan % 100) / 10) + '0';
    	command2[ind + 2] = (chan % 10) + '0';
    	ind += 2;	
    }

    command2[++ind] = ' ';
    ind++;
    for(int i = 0; i < 9; i++)
    {
        command2[ind + i] = hex[i];
    }
    command2[ind + 9] = '\0';
    printf("Writing the coordinates back %s\n", command2);

	ReturnCode pStatus = parseLine(handle, command2, error);
	printf("Wrote the coordinates back\n");
	return pStatus;

}





int main(int argc, char *argv[])
{
	ReturnCode retVal = FLP_SUCCESS, pStatus;
	struct arg_str *ivpOpt = arg_str0("i", "ivp", "<VID:PID>", "            vendor ID and product ID (e.g 04B4:8613)");
	struct arg_str *vpOpt = arg_str1("v", "vp", "<VID:PID[:DID]>", "       VID, PID and opt. dev ID (e.g 1D50:602B:0001)");
	struct arg_str *fwOpt = arg_str0("f", "fw", "<firmware.hex>", "        firmware to RAM-load (or use std fw)");
	struct arg_str *portOpt = arg_str0("d", "ports", "<bitCfg[,bitCfg]*>", " read/write digital ports (e.g B13+,C1-,B2?)");
	struct arg_str *queryOpt = arg_str0("q", "query", "<jtagBits>", "         query the JTAG chain");
	struct arg_str *progOpt = arg_str0("p", "program", "<config>", "         program a device");
	struct arg_uint *conOpt = arg_uint0("c", "conduit", "<conduit>", "        which comm conduit to choose (default 0x01)");
	struct arg_str *actOpt = arg_str0("a", "action", "<actionString>", "    a series of CommFPGA actions");
	struct arg_lit *shellOpt = arg_lit0("s", "shell", "                    start up an interactive CommFPGA session");
	struct arg_lit *benOpt = arg_lit0("b", "benchmark", "                enable benchmarking & checksumming");
	struct arg_lit *rstOpt = arg_lit0("r", "reset", "                    reset the bulk endpoints");
	struct arg_str *dumpOpt = arg_str0("l", "dumploop", "<ch:file.bin>", "   write data from channel ch to file");
	struct arg_lit *helpOpt = arg_lit0("h", "help", "                     print this help and exit");
	struct arg_str *eepromOpt = arg_str0(NULL, "eeprom", "<std|fw.hex|fw.iic>", "   write firmware to FX2's EEPROM (!!)");
	struct arg_str *backupOpt = arg_str0(NULL, "backup", "<kbitSize:fw.iic>", "     backup FX2's EEPROM (e.g 128:fw.iic)\n");
	struct arg_lit *trackCommOpt = arg_lit0("z", "trackcomm", "             will start the comm with the fpga for trackIfo");
	struct arg_end *endOpt = arg_end(20);
	void *argTable[] = {
		ivpOpt, vpOpt, fwOpt, portOpt, queryOpt, progOpt, conOpt, actOpt,
		shellOpt, benOpt, rstOpt, dumpOpt, helpOpt, eepromOpt, backupOpt, trackCommOpt, endOpt};
	const char *progName = "flcli";
	int numErrors;
	struct FLContext *handle = NULL;
	FLStatus fStatus;
	const char *error = NULL;
	const char *ivp = NULL;
	const char *vp = NULL;
	bool isNeroCapable, isCommCapable;
	uint32 numDevices, scanChain[16], i;
	const char *line = NULL;
	uint8 conduit = 0x01;

	int length = 0, length1 = 0;

	FILE *stream = fopen("track_data.csv", "r");

	char line2[1024];
	while (fgets(line2, 1024, stream))
	{
		length = length + 1;
	}

	struct TrackInfo database[length + 1];

	FILE *stream1 = fopen("track_data.csv", "r");
	char line1[1024];
	char temp[1024];
	while (fgets(line1, 1024, stream1))
	{
		if (length1 == length)
		{
			break;
		}
		copy(line1, temp);
		database[length1].cords[0] = *getfield(temp, 1) - 48;
		copy(line1, temp);
		database[length1].cords[1] = *getfield(temp, 2) - 48;
		copy(line1, temp);
		database[length1].direction = *getfield(temp, 3) - 48;
		printf("%d\n", database[length1].direction);
		copy(line1, temp);
		database[length1].trackok = *getfield(temp, 4) - 48;
		copy(line1, temp);
		database[length1].nexthop = *getfield(temp, 5) - 48;
		// NOTE strtok clobbers tmp
		length1 += 1;
	}

	if (arg_nullcheck(argTable) != 0)
	{
		fprintf(stderr, "%s: insufficient memory\n", progName);
		FAIL(1, cleanup);
	}

	numErrors = arg_parse(argc, argv, argTable);

	if (helpOpt->count > 0)
	{
		printf("FPGALink Command-Line Interface Copyright (C) 2012-2014 Chris McClelland\n\nUsage: %s", progName);
		arg_print_syntax(stdout, argTable, "\n");
		printf("\nInteract with an FPGALink device.\n\n");
		arg_print_glossary(stdout, argTable, "  %-10s %s\n");
		FAIL(FLP_SUCCESS, cleanup);
	}

	if (numErrors > 0)
	{
		arg_print_errors(stdout, endOpt, progName);
		fprintf(stderr, "Try '%s --help' for more information.\n", progName);
		FAIL(FLP_ARGS, cleanup);
	}

	fStatus = flInitialise(0, &error);
	CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);

	vp = vpOpt->sval[0];

	printf("Attempting to open connection to FPGALink device %s...\n", vp);
	fStatus = flOpen(vp, &handle, NULL);
	if (fStatus)
	{
		if (ivpOpt->count)
		{
			int count = 60;
			uint8 flag;
			ivp = ivpOpt->sval[0];
			printf("Loading firmware into %s...\n", ivp);
			if (fwOpt->count)
			{
				fStatus = flLoadCustomFirmware(ivp, fwOpt->sval[0], &error);
			}
			else
			{
				fStatus = flLoadStandardFirmware(ivp, vp, &error);
			}
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);

			printf("Awaiting renumeration");
			flSleep(1000);
			do
			{
				printf(".");
				fflush(stdout);
				fStatus = flIsDeviceAvailable(vp, &flag, &error);
				CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
				flSleep(250);
				count--;
			} while (!flag && count);
			printf("\n");
			if (!flag)
			{
				fprintf(stderr, "FPGALink device did not renumerate properly as %s\n", vp);
				FAIL(FLP_LIBERR, cleanup);
			}

			printf("Attempting to open connection to FPGLink device %s again...\n", vp);
			fStatus = flOpen(vp, &handle, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
		}
		else
		{
			fprintf(stderr, "Could not open FPGALink device at %s and no initial VID:PID was supplied\n", vp);
			FAIL(FLP_ARGS, cleanup);
		}
	}

	printf(
		"Connected to FPGALink device %s (firmwareID: 0x%04X, firmwareVersion: 0x%08X)\n",
		vp, flGetFirmwareID(handle), flGetFirmwareVersion(handle));

	if (eepromOpt->count)
	{
		if (!strcmp("std", eepromOpt->sval[0]))
		{
			printf("Writing the standard FPGALink firmware to the FX2's EEPROM...\n");
			fStatus = flFlashStandardFirmware(handle, vp, &error);
		}
		else
		{
			printf("Writing custom FPGALink firmware from %s to the FX2's EEPROM...\n", eepromOpt->sval[0]);
			fStatus = flFlashCustomFirmware(handle, eepromOpt->sval[0], &error);
		}
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
	}

	if (backupOpt->count)
	{
		const char *fileName;
		const uint32 kbitSize = strtoul(backupOpt->sval[0], (char **)&fileName, 0);
		if (*fileName != ':')
		{
			fprintf(stderr, "%s: invalid argument to option --backup=<kbitSize:fw.iic>\n", progName);
			FAIL(FLP_ARGS, cleanup);
		}
		fileName++;
		printf("Saving a backup of %d kbit from the FX2's EEPROM to %s...\n", kbitSize, fileName);
		fStatus = flSaveFirmware(handle, kbitSize, fileName, &error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
	}

	if (rstOpt->count)
	{
		// Reset the bulk endpoints (only needed in some virtualised environments)
		fStatus = flResetToggle(handle, &error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
	}

	if (conOpt->count)
	{
		conduit = (uint8)conOpt->ival[0];
	}

	isNeroCapable = flIsNeroCapable(handle);
	isCommCapable = flIsCommCapable(handle, conduit);

	if (portOpt->count)
	{
		uint32 readState;
		char hex[9];
		const uint8 *p = (const uint8 *)hex;
		printf("Configuring ports...\n");
		fStatus = flMultiBitPortAccess(handle, portOpt->sval[0], &readState, &error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
		sprintf(hex, "%08X", readState);
		printf("Readback:   28   24   20   16    12    8    4    0\n          %s", nibbles[*p++ - '0']);
		printf(" %s", nibbles[*p++ - '0']);
		printf(" %s", nibbles[*p++ - '0']);
		printf(" %s", nibbles[*p++ - '0']);
		printf("  %s", nibbles[*p++ - '0']);
		printf(" %s", nibbles[*p++ - '0']);
		printf(" %s", nibbles[*p++ - '0']);
		printf(" %s\n", nibbles[*p++ - '0']);
		flSleep(100);
	}

	if (queryOpt->count)
	{
		if (isNeroCapable)
		{
			fStatus = flSelectConduit(handle, 0x00, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			fStatus = jtagScanChain(handle, queryOpt->sval[0], &numDevices, scanChain, 16, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			if (numDevices)
			{
				printf("The FPGALink device at %s scanned its JTAG chain, yielding:\n", vp);
				for (i = 0; i < numDevices; i++)
				{
					printf("  0x%08X\n", scanChain[i]);
				}
			}
			else
			{
				printf("The FPGALink device at %s scanned its JTAG chain but did not find any attached devices\n", vp);
			}
		}
		else
		{
			fprintf(stderr, "JTAG chain scan requested but FPGALink device at %s does not support NeroProg\n", vp);
			FAIL(FLP_ARGS, cleanup);
		}
	}

	if (progOpt->count)
	{
		printf("Programming device...\n");
		if (isNeroCapable)
		{
			fStatus = flSelectConduit(handle, 0x00, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			fStatus = flProgram(handle, progOpt->sval[0], NULL, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
		}
		else
		{
			fprintf(stderr, "Program operation requested but device at %s does not support NeroProg\n", vp);
			FAIL(FLP_ARGS, cleanup);
		}
	}

	if (benOpt->count)
	{
		enableBenchmarking = true;
	}

	if (actOpt->count)
	{
		printf("Executing CommFPGA actions on FPGALink device %s...\n", vp);
		if (isCommCapable)
		{
			uint8 isRunning;
			fStatus = flSelectConduit(handle, conduit, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			fStatus = flIsFPGARunning(handle, &isRunning, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			if (isRunning)
			{
				pStatus = parseLine(handle, actOpt->sval[0], &error);
				CHECK_STATUS(pStatus, pStatus, cleanup);
			}
			else
			{
				fprintf(stderr, "The FPGALink device at %s is not ready to talk - did you forget --program?\n", vp);
				FAIL(FLP_ARGS, cleanup);
			}
		}
		else
		{
			fprintf(stderr, "Action requested but device at %s does not support CommFPGA\n", vp);
			FAIL(FLP_ARGS, cleanup);
		}
	}

	if (dumpOpt->count)
	{
		const char *fileName;
		unsigned long chan = strtoul(dumpOpt->sval[0], (char **)&fileName, 10);
		FILE *file = NULL;
		const uint8 *recvData;
		uint32 actualLength;
		if (*fileName != ':')
		{
			fprintf(stderr, "%s: invalid argument to option -l|--dumploop=<ch:file.bin>\n", progName);
			FAIL(FLP_ARGS, cleanup);
		}
		fileName++;
		printf("Copying from channel %lu to %s", chan, fileName);
		file = fopen(fileName, "wb");
		CHECK_STATUS(!file, FLP_CANNOT_SAVE, cleanup);
		sigRegisterHandler();
		fStatus = flSelectConduit(handle, conduit, &error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
		fStatus = flReadChannelAsyncSubmit(handle, (uint8)chan, 22528, NULL, &error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
		do
		{
			fStatus = flReadChannelAsyncSubmit(handle, (uint8)chan, 22528, NULL, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			fStatus = flReadChannelAsyncAwait(handle, &recvData, &actualLength, &actualLength, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			fwrite(recvData, 1, actualLength, file);
			printf(".");
		} while (!sigIsRaised());
		printf("\nCaught SIGINT, quitting...\n");
		fStatus = flReadChannelAsyncAwait(handle, &recvData, &actualLength, &actualLength, &error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
		fwrite(recvData, 1, actualLength, file);
		fclose(file);
	}

	if (trackCommOpt->count > 0)
	{
		int channel_for_read = 0;
		int x = 0;
		int y = 0;
		uint8 isRunning;
		fStatus = flSelectConduit(handle, conduit, &error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
		fStatus = flIsFPGARunning(handle, &isRunning, &error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);

		char dirs[65] = "0000000000001000000100000001100000100000001010000011000000111000";
		char sentinel[9] = "10111011";
		char flag_s3[9] = "10101000";
		char flag_s4[9] = "11000100";
		char flag_none[9] = "00001000"; 
		char data[65];
		char data2[33];
		char binArr[33];
		char bin8[9];
		int counter = 0;
		int newport;
		// RS232_OpenComport(&newport);
		int state = -1;
		int mState = 1;
		char enAck2[33];
		for (int i = 0; i < 32; i++)
		{
			enAck2[i] = ack2[i];
		}
		enAck2[32] = '\0';
		char data1[33];
		encrypt(enAck2, key);
		sleep(1);
		unsigned char to_send;
		while (true)
		{
			printf("3\n");
			fflush(stdout);
			switch (state)
			{

			/* Reset state */
			case -1:

				channel_for_read = 0;
				x = 0;
				y = 0;
				for (int i = 0; i < 64; i++)
				{
					data[i] = dirs[i];
				}

				data[64] = '\0';
				state = 0;
				counter = 0;
				mState = 1;
				break;

			case 0:
				printf("2\n");
				fflush(stdout);
				for (int i = 0; i < 64; i = (i + 1) % 64)
				{

					// printf("lol\n");
					/* Read from channel 2i */
					printf("Trying %d %d", 2 * i, 2 * i + 1);
					fflush(stdout);
					pStatus = readnBytes(2 * i, binArr, handle, &error, 4);
					// printf("The binArr is %s\n", binArr);
					for (int i = 0; i < 32; i++)
					{
						data2[i] = binArr[i];
					}
					data2[32] = '\0';
					decrypt(binArr, key);
					printf("The binArr is %s\n", binArr);

					char cx[5], cy[5];
					cx[0] = binArr[24];
					cx[1] = binArr[25];
					cx[2] = binArr[26];
					cx[3] = binArr[27];
					cx[4] = '\0';
					cy[4] = '\0';
					cy[0] = binArr[28];
					cy[1] = binArr[29];
					cy[2] = binArr[30];
					cy[3] = binArr[31];
					// printf("the cordinates are x ; %s and y : %s\n", cx, cy);
					// printf("%s encrypted binarr\n", binArr);

					pStatus = write4(2 * i + 1, data2, handle, &error);

					/* Listen on the channel */
					sleep(1);
					pStatus = readnBytes(2 * i, binArr, handle, &error, 4);
					decrypt(binArr, key);
					printf("Checking\n");

					if (checkEqual(binArr, ack1, 32))
					{

						x = toDecimal(cx, 4);
						y = toDecimal(cy, 4);
						getData(x, y, data, length, database);
						printf("coordinates are: HHHH %d %d\n", x , y);
						channel_for_read = 2 * i;
						state = 1;
						break;
						printf("Success\n");
					}
					else
					{
						sleep(5);
						pStatus = readnBytes(2 * i, binArr, handle, &error, 4);
						decrypt(binArr, key);
						printf("Checking\n");
						if (checkEqual(binArr, ack1, 32))
						{
							x = toDecimal(cx, 4);
							y = toDecimal(cy, 4);
							getData(x, y, data, length, database);
							printf("coordinates are: HHHH %d %d\n", x , y);
							channel_for_read = 2 * i;
							state = 1;
							printf("Success\n");
						}
						else
						{

							printf("Fail\n");
							// continue;
						}
					}
				}
				printf("2");
				break;

			case 1:
				printf("1");
				pStatus = write4(channel_for_read + 1, enAck2, handle, &error);
				

				for (int i = 0; i < 32; i++)
				{
					data1[i] = data[i];
				}
				data1[32] = '\0';
				encrypt(data1, key);
				pStatus = write4(channel_for_read + 1, data1, handle, &error);
				state = 2;
				break;

			case 2:
				pStatus = readnBytes(channel_for_read, binArr, handle, &error, 4);
				decrypt(binArr, key);

				if (checkEqual(binArr, ack1, 32))
				{
					state = 3;
					counter = 0;
				}
				else if (counter > 256)
				{
					state = -1;
				}
				else
				{
					counter++;
					sleep(1);
				}
				break;

			case 3:
				for (int i = 0; i < 32; i++)
				{
					data1[i] = data[32 + i];
				}
				data1[32] = '\0';
				encrypt(data1, key);
				pStatus = write4(channel_for_read + 1, data1, handle, &error);
				state = 4;
				break;

			case 4:
				pStatus = readnBytes(channel_for_read, binArr, handle, &error, 4);
				printf("In 4\n");
				decrypt(binArr, key);
				printf("BIN: %s\n", binArr);
				printf("ACK1: %s\n", ack1);
				if (checkEqual(binArr, ack1, 32))
				{
					printf("Nice!!\n");
					state = 5;
					counter = 0;
				}
				else if (counter > 256)
				{
					state = -1;
				}
				else
				{
					counter++;
					sleep(1); 
				}
				break;

			case 5:
				printf("WHOOHOOHFHHHHHHHHHHHHHHHHHHHHHHHH\n");	
				pStatus = write4(channel_for_read + 1, enAck2, handle, &error);
				state = 7;
				mState = 1;
				break;

			case 6:
				printf("Do you have data to send over uart? (y/n)");
				char resp;
				scanf("%c", &resp);
				switch (resp){
					case 'y':
						printf("Enter the byte to send");
						char bin_to_send[9];
						scanf("%s", &bin_to_send);
						bin_to_send[8] = '\0';
						to_send = toDecimal(bin_to_send, 8);
						printf("%c\n", to_send);
						state = 7;
						mState = 1;
						counter = 0;
					break;

					default:
						state = 7;
						mState = 1;
						counter = 0;
				}
				break;

			case 7:
				printf("mState: %d\n", mState);
				fflush(stdin);
				switch(mState){
					case 1:
						counter ++;
						pStatus = readnBytes(channel_for_read, bin8, handle, &error, 1);
						if(counter > 75){
							state = -1;
							counter = 0;
						}
						else if(checkEqual(bin8, flag_s3, 8)){
							mState = 2;
							counter = 0;
						}
						else if(checkEqual(bin8, flag_s4, 8)){
							state = 8;
							mState = 1;
							counter = 0;
						}
						else if(checkEqual(bin8, flag_none, 8)){
							state = 9;
							counter = 0;
				
						}
						else{
							sleep(1);
						}
					break;
					case 2:
						pStatus = readnBytes(channel_for_read, bin8, handle, &error, 1);
						counter = counter + 1;
						if(counter > 75){
							state = -1;
						}
							
						else if(checkEqual(bin8, sentinel, 8)){
							mState = 3;
							counter = 0;
							//sleep(1);
						}
						else{
							sleep(1);
						}
					break;
					case 3:
						pStatus = readnBytes(channel_for_read, binArr, handle, &error, 4);
						printf("Before decryption %s\n", binArr);
						decrypt(binArr, key);
						char read_byte[9];
						for(int i = 0; i < 8; i++)
							read_byte[i] = binArr[24 + i];
						read_byte[8] = '\n';
						printf("%s Data read from fpga %s\n", read_byte, binArr);
						updateTable(database, read_byte, x, y, length);
						mState = 4;
					break;

					case 4:
						pStatus = readnBytes(channel_for_read, bin8, handle, &error, 1);
						counter = counter + 1;
						if(counter > 75){
							state = -1;
						}
							
						else if(checkEqual(bin8, flag_s4, 8)){
							state = 8;
							mState = 1;
							counter = 0;
						}
						else if(checkEqual(bin8, flag_none, 8)){
							state = 9;
							counter = 0;
						}
						else{
							sleep(1);
						}
					break;

				}

				break;



			case 8: ; //empty statement
				unsigned char buf;
				int n_read_bytes = 1;
				// int n_read_bytes = RS232_PollComport(newport, &buf, 1);
				if(n_read_bytes == 1){
					int n = buf;
					char arr[9];
					tochar(n, arr, 8);
					arr[8] = '\0';
					printf("%s", arr);
					state = 9;
				}
				else {
					sleep(1);
				}
				break;

			case 9:
				sleep(1);
				// int ret = RS232_SendByte(newport, to_send);
				// printf("%d Sent through UART jhkh\n", ret);
				state = -1;
				sleep(32);



			default:
				printf("Something went wrong");
			}
		}
	}

	if (shellOpt->count)
	{
		printf("\nEntering CommFPGA command-line mode:\n");
		if (isCommCapable)
		{
			uint8 isRunning;
			fStatus = flSelectConduit(handle, conduit, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			fStatus = flIsFPGARunning(handle, &isRunning, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			if (isRunning)
			{
				do
				{
					do
					{
						line = readline("> ");
					} while (line && !line[0]);
					if (line && line[0] && line[0] != 'q')
					{
						add_history(line);
						pStatus = parseLine(handle, line, &error);
						CHECK_STATUS(pStatus, pStatus, cleanup);
						free((void *)line);
					}
				} while (line && line[0] != 'q');
			}
			else
			{
				fprintf(stderr, "The FPGALink device at %s is not ready to talk - did you forget --xsvf?\n", vp);
				FAIL(FLP_ARGS, cleanup);
			}
		}
		else
		{
			fprintf(stderr, "Shell requested but device at %s does not support CommFPGA\n", vp);
			FAIL(FLP_ARGS, cleanup);
		}
	}

cleanup:
	free((void *)line);
	flClose(handle);
	if (error)
	{
		fprintf(stderr, "%s\n", error);
		flFreeError(error);
	}
	return retVal;
}
