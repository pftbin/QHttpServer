#include "httpkit.h"

#pragma comment(lib, "libcurl.lib") 
#pragma comment(lib, "ssleay32.lib")
#pragma comment(lib, "libeay32.lib")

namespace httpkit
{
	DataBlock::DataBlock(int nBufferSize)
	{
		nBufSize = nBufferSize;
		nPos = 0;
		pBuff = new BYTE[nBufferSize];
		memset(pBuff, 0, nBufferSize);
	}

	DataBlock::~DataBlock()
	{
		delete[]pBuff;
		nBufSize = 0;
		nPos = 0;
	}
	//0
	static size_t write_data_to_buf(void* ptr, size_t size, size_t nmemb, void* pUserData)
	{
		DataBlock* pDataBlock = (DataBlock*)pUserData;
		if (memcpy_s(pDataBlock->pBuff + pDataBlock->nPos, pDataBlock->nBufSize - pDataBlock->nPos, ptr, size * nmemb) != 0)
		{

			//TODO: error code;
			return 0;
		}

		pDataBlock->nPos += size * nmemb;
		return size * nmemb;
	}

	static size_t write_file(void* ptr, size_t size, size_t nmemb, void* pUserData)
	{
		HANDLE hFile = *((HANDLE*)pUserData);
		DWORD dwWrittenSize(0);
		::WriteFile(hFile, ptr, size * nmemb, &dwWrittenSize, NULL);

		return dwWrittenSize;
	}

	static size_t write_data(void* ptr, size_t size, size_t nmemb, void* stream)
	{
		size_t written = fwrite(ptr, size, nmemb, (FILE*)stream);
		return written;
	}

	//1 base func
	void UTF8ToUnicode(char* pUTF8Src, WCHAR** ppUnicodeDst)
	{

		int nUnicodeLen;        //转换后Unicode的长度

		//获得转换后的长度，并分配内存
		nUnicodeLen = MultiByteToWideChar(CP_UTF8,
			0,
			pUTF8Src,
			-1,
			NULL,
			0);

		nUnicodeLen += 1;
		*ppUnicodeDst = new WCHAR[nUnicodeLen];

		//转为Unicode
		MultiByteToWideChar(CP_UTF8,
			0,
			pUTF8Src,
			-1,
			*ppUnicodeDst,
			nUnicodeLen);
	}

	void UnicodeToUTF8(const WCHAR* pUnicodeSrc, char** ppUTF8Dst)
	{
		/* get output buffer length */
		int		iUTF8Len(0);
		// wide char to multi char
		iUTF8Len = WideCharToMultiByte(CP_UTF8,
			0,
			pUnicodeSrc,
			-1,
			NULL,
			0,
			NULL,
			NULL);

		/* convert unicode to UTF8 */
		iUTF8Len += 1;
		*ppUTF8Dst = new char[iUTF8Len];
		memset(*ppUTF8Dst, 0, iUTF8Len);

		iUTF8Len = WideCharToMultiByte(CP_UTF8,
			0,
			pUnicodeSrc,
			-1,
			*ppUTF8Dst,
			iUTF8Len,
			NULL,
			NULL);

	}

#define TOHEX(x) ((x)>9 ? (x)+55 : (x)+48)
	void URLEncode(char* szIn, char** pOut)
	{
		int nInLenth = strlen(szIn);
		int nFlag = 0;
		BYTE byte;
		*pOut = new char[nInLenth * 3 + 1];
		char* szOut = *pOut;
		for (int i = 0; i < nInLenth; i++)
		{
			byte = szIn[i];
			if (isalnum(byte) ||
				(byte == '-') ||
				(byte == '_') ||
				(byte == '.') ||
				(byte == '~') ||
				(byte == '/') ||
				(byte == '?') ||
				(byte == ':') ||
				(byte == '=') ||
				(byte == '|') ||
				(byte == '&'))
			{
				szOut[nFlag++] = byte;
			}
			else
			{
				if (isspace(byte))
				{
					szOut[nFlag++] = '+';
				}
				else if (byte == '%')
				{
					szOut[nFlag++] = byte;

					i++; byte = szIn[i];
					szOut[nFlag++] = byte;

					i++; byte = szIn[i];
					szOut[nFlag++] = byte;
				}
				else
				{
					szOut[nFlag++] = '%';
					szOut[nFlag++] = TOHEX(byte >> 4);
					szOut[nFlag++] = TOHEX(byte % 16);
				}
			}
		}
		szOut[nFlag] = '\0';
	}


	//2 API func
	bool httprequest_get(char* lpRequestUTF8Encoded, char* lpHeader[], DataBlock& headerData, DataBlock& bodyData, int nHeaderCount)
	{

		CURL* curl_handle;
		curl_global_init(CURL_GLOBAL_ALL);

		/* init the curl session */
		curl_handle = curl_easy_init();

		curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10);

		/* set URL to get */
		curl_easy_setopt(curl_handle, CURLOPT_URL, lpRequestUTF8Encoded);

		curl_slist* plist = curl_slist_append(NULL, lpHeader[0]);
		for (int i = 1; i < nHeaderCount; i++)
		{
			curl_slist_append(plist, lpHeader[i]);
		}

		curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, plist);

		/* no progress meter please */
		curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);

		/* send all data to this function  */
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data_to_buf);


		/* we want the headers be written to this file handle */
		curl_easy_setopt(curl_handle, CURLOPT_WRITEHEADER, &headerData);

		/* we want the body be written to this file handle instead of stdout */
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &bodyData);
		try
		{
			// 不验证证书
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, false);
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);

			/* get it! */
			CURLcode res = curl_easy_perform(curl_handle);

			curl_slist_free_all(plist);

			/* cleanup curl stuff */
			curl_easy_cleanup(curl_handle);

		}
		catch (...)
		{
			curl_slist_free_all(plist);

			/* cleanup curl stuff */
			curl_easy_cleanup(curl_handle);
		}

		return true;
	}

	bool httprequest_post(char* lpRequestUTF8Encoded, char* lpHeader[], DataBlock& headerData, DataBlock& bodyData, int nHeaderCount, char* lpData)
	{
		CURL* curl_handle;
		curl_global_init(CURL_GLOBAL_ALL);

		/* init the curl session */
		curl_handle = curl_easy_init();

		curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT,10);   
		curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, "POST");

		if (lpData)
		{
			long len = (long)strlen(lpData);
			curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, lpData);
			curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, len);
		}

		/* set URL to get */
		curl_easy_setopt(curl_handle, CURLOPT_URL, lpRequestUTF8Encoded);

		curl_slist* plist = curl_slist_append(NULL, lpHeader[0]);
		for (int i = 1; i < nHeaderCount; i++)
		{
			curl_slist_append(plist, lpHeader[i]);
		}

		curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, plist);

		/* no progress meter please */
		curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);

		/* send all data to this function  */
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data_to_buf);


		///* we want the headers be written to this file handle */
		curl_easy_setopt(curl_handle, CURLOPT_WRITEHEADER, &headerData);

		///* we want the body be written to this file handle instead of stdout */
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &bodyData);

		bool bRet = false;
		// 不验证证书
		curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, false);
		curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);

		/* get it! */
		CURLcode res = curl_easy_perform(curl_handle);

		//get HTTP error   
		long HTTP_flag = 0;
		curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &HTTP_flag);
		if (HTTP_flag == 307)//error
			bRet = false;
		
		if (res == CURLE_OK)
			bRet = true;

		curl_slist_free_all(plist);

		/* cleanup curl stuff */
		curl_easy_cleanup(curl_handle);

		return bRet;
	}

	bool httprequest_patch(char* lpRequestUTF8Encoded, char* lpHeader[], DataBlock& headerData, DataBlock& bodyData, int nHeaderCount, char* lpData)
	{
		CURL* curl_handle;
		curl_global_init(CURL_GLOBAL_ALL);

		/* init the curl session */
		curl_handle = curl_easy_init();

		curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10);
		curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, "PATCH");

		if (lpData)
		{
			long len = (long)strlen(lpData);
			curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, lpData);
			curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, len);
		}

		/* set URL to get */
		curl_easy_setopt(curl_handle, CURLOPT_URL, lpRequestUTF8Encoded);

		curl_slist* plist = curl_slist_append(NULL, lpHeader[0]);
		for (int i = 1; i < nHeaderCount; i++)
		{
			curl_slist_append(plist, lpHeader[i]);
		}

		curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, plist);

		/* no progress meter please */
		curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);

		/* send all data to this function  */
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data_to_buf);


		///* we want the headers be written to this file handle */
		curl_easy_setopt(curl_handle, CURLOPT_WRITEHEADER, &headerData);

		///* we want the body be written to this file handle instead of stdout */
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &bodyData);

		bool bRet = false;
		// 不验证证书
		curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, false);
		curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);

		/* get it! */
		CURLcode res = curl_easy_perform(curl_handle);

		//get HTTP error   
		long HTTP_flag = 0;
		curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &HTTP_flag);
		if (HTTP_flag == 307)//error
			bRet = false;

		if (res == CURLE_OK)
			bRet = true;

		curl_slist_free_all(plist);

		/* cleanup curl stuff */
		curl_easy_cleanup(curl_handle);

		return bRet;
	}

	//3 Other
	bool httprequest_download(char* lpRequestUTF8Encoded, char* lpfilefullpath)
	{
		CURL* curl_handle;
		curl_global_init(CURL_GLOBAL_ALL);

		/* init the curl session */
		curl_handle = curl_easy_init();

		/* set URL to get here */
		curl_easy_setopt(curl_handle, CURLOPT_URL, lpRequestUTF8Encoded);

		/* Switch on full protocol/debug output while testing */
		curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);

		/* disable progress meter, set to 0L to enable and disable debug output */
		curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);

		/* send all data to this function  */
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);

		/* open the file */
		FILE*  pfile = fopen(lpfilefullpath, "wb");
		if (pfile) {

			/* write the page body to this file handle */
			curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pfile);

			// 不验证证书
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, false);
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);

			/* get it! */
			CURLcode res = curl_easy_perform(curl_handle);

			/* close the header file */
			fclose(pfile);
		}

		/* cleanup curl stuff */
		curl_easy_cleanup(curl_handle);

		return true;
	}

};

