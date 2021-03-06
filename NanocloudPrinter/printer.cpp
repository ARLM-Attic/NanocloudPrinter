/**
	@file
	@brief Convert postscript from stdin (sent by RedMon) to a pdf file. Path is then sent to Photon
*/

#include "stdafx.h"

#include "iapi.h"
#include <shellapi.h>
#include <errno.h>
#include <iostream>
#include <ctime>
#include <fstream>
#include <stdio.h>
#include <cstdlib>
#include <cstring>
#include <boost/asio.hpp>
#include "Helpers.h"
#include <io.h>

using boost::asio::ip::tcp;

#define PRODUCT_NAME	"Nanocloud Printer"

/// Input file pointer
FILE* fileInput;
/// Initial input buffer
char cBuffer[MAX_PATH * 2 + 1];
/// Length of data in initial buffer
int nBuffer = 0;
/// Current location in the initial buffer
int nInBuffer = 0;
/// Size of error string buffer
#define MAX_ERR		1023
/// Error string buffer
char cErr[MAX_ERR + 1];

#define TEMP_FILENAME "print_"
#define TEMP_EXTENSION "pdf"

void CleanTempFiles()
{
	char sTempFolder[MAX_PATH];
	GetTempPath(MAX_PATH, sTempFolder);

	char sFileToFind[MAX_PATH];
	sprintf_s (sFileToFind, "%s%s*.%s", sTempFolder, TEMP_FILENAME, TEMP_EXTENSION);

	struct _finddata64i32_t finddata;
	intptr_t hFind = _findfirst (sFileToFind, &finddata);
	int ret = (int)hFind;
	char sFullFilename[MAX_PATH];

	while (ret != -1) {
		sprintf_s (sFullFilename, "%s%s", sTempFolder, finddata.name);
		DeleteFile (sFullFilename);						// Deliberately ignore the return value
		ret = _findnext (hFind, &finddata);		// Find the next file
	}
	
	_findclose (hFind);
}

/**
	@brief Generate random name for output pdf file
	@param s Buffer to fill with random string
	@param len Length of random string to generate
	@return Path for output file with random filename
*/
std::string getTmpPath() {
	const int	randomLen = 6;
	std::string path;
	
	path = "C:\\Windows\\Temp\\";
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";

	for (int i = 0; i < randomLen; ++i) {
		path += alphanum[rand() % (sizeof(alphanum) - 1)];
	}
	path += ".pdf";
	return path;
}

/**
	@brief Callback function used by GhostScript to retrieve more data from the input buffer; stops at newlines
	@param instance Pointer to the GhostScript instance (not used)
	@param buf Buffer to fill with data
	@param len Length of requested data
	@return Size of retrieved data (in bytes), 0 when there's no more data
*/
static int GSDLLCALL my_in(void *instance, char *buf, int len)
{
	// Initialize variables
    int ch;
    int count = 0;
	char* pStart = buf;
	// Read until we reached the wanted size...
    while (count < len) 
	{
		// Is there still data in the initial buffer?
		if (nBuffer > nInBuffer)
			// Yes, read from there
			ch = cBuffer[nInBuffer++];
		else
			// No, get more data
			ch = fgetc(fileInput);
		if (ch == EOF)
			// That's it
			return 0;
		// Put the character in the buffer and increate the countn
		*buf++ = ch;
		count++;
		if (ch == '\n')
			// Stop on newlines
			break;
    }
	// That's it
    return count;
}

/**
	@brief Callback function used by GhostScript to output notes and warnings
	@param instance Pointer to the GhostScript instance (not used)
	@param str String to output
	@param len Length of output
	@return Count of characters written
*/
static int GSDLLCALL my_out(void *instance, const char *str, int len)
{
    return len;
}

/**
	@brief Callback function used by GhostScript to output errors
	@param instance Pointer to the GhostScript instance (not used)
	@param str Error string
	@param len Length of string
	@return Count of characters written
*/
static int GSDLLCALL my_err(void *instance, const char *str, int len)
{
	// Keep the error in cErr for later handling
	int nAdd = min(len, (int)(MAX_ERR - strlen(cErr)));
	strncat_s(cErr, str, MAX_ERR);
	// OK
    return len;
}

/**
	Reads all the data from the input (so no error will be raised if application
	ends without sending the data to ghostscript)
*/
void CleanInput()
{
	char cBuffer[1024];
	while (fread(cBuffer, 1, 1024, fileInput) > 0)
		;
}

/// Command line options used by GhostScript
const char* ARGS[] =
{
	"PS2PDF",
	"-dNOPAUSE",
	"-dBATCH",
    "-dSAFER",
    "-sDEVICE=pdfwrite",
	"-sOutputFile=c:\\test.pdf",
	"-I.\\",
    "-c",
    ".setpdfwrite",
	"-"
};

/**
	@brief Main function
	@param hInstance Handle to the current instance
	@param hPrevInstance Handle to the previous running instance (not used)
	@param lpCmdLine Command line (not used)
	@param nCmdShow Initial window visibility and location flag (not used)
	@return 0 if all went well, other values upon errors
*/
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	srand(time(NULL));
	// Initialize stuff
	std::string randomPath = getTmpPath();
	const char *constRandomPath = randomPath.c_str();
	char cPath[MAX_PATH + 1];
	char cFile[MAX_PATH + 128];
	char cInclude[3 * MAX_PATH + 7];
	cErr[0] = '\0';

	// Delete whichever temp files might exist
	CleanTempFiles();
	// Add the include directories to the command line flags we'll use with GhostScript:
	if (::GetModuleFileName(NULL, cPath, MAX_PATH))
	{
		// Should be next to the application
		char* pPos = strrchr(cPath, '\\');
		if (pPos != NULL)
			*(pPos) = '\0';
		else
			cPath[0] = '\0';
		// OK, add the fonts and lib folders:
		sprintf_s(cInclude, sizeof(cInclude), "-I%s\\urwfonts;%s\\lib", cPath, cPath);
		ARGS[6] = cInclude;
	}

	// Get the data from stdin (that's where the redmon port monitor sends it)
	fileInput = stdin;

	// Check if we have a filename to write to:
	cPath[0] = '\0';
	bool bAutoOpen = false;
	bool bMakeTemp = false;
	// Read the start of the file; if we have a filename and/or the auto-open flag, they must be there:
	nBuffer = fread(cBuffer, 1, MAX_PATH * 2, fileInput);
	cBuffer[nBuffer] = EOF;

	// Do we have a %%File: starting the buffer?
	if ((nBuffer > 8) && (strncmp(cBuffer, "%%File: ", 8) == 0))
	{
		// Yes, so read the filename
		char ch;
		int nCount = 0;
		nInBuffer += 8;
		do
		{
			ch = cBuffer[nInBuffer++];
			if (ch == EOF)
				break;
			if (ch == '\n')
				break;
			cPath[nCount++] = ch;
		} while (true);

		if (ch == EOF)
		{
			// If we didn't find a newline, something ain't right
			return 0;
		}

		// OK, found the page, so set it as a command line variable now
		cPath[nCount] = '\0';

		// Sometimes we don't want any output:
		if (strcmp(cPath, ":dropfile:") == 0)
		{
			// Nothing doing
			CleanInput();
			return 0;
		}

		sprintf_s(cFile, sizeof(cFile), "-sOutputFile=%s", cPath);
		ARGS[5] = cFile;
	}
	// Do we have an auto-file-open flag?
	if ((nBuffer - nInBuffer > 14) && ((!strncmp(cBuffer + nInBuffer, "%%FileAutoOpen", 14)) || (!strncmp(cBuffer + nInBuffer, "%%CreateAsTemp", 14))))
	{
		// Yes, found it, so jump over it until the newline
		if (!strncmp(cBuffer + nInBuffer, "%%CreateAsTemp", 14))
			bMakeTemp = true;

		nInBuffer += 14;
		bAutoOpen = true;
		while ((cBuffer[nInBuffer] != EOF) && (cBuffer[nInBuffer] != '\n'))
			nBuffer++;
		if (cBuffer[nInBuffer] == EOF)
		{
			// Nothing else, leave
			return 0;
		}
	}

	if (cPath[0] == '\0')
	{
		// Do we make it a temp file?
		if (bMakeTemp) {
			char sTempFolder[MAX_PATH];
			GetTempPath(MAX_PATH, sTempFolder);
			sprintf_s(cPath, MAX_PATH, "%s%s%u.%s", sTempFolder, TEMP_FILENAME, GetTickCount(), TEMP_EXTENSION);

			HANDLE test = CreateFile(cPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
			if (test == INVALID_HANDLE_VALUE) {
				// If we can't write this file, for some reason:
				bMakeTemp = false;
			}
			else {
				CloseHandle(test);
				sprintf_s(cFile, sizeof(cFile), "-sOutputFile=%s", cPath);
				ARGS[5] = cFile;
			}
		}

		// It's possible that if something fails in the process of making a temp file, the bMakeTemp flag
		// will be disabled in the above block and then we want to run the following block as usual.
		if (!bMakeTemp) 
		{
			strcpy(cPath, constRandomPath);
			FILE *handle = fopen(constRandomPath, "w+b");
			sprintf_s(cFile, sizeof(cFile), "-sOutputFile=%s", cPath);
			ARGS[5] = cFile;
			fclose(handle);
		}
	}

	
	// First try to initialize a new GhostScript instance
	void* pGS;
	if (gsapi_new_instance(&pGS, NULL) < 0)
	{
		// Error 
		return -1;
	}

	// Set up the callbacks
	if (gsapi_set_stdio(pGS, my_in, my_out, my_err) < 0)
	{
		// Failed...
		gsapi_delete_instance(pGS);
		return -2;
	}

	// Now run the GhostScript engine to transform PostScript into PDF
	int nRet = gsapi_init_with_args(pGS, sizeof(ARGS)/sizeof(char*), (char**)ARGS);

	gsapi_exit(pGS);
	gsapi_delete_instance(pGS);

	// Did we get an error?
	if (strlen(cErr) > 0)
	{
		// Yes, show it
		MessageBox(NULL, cErr, PRODUCT_NAME, MB_ICONERROR|MB_OK);
		return 0;
	}

	// Should we open the file (also make sure there's a handler for PDFs)
	if (bAutoOpen && CanOpenPDFFiles()) {
		// Yes, so open it
		ShellExecute(NULL, NULL, randomPath.c_str(), NULL, NULL, SW_NORMAL);
	}

	// Send path to Photon via HTTP
	enum { max_length = MAX_PATH };

	try
	{
		boost::asio::io_service io_service;

		tcp::resolver resolver(io_service);
		tcp::resolver::query query("localhost", "8888");
		tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
		tcp::resolver::iterator end;

		tcp::socket socket(io_service);
		boost::system::error_code error = boost::asio::error::host_not_found;
		while (error && endpoint_iterator != end)
		{
			socket.close();
			socket.connect(*endpoint_iterator++, error);
		}
		if (error)
			throw boost::system::system_error(error);

		boost::asio::streambuf request;
		std::ostream request_stream(&request);

		request_stream << "POST /print HTTP/1.1\r\n";
		request_stream << "Host: " << "localhost:8888" << "\r\n";
		request_stream << "Accept: */*\r\n";
		request_stream << "Content-Length: " << randomPath.size() << "\r\n";
		request_stream << "Content-Type: application/x-www-form-urlencoded\r\n";
		request_stream << "Connection: close\r\n\r\n";
		request_stream << randomPath.c_str();

		// Send the request.
		boost::asio::write(socket, request);

		boost::asio::streambuf response;
		boost::asio::read_until(socket, response, "\r\n");
	}
	catch (std::exception& e)
	{
		if (strncmp(e.what(), "read_until", 10)) {
			MessageBox(NULL, e.what(), PRODUCT_NAME, MB_ICONERROR | MB_OK);
			return 1;
		}
	}

	return 0;
}
