#include "ssoDataFrom.h"

#include "public.h"
#include "json.h"
#include <vector>

//custom loger
static FileWriter loger_ssoDataFrom("ssoDataFrom.log");

class _autoRelease
{
public:
	_autoRelease(char* pdel) :pbuffer(pdel)
	{
		pwbuffer = NULL;
	}
	_autoRelease(WCHAR* pdel) :pwbuffer(pdel)
	{
		pbuffer = NULL;
	}
	~_autoRelease()
	{
		if (pbuffer)
		{
			delete[]pbuffer;
			pbuffer = NULL;
		}
		if (pwbuffer)
		{
			delete[]pwbuffer;
			pwbuffer = NULL;
		}
	}
protected:
	char* pbuffer;
	WCHAR* pwbuffer;
};
#define  AUTODEL(name,pbuff)  _autoRelease (name)(pbuff)

namespace ssoDataFrom
{
	bool SSO_gettoken_bycode(std::string url, std::string head_redirect_uri, std::string head_code, std::string head_Authorization, ssoTokenInfo& ossTokenItem)
	{
		//URL
		url += std::string("?grant_type=authorization_code");
		url += std::string("&redirect_uri=") + head_redirect_uri;
		url += std::string("&code=") + head_code;
		std::string url_utf8; ansi_to_utf8(url.c_str(), url.length(), url_utf8);
		char* pEncodedRequest(NULL);
		httpkit::URLEncode((char*)url_utf8.c_str(), &pEncodedRequest);
		AUTODEL(url_auto, pEncodedRequest);

		//Header
		#define	headcount 2
		std::vector<std::string> headarray;
		headarray.push_back(std::string("Content-Type:application/json;charset=utf-8"));
		headarray.push_back(std::string("Authorization:") + head_Authorization);
		char* pHeader[headcount];
		for (size_t i = 0; i < headarray.size(); i++)
		{
			pHeader[i] = (char*)headarray[i].c_str();
		}

		//POST
		httpkit::DataBlock headerData;
		httpkit::DataBlock bodyData;
		long long time_S = gettimecount_now();
		httpkit::httprequest_post(pEncodedRequest, pHeader, headerData, bodyData, headcount, nullptr);
		long long time_E = gettimecount_now();

		//////////////////////////////////////////////////////////////////////////
		json::Value json_value = json::Deserialize((char*)bodyData.pBuff);
		if (bodyData.pBuff == nullptr || json_value.GetType() == json::NULLVal)
		{
			_debug_to(loger_ssoDataFrom, 1, ("SSO_gettoken_bycode: body type is null...\n"));
			return false;
		}
		else
		{
			json::Object json_obj = json_value.ToObject();
			if(json_obj.HasKey("access_token"))
				ossTokenItem.access_token = json_obj["access_token"].ToString();
			if (json_obj.HasKey("token_type"))
				ossTokenItem.token_type = json_obj["token_type"].ToString();
			if (json_obj.HasKey("refresh_token"))
				ossTokenItem.refresh_token = json_obj["refresh_token"].ToString();
			if (json_obj.HasKey("expires_in"))
				ossTokenItem.expires_in = json_obj["expires_in"].ToInt();
			if (json_obj.HasKey("scope"))
				ossTokenItem.scope = json_obj["scope"].ToString();
			if (json_obj.HasKey("jti"))
				ossTokenItem.jti = json_obj["jti"].ToString();
			
			ossTokenItem.to_ansi();
			if (ossTokenItem.access_token.empty() || ossTokenItem.token_type.empty() || ossTokenItem.refresh_token.empty())
			{
				_debug_to(loger_ssoDataFrom, 1, ("SSO_gettoken_bycode: object keyvalue exist empty...\n"));
				return false;
			}
			else
			{
				_debug_to(loger_ssoDataFrom, 0, ("SSO_gettoken_bycode: access_token=%s,token_type=%s,refresh_token=%s\n"), ossTokenItem.access_token.c_str(), ossTokenItem.token_type.c_str(), ossTokenItem.refresh_token.c_str());
			}
		}

		return true;
	}
	bool SSO_gettoken_refresh(std::string url, std::string refresh_token, std::string head_Authorization, ssoTokenInfo& ossTokenItem)
	{
		//URL
		url += std::string("?grant_type=refresh_token");
		url += std::string("&refresh_token=") + refresh_token;
		std::string url_utf8; ansi_to_utf8(url.c_str(), url.length(), url_utf8);
		char* pEncodedRequest(NULL);
		httpkit::URLEncode((char*)url_utf8.c_str(), &pEncodedRequest);
		AUTODEL(url_auto, pEncodedRequest);

		//Header
		#define	headcount 2
		std::vector<std::string> headarray;
		headarray.push_back(std::string("Content-Type:application/json;charset=utf-8"));
		headarray.push_back(std::string("Authorization:") + head_Authorization);
		char* pHeader[headcount];
		for (size_t i = 0; i < headarray.size(); i++)
		{
			pHeader[i] = (char*)headarray[i].c_str();
		}

		//POST
		httpkit::DataBlock headerData;
		httpkit::DataBlock bodyData;
		long long time_S = gettimecount_now();
		httpkit::httprequest_post(pEncodedRequest, pHeader, headerData, bodyData, headcount, nullptr);
		long long time_E = gettimecount_now();

		//////////////////////////////////////////////////////////////////////////
		json::Value json_value = json::Deserialize((char*)bodyData.pBuff);
		if (bodyData.pBuff == nullptr || json_value.GetType() == json::NULLVal)
		{
			_debug_to(loger_ssoDataFrom, 1, ("SSO_gettoken_refresh: body type is null...\n"));
			return false;
		}
		else
		{
			json::Object json_obj = json_value.ToObject();
			if (json_obj.HasKey("access_token"))
				ossTokenItem.access_token = json_obj["access_token"].ToString();
			if (json_obj.HasKey("token_type"))
				ossTokenItem.token_type = json_obj["token_type"].ToString();
			if (json_obj.HasKey("refresh_token"))
				ossTokenItem.refresh_token = json_obj["refresh_token"].ToString();
			if (json_obj.HasKey("expires_in"))
				ossTokenItem.expires_in = json_obj["expires_in"].ToInt();
			if (json_obj.HasKey("scope"))
				ossTokenItem.scope = json_obj["scope"].ToString();
			if (json_obj.HasKey("jti"))
				ossTokenItem.jti = json_obj["jti"].ToString();

			ossTokenItem.to_ansi();
			if (ossTokenItem.access_token.empty() || ossTokenItem.token_type.empty() || ossTokenItem.refresh_token.empty())
			{
				_debug_to(loger_ssoDataFrom, 1, ("SSO_gettoken_refresh object keyvalue exist empty...\n"));
				return false;
			}
			else
			{
				_debug_to(loger_ssoDataFrom, 0, ("SSO_gettoken_refresh: access_token=%s,token_type=%s,refresh_token=%s\n"), ossTokenItem.access_token.c_str(), ossTokenItem.token_type.c_str(), ossTokenItem.refresh_token.c_str());
			}
		}

		return true;
	}
	bool SSO_getuser_bytoken(std::string url, std::string head_token, ssoUserInfo& ossUserItem)
	{
		//URL
		std::string url_utf8; ansi_to_utf8(url.c_str(), url.length(), url_utf8);
		char* pEncodedRequest(NULL);
		httpkit::URLEncode((char*)url_utf8.c_str(), &pEncodedRequest);
		AUTODEL(url_auto, pEncodedRequest);

		//Header
		#define	headcount 2
		std::vector<std::string> headarray;
		headarray.push_back(std::string("Content-Type:application/json;charset=utf-8"));
		headarray.push_back(std::string("sobeycloud-token:") + head_token);
		char* pHeader[headcount];
		for (size_t i = 0; i < headarray.size(); i++)
		{
			pHeader[i] = (char*)headarray[i].c_str();
		}

		//GET
		httpkit::DataBlock headerData;
		httpkit::DataBlock bodyData;
		long long time_S = gettimecount_now();
		httpkit::httprequest_get(pEncodedRequest, pHeader, headerData, bodyData, headcount);
		long long time_E = gettimecount_now();

		//////////////////////////////////////////////////////////////////////////
		json::Value json_value = json::Deserialize((char*)bodyData.pBuff);
		if (bodyData.pBuff == nullptr || json_value.GetType() == json::NULLVal)
		{
			_debug_to(loger_ssoDataFrom, 1, ("SSO_getuser_bytoken: body type is null...\n"));
			return false;
		}
		else
		{
			json::Object json_obj = json_value.ToObject();
			if (json_obj.HasKey("id"))
				ossUserItem.id = json_obj["id"].ToInt();
			if (json_obj.HasKey("disabled"))
				ossUserItem.disabled = json_obj["disabled"].ToInt();
			if (json_obj.HasKey("rootUserFlag"))
				ossUserItem.rootflag = (json_obj["rootUserFlag"].ToBool())?(1):(0);
			if (json_obj.HasKey("userCode"))
				ossUserItem.userCode = json_obj["userCode"].ToString();
			if (json_obj.HasKey("parentUserCode"))
				ossUserItem.parentCode = json_obj["parentUserCode"].ToString();
			if (json_obj.HasKey("siteCode"))
				ossUserItem.siteCode = json_obj["siteCode"].ToString();
			if (json_obj.HasKey("loginName"))
				ossUserItem.loginName = json_obj["loginName"].ToString();
			if (json_obj.HasKey("encryptSecretKey"))
				ossUserItem.loginPassword = json_obj["encryptSecretKey"].ToString();//encryptSecretKey×Ö¶Î×÷ÎªÃÜÂë±£´æ
			if (json_obj.HasKey("phone"))
				ossUserItem.phone = json_obj["phone"].ToString();
			if (json_obj.HasKey("email"))
				ossUserItem.email = json_obj["email"].ToString();

			ossUserItem.to_ansi();
			if (ossUserItem.userCode.empty() || ossUserItem.parentCode.empty() || ossUserItem.siteCode.empty())
			{
				_debug_to(loger_ssoDataFrom, 1, ("SSO_getuser_bytoken: object keyvalue exist empty...\n"));
				return false;
			}
			else
			{
				//_debug_to(loger_ssoDataFrom, 0, ("SSO_getuser_bytoken: userinfo = %s\n"), json::Serialize(json_value).c_str());
				_debug_to(loger_ssoDataFrom, 0, ("SSO_getuser_bytoken: userCode=%s,parentCode=%s,siteCode=%s\n"), ossUserItem.userCode.c_str(), ossUserItem.parentCode.c_str(), ossUserItem.siteCode.c_str());
			}
		}

		return true;
	}
};
