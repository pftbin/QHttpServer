#pragma once
#include <curl/curl.h>

namespace httpkit
{
	struct DataBlock
	{
		DataBlock(int nBufferSize = 1024 * 1000);
		~DataBlock();

		BYTE* pBuff;
		int nBufSize;
		int nPos;
	};

	//1 base func
	void UTF8ToUnicode(char* pUTF8Src, WCHAR** ppUnicodeDst);
	void UnicodeToUTF8(const WCHAR* pUnicodeSrc, char** ppUTF8Dst);
	void URLEncode(char* szIn, char** pOut);

	//2 API func
	bool httprequest_get(char* lpRequestUTF8Encoded, char* lpHeader[], DataBlock& headerData, DataBlock& bodyData, int nHeaderCount);
	bool httprequest_post(char* lpRequestUTF8Encoded, char* lpHeader[], DataBlock& headerData, DataBlock& bodyData, int nHeaderCount, char* lpData);
	bool httprequest_patch(char* lpRequestUTF8Encoded, char* lpHeader[], DataBlock& headerData, DataBlock& bodyData, int nHeaderCount, char* lpData);

	//3 Other
	bool httprequest_download(char* lpRequestUTF8Encoded, char* lpfilefullpath);

};

