#pragma once
#include <string>

#include "public.h"
#include "httpkit.h"

namespace ssoDataFrom
{
	void SSO_setcertificate(std::string path_certificate);

	typedef struct _ssoTokenInfo
	{
		std::string access_token;
		std::string token_type;
		std::string refresh_token;
		int			expires_in;
		std::string scope;
		std::string jti;

		_ssoTokenInfo()
		{
			access_token = "";
			token_type = "";
			refresh_token = "";
			expires_in = 0;
			scope = "";
			jti = "";
		}

		void to_ansi()
		{
			utf8_to_ansi(access_token.c_str(), access_token.length(), access_token);
			utf8_to_ansi(token_type.c_str(), token_type.length(), token_type);
			utf8_to_ansi(refresh_token.c_str(), refresh_token.length(), refresh_token);
			utf8_to_ansi(scope.c_str(), scope.length(), scope);
			utf8_to_ansi(jti.c_str(), jti.length(), jti);
		}
	}ssoTokenInfo;
	bool SSO_gettoken_bycode(std::string url, std::string head_redirect_uri, std::string head_code, std::string head_Authorization, ssoTokenInfo& ossTokenItem);
	bool SSO_gettoken_refresh(std::string url, std::string refresh_token, std::string head_Authorization, ssoTokenInfo& ossTokenItem);

	typedef struct _ssoUserInfo
	{
		int			id;
		int			disabled;
		int			rootflag;
		std::string userCode;
		std::string parentCode;
		std::string siteCode;
		std::string loginName;
		std::string loginPassword;
		std::string phone;
		std::string email;
		_ssoUserInfo()
		{
			id = -1;
			disabled = 1;
			rootflag = 0;
			userCode = "";
			parentCode = "";
			siteCode = "";
			loginName = "";
			loginPassword = "";
			phone = "";
			email = "";
		}

		void to_ansi()
		{
			utf8_to_ansi(userCode.c_str(), userCode.length(), userCode);
			utf8_to_ansi(parentCode.c_str(), parentCode.length(), parentCode);
			utf8_to_ansi(siteCode.c_str(), siteCode.length(), siteCode);
			utf8_to_ansi(loginName.c_str(), loginName.length(), loginName);
			utf8_to_ansi(loginPassword.c_str(), loginPassword.length(), loginPassword);
			utf8_to_ansi(phone.c_str(), phone.length(), phone);
			utf8_to_ansi(email.c_str(), email.length(), email);
		}
	}ssoUserInfo;
	bool SSO_getuser_bytoken(std::string url, std::string head_token, ssoUserInfo& ossUserItem);
};

