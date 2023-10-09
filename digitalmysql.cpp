#include "digitalmysql.h"

#pragma comment(lib,"libmysql.lib")
#pragma warning(error:4477)  //<snprintf> warning as error


static bool simulation = false;
std::string g_database_ip = "";
short	    g_database_port = 3306;
std::string g_database_username = "";
std::string g_database_password = "";
std::string g_database_dbname = "";

#define BUFF_SZ				1024*16			//system max stack size

//custom loger
static FileWriter loger_mysql("mysql.log");

namespace digitalmysql
{
	std::string row_value(char* pChar)
	{
		std::string result_utf8 = "";
		if (pChar != nullptr)
			result_utf8 = pChar;

		std::string result_ansi = "";
		utf8_to_ansi(result_utf8.c_str(), result_utf8.length(), result_ansi);
		return result_ansi;
	}
	bool getconfig_mysql(std::string configfilepath, std::string& error)
	{
		long length = 0;
		char* configbuffer = nullptr;
		FILE* fp = nullptr;
		fp = fopen(configfilepath.c_str(), "r");
		if (fp != nullptr)
		{
			fseek(fp, 0, SEEK_END);
			length = ftell(fp);
			rewind(fp);

			configbuffer = (char*)malloc(length * sizeof(char));
			if (configbuffer == nullptr) return false;

			fread(configbuffer, length, 1, fp);
			fclose(fp);
		}

		if (length && configbuffer)
		{
			std::string value = "";
			std::string config = configbuffer;
			free(configbuffer);

			value = getnodevalue(config, "mysql_ip"); CHECK_CONFIG("mysql_ip", value, error);
			g_database_ip = value;
			_debug_to(1, ("CONFIG database_ip = %s\n"), g_database_ip.c_str());

			value = getnodevalue(config, "mysql_port"); CHECK_CONFIG("mysql_port", value, error);
			g_database_port = atoi(value.c_str());
			_debug_to(1, ("CONFIG database_port = %d\n"), g_database_port);

			value = getnodevalue(config, "mysql_username"); CHECK_CONFIG("mysql_username", value, error);
			g_database_username = value;
			_debug_to(1, ("CONFIG database_username = %s\n"), g_database_username.c_str());

			value = getnodevalue(config, "mysql_password"); CHECK_CONFIG("mysql_password", value, error);
			g_database_password = value;
			_debug_to(1, ("CONFIG database_password = %s\n"), g_database_password.c_str());

			value = getnodevalue(config, "mysql_dbname"); CHECK_CONFIG("mysql_dbname", value, error);
			g_database_dbname = value;
			_debug_to(1, ("CONFIG database_dbname = %s\n"), g_database_dbname.c_str());

			return true;
		}

		return false;
	}
	std::string transsequencename(std::string strname)
	{
		std::string strvalue = str_replace(strname, std::string("sbt_"), std::string("sbt_sqt_"));
		return strvalue;
	}
	int newgetsequencenextvalue(std::string strsequencename, MYSQL* pmysql)
	{
		strsequencename = transsequencename(strsequencename);
		if (strsequencename.empty()) return 0;

		bool ret = true;int sequence = 0;

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select id from %s order by id desc", strsequencename.c_str());
		_debug_to(loger_mysql, 0, ("[newgetsequencenextvalue] sql: %s\n"), sql_buff);
		if (!mysql_query(pmysql, sql_buff))	//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(pmysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[newgetsequencenextvalue] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 1)//keep right
			{
				sequence = atoi(row[0]);
			}
			else
			{
				sequence = 0;
			}
			sequence++;

			//
			snprintf(sql_buff, BUFF_SZ, "insert into %s (id) value(%d)", strsequencename.c_str(), sequence);// insert
			_debug_to(loger_mysql, 0, ("[newgetsequencenextvalue] sql: %s\n"), sql_buff);
			if (!mysql_query(pmysql, sql_buff))	//success return 0,failed return random number
			{
				_debug_to(loger_mysql, 0, ("[newgetsequencenextvalue], insert sqt table success, sequence=%d\n"), sequence);
			}
			else
			{
				ret = false;
				std::string error = mysql_error(pmysql);
				_debug_to(loger_mysql, 1, ("[newgetsequencenextvalue], insert sqt table failed: %s\n"), error.c_str());
			}
			mysql_free_result(result);				//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(pmysql);
			_debug_to(loger_mysql, 1, ("[newgetsequencenextvalue], select sqt table failed: %s\n"), error.c_str());
		}

		if (!ret) return 0;
		return sequence;
	}

	//用户
	bool adduserinfo(userinfo useritem, bool update)
	{
		if (simulation) return true;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		useritem.to_utf8();

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[adduserinfo]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		std::string OPERATION = "";
		char sql_buff[BUFF_SZ] = { 0 };
		if (update)
		{
			OPERATION = "UPDATE";
			//update
			snprintf(sql_buff, BUFF_SZ, "update sbt_userinfo set disabled=%d,usertype=%d,servicetype=%d,rootflag=%d,adminflag=%d,usercode='%s',parentcode='%s',sitecode='%s',loginname='%s',loginpassword='%s',phone='%s',email='%s',projectname='%s',remaintime=%lld,deadlinetime='%s' where id=%d",
				useritem.disabled, useritem.usertype, useritem.servicetype, useritem.rootflag, useritem.adminflag, useritem.userCode.c_str(), useritem.parentCode.c_str(), useritem.siteCode.c_str(), useritem.loginName.c_str(), useritem.loginPassword.c_str(), useritem.phone.c_str(), useritem.email.c_str(), useritem.projectName.c_str(), useritem.remainTime, useritem.deadlineTime.c_str(),
				useritem.id);//update
		}
		else
		{
			OPERATION = "INSERT";
			//insert 
			snprintf(sql_buff, BUFF_SZ, "insert into sbt_userinfo (id,disabled,usertype,servicetype,rootflag,adminflag,usercode,parentcode,sitecode,loginname,loginpassword,phone,email,projectname,remaintime,deadlinetime) values(%d, %d, %d, %d, %d, %d, '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%lld', '%s')",
				useritem.id,
				useritem.disabled, useritem.usertype, useritem.servicetype, useritem.rootflag, useritem.adminflag, useritem.userCode.c_str(), useritem.parentCode.c_str(), useritem.siteCode.c_str(), useritem.loginName.c_str(), useritem.loginPassword.c_str(), useritem.phone.c_str(), useritem.email.c_str(), useritem.projectName.c_str(), useritem.remainTime, useritem.deadlineTime.c_str());
		}

		//run sql
		_debug_to(loger_mysql, 0, ("[adduserinfo] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[adduserinfo]id=%d, %s success\n"), useritem.id, OPERATION.c_str());
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[adduserinfo]id=%d, %s failed: %s\n"), useritem.id, OPERATION.c_str(), error.c_str());
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool getuserinfo(int userid, userinfo& useritem)
	{
		if (simulation) return true;
		if (userid <= 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getuserinfo]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select id,disabled,usertype,servicetype,rootflag,adminflag,usercode,parentcode,sitecode,loginname,loginpassword,phone,email,projectname,remaintime,deadlinetime from sbt_userinfo where id=%d", userid);//select	
		_debug_to(loger_mysql, 0, ("[getuserinfo] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[getuserinfo] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 16)//keep right
			{
				int i = 0;
				int row_id = atoi(row_value(row[i++]).c_str());
				int row_disabled = atoi(row_value(row[i++]).c_str());
				int row_usertype = atoi(row_value(row[i++]).c_str());
				int row_servicetype = atoi(row_value(row[i++]).c_str());
				int row_rootflag = atoi(row_value(row[i++]).c_str());
				int row_adminflag = atoi(row_value(row[i++]).c_str());
				std::string row_usercode = row_value(row[i++]);			
				std::string row_parentcode = row_value(row[i++]);       
				std::string row_sitecode = row_value(row[i++]);			
				std::string row_loginname = row_value(row[i++]);		
				std::string row_loginpassword = row_value(row[i++]);	
				std::string row_phone = row_value(row[i++]);	
				std::string row_email = row_value(row[i++]);
				std::string row_projectname = row_value(row[i++]);
				long long   row_remaintime = atoll(row_value(row[i++]).c_str());
				std::string row_deadlinetime = row_value(row[i++]);

				useritem.id = row_id;
				useritem.disabled = row_disabled;
				useritem.servicetype = row_servicetype;
				useritem.usertype = row_usertype;
				useritem.rootflag = row_rootflag;
				useritem.adminflag = row_adminflag;
				useritem.userCode = row_usercode;
				useritem.parentCode = row_parentcode;
				useritem.siteCode = row_sitecode;
				useritem.loginName = row_loginname;
				useritem.loginPassword = row_loginpassword;
				useritem.phone = row_phone;
				useritem.email = row_email;
				useritem.projectName = row_projectname;
				useritem.remainTime = row_remaintime;
				useritem.deadlineTime = row_deadlinetime;
			}
			else
			{
				ret = false;
				_debug_to(loger_mysql, 1, ("[getuserinfo] select userinfo count/colnum error\n"));
			}
			mysql_free_result(result);				//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getuserinfo]mysql_query userinfo failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool getuserinfo(std::string name, std::string password, userinfo& useritem)
	{
		if (simulation) return true;
		if (name.empty() || password.empty()) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(password.c_str(), password.length(), password);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getuserinfo]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select id,disabled,usertype,servicetype,rootflag,adminflag,usercode,parentcode,sitecode,loginname,loginpassword,phone,email,projectname,remaintime,deadlinetime from sbt_userinfo where loginname='%s' and loginpassword='%s'", name.c_str(), password.c_str());//select	
		_debug_to(loger_mysql, 0, ("[getuserinfo] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[getuserinfo] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 16)//keep right
			{
				int i = 0;
				int row_id = atoi(row_value(row[i++]).c_str());
				int row_disabled = atoi(row_value(row[i++]).c_str());
				int row_usertype = atoi(row_value(row[i++]).c_str());
				int row_servicetype = atoi(row_value(row[i++]).c_str());
				int row_rootflag = atoi(row_value(row[i++]).c_str());
				int row_adminflag = atoi(row_value(row[i++]).c_str());
				std::string row_usercode = row_value(row[i++]);
				std::string row_parentcode = row_value(row[i++]);
				std::string row_sitecode = row_value(row[i++]);
				std::string row_loginname = row_value(row[i++]);
				std::string row_loginpassword = row_value(row[i++]);
				std::string row_phone = row_value(row[i++]);
				std::string row_email = row_value(row[i++]);
				std::string row_projectname = row_value(row[i++]);
				long long   row_remaintime = atoll(row_value(row[i++]).c_str());
				std::string row_deadlinetime = row_value(row[i++]);

				useritem.id = row_id;
				useritem.disabled = row_disabled;
				useritem.servicetype = row_servicetype;
				useritem.usertype = row_usertype;
				useritem.rootflag = row_rootflag;
				useritem.adminflag = row_adminflag;
				useritem.userCode = row_usercode;
				useritem.parentCode = row_parentcode;
				useritem.siteCode = row_sitecode;
				useritem.loginName = row_loginname;
				useritem.loginPassword = row_loginpassword;
				useritem.phone = row_phone;
				useritem.email = row_email;
				useritem.projectName = row_projectname;
				useritem.remainTime = row_remaintime;
				useritem.deadlineTime = row_deadlinetime;
			}
			else
			{
				ret = false;
				_debug_to(loger_mysql, 1, ("[getuserinfo] select userinfo count/colnum error\n"));
			}
			mysql_free_result(result);				//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getuserinfo]mysql_query userinfo failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool getusername_id(int userid, std::string& username)
	{
		if (simulation) return true;
		if (userid <= 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getusername_id]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select loginname from sbt_userinfo where id = %d", userid);//select	
		_debug_to(loger_mysql, 0, ("[getusername_id] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[getusername_id] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 1)//keep right
			{
				int i = 0;
				username = row_value(row[i++]).c_str();
			}
			else
			{
				ret = false;
				_debug_to(loger_mysql, 1, ("[getusername_id] select userinfo count/colnum error\n"));
			}
			mysql_free_result(result);				//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getusername_id]mysql_query userinfo failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool getusercode_id(int userid, std::string& usercode)
	{
		if (simulation) return true;
		if (userid <= 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getusercode_id]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select usercode from sbt_userinfo where id = %d", userid);//select	
		_debug_to(loger_mysql, 0, ("[getusercode_id] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[getusercode_id] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 1)//keep right
			{
				int i = 0;
				usercode = row_value(row[i++]).c_str();
			}
			else
			{
				ret = false;
				_debug_to(loger_mysql, 1, ("[getusercode_id] select userinfo count/colnum error\n"));
			}
			mysql_free_result(result);				//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getusercode_id]mysql_query userinfo failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool getuserid_parent(int childid, int& parentid)
	{
		if (simulation) return true;
		if (childid < 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getuserid_parent]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select id from sbt_userinfo where usercode in (select parentcode from sbt_userinfo where id = %d)", childid);//select	
		_debug_to(loger_mysql, 0, ("[getuserid_parent] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[getuserid_parent] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 1)//keep right
			{
				int i = 0;
				parentid = atoi(row_value(row[i++]).c_str());
			}
			else
			{
				ret = false;
				_debug_to(loger_mysql, 1, ("[getuserid_parent] select userinfo count/colnum error\n"));
			}
			mysql_free_result(result);				//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getuserid_parent]mysql_query userinfo failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool getuserid_likename(std::string username, int& userid)
	{
		if (simulation) return true;
		if (username.empty()) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getuserid_likename]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select id from sbt_userinfo where loginname like '%%%s%%'", username.c_str());//select	
		_debug_to(loger_mysql, 0, ("[getuserid_likename] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[getuserid_likename] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 1)//keep right
			{
				int i = 0;
				userid = atoi(row_value(row[i++]).c_str());
			}
			else
			{
				ret = false;
				_debug_to(loger_mysql, 1, ("[getuserid_likename] select userinfo count/colnum error\n"));
			}
			mysql_free_result(result);				//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getuserid_likename]mysql_query userinfo failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool getuserid_childs(int parentid, std::vector<int>& vecchildid)
	{
		if (simulation) return true;
		if (parentid < 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getuserid_childs]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select id from sbt_userinfo where parentcode in (select usercode from sbt_userinfo where id = %d)", parentid);//select	
		_debug_to(loger_mysql, 0, ("[getuserid_childs] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[getuserid_childs] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row;							//table row data
			while (row = mysql_fetch_row(result))
			{
				if (row && colnum >= 1) //keep right
				{
					int i = 0;
					int childid = atoi(row_value(row[i++]).c_str());
					vecchildid.push_back(childid);
				}
				else
				{
					ret = false;
					_debug_to(loger_mysql, 1, ("[getuserid_childs] select userinfo count/colnum error\n"));
				}
			}
			mysql_free_result(result);				//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getuserid_childs]mysql_query userinfo failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool isexistuser_id(int userid)
	{
		if (simulation) return true;
		if (userid <= 0) return false;

		bool ret = false;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[isexistuser_id]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select * from sbt_userinfo where id = %d", userid);
		_debug_to(loger_mysql, 0, ("[isexistuser_id] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[isexistuser_id] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1)//keep right
				ret = true;

			mysql_free_result(result);				//free result
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool isexistuser_usercode(std::string usercode)
	{
		if (simulation) return true;
		if (usercode.empty()) return false;

		bool ret = false;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(usercode.c_str(), usercode.length(), usercode);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[isexistuser_id]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select * from sbt_userinfo where usercode = '%s'", usercode.c_str());
		_debug_to(loger_mysql, 0, ("[isexistuser_id] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[isexistuser_id] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1)//keep right
				ret = true;

			mysql_free_result(result);				//free result
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool isrootuser_id(int userid)
	{
		if (simulation) return true;
		if (userid <= 0) return false;

		bool ret = false;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[isrootuser_id]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select * from sbt_userinfo where id = %d and rootflag = 1", userid);
		_debug_to(loger_mysql, 0, ("[isrootuser_id] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[isrootuser_id] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1)//keep right
				ret = true;

			mysql_free_result(result);				//free result
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool isvaliduser_id(int userid)
	{
		if (simulation) return true;
		if (userid <= 0) return false;

		bool ret = false;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[isvaliduser_id]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select * from sbt_userinfo where id = %d and disabled = 0", userid);
		_debug_to(loger_mysql, 0, ("[isvaliduser_id] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[isvaliduser_id] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1)//keep right
				ret = true;

			mysql_free_result(result);				//free result
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool isvaliduser_namepwd(std::string name, std::string password)
	{
		if (simulation) return true;
		if (name.empty() || password.empty()) return false;

		bool ret = false;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(name.c_str(), name.length(), name);
		ansi_to_utf8(password.c_str(), password.length(), password);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[isvaliduser_namepwd]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select * from sbt_userinfo where loginname='%s' and loginpassword='%s' and disabled = 0", name.c_str(), password.c_str());
		_debug_to(loger_mysql, 0, ("[isvaliduser_namepwd] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[isvaliduser_namepwd] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1)//keep right
				ret = true;

			mysql_free_result(result);				//free result
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool islocaluser_namepwd(std::string name, std::string password)
	{
		if (simulation) return true;
		if (name.empty() || password.empty()) return false;

		bool ret = false;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(name.c_str(), name.length(), name);
		ansi_to_utf8(password.c_str(), password.length(), password);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[isvaliduser_namepwd]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select * from sbt_userinfo where loginname='%s' and loginpassword='%s' and usertype = 0", name.c_str(), password.c_str());
		_debug_to(loger_mysql, 0, ("[isvaliduser_namepwd] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[isvaliduser_namepwd] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1)//keep right
				ret = true;

			mysql_free_result(result);				//free result
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool getuserinfo_adminflag(std::string usercode, int& adminflag)
	{
		if (simulation) return true;
		if (usercode.empty()) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(usercode.c_str(), usercode.length(), usercode);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getuserinfo_adminflag]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select adminflag from sbt_userinfo where usercode='%s'", usercode.c_str());//select	
		_debug_to(loger_mysql, 0, ("[getuserinfo_adminflag] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[getuserinfo_adminflag] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 1)//keep right
			{
				int i = 0;
				int row_adminflag = atoi(row_value(row[i++]).c_str());
				adminflag = max(0, row_adminflag);
			}
			else
			{
				ret = false;
				_debug_to(loger_mysql, 1, ("[getuserinfo_adminflag] select userinfo_adminflag count/colnum error\n"));
			}
			mysql_free_result(result);				//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getuserinfo_adminflag]mysql_query userinfo_adminflag failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool setuserinfo_disable(std::string usercode, int disabled)
	{
		if (simulation) return true;
		if (usercode.empty()) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(usercode.c_str(), usercode.length(), usercode);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setuserinfo_disable]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		//
		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "update sbt_userinfo set disabled=%d where usercode='%s'", disabled, usercode.c_str());//update
		_debug_to(loger_mysql, 0, ("[setuserinfo_disable] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[setuserinfo_disable]usercode = %s, update userinfo_disable=%d success\n"), usercode.c_str(), disabled);
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setuserinfo_disable]usercode = %s, update userinfo_disable=%d failed: %s\n"), usercode.c_str(), disabled, error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool setuserinfo_deadlinetime(std::string usercode, std::string deadlinetime)
	{
		if (simulation) return true;
		if (usercode.empty()) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(usercode.c_str(), usercode.length(), usercode);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setuserinfo_deadlinetime]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		//
		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "update sbt_userinfo set deadlinetime='%s' where usercode='%s'", deadlinetime.c_str(), usercode.c_str());//update
		_debug_to(loger_mysql, 0, ("[setuserinfo_deadlinetime] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[setuserinfo_deadlinetime]usercode = %s, update userinfo_deadlinetime=%s success\n"), usercode.c_str(), deadlinetime.c_str());
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setuserinfo_deadlinetime]usercode = %s, update userinfo_deadlinetime=%s failed: %s\n"), usercode.c_str(), deadlinetime.c_str(), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool getuserinfo_remaintime(std::string usercode, long long& remaintime)
	{
		if (simulation) return true;
		if (usercode.empty()) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(usercode.c_str(), usercode.length(), usercode);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getuserinfo_remaintime]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select remaintime from sbt_userinfo where usercode='%s'", usercode.c_str());//select	
		_debug_to(loger_mysql, 0, ("[getuserinfo_remaintime] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[getuserinfo_remaintime] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 1)//keep right
			{
				int i = 0;
				int row_remain = atoi(row_value(row[i++]).c_str());
				remaintime = max(0, row_remain);
			}
			else
			{
				ret = false;
				_debug_to(loger_mysql, 1, ("[getuserinfo_remaintime] select userinfo_remaintime count/colnum error\n"));
			}
			mysql_free_result(result);				//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getuserinfo_remaintime]mysql_query userinfo_remaintime failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool setuserinfo_remaintime(std::string usercode, long long remaintime)
	{
		if (simulation) return true;
		if (usercode.empty()) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(usercode.c_str(), usercode.length(), usercode);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setuserinfo_remaintime]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		//
		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "update sbt_userinfo set remaintime=%lld where usercode='%s'", remaintime, usercode.c_str());//update
		_debug_to(loger_mysql, 0, ("[setuserinfo_remaintime] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[setuserinfo_remaintime]usercode = %s, update userinfo_remaintime success\n"), usercode.c_str());
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setuserinfo_remaintime]usercode = %s, update userinfo_remaintime failed: %s\n"), usercode.c_str(), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}

	bool getuserid_allroot(std::vector<int>& vecbelongids)
	{
		if (simulation) return true;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getuserid_allroot]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select id from sbt_userinfo where rootflag = 1");//select	
		_debug_to(loger_mysql, 0, ("[getuserid_allroot] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{

			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[getindentlistinfo] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row;							//table row data
			while (row = mysql_fetch_row(result))
			{
				if (row && colnum >= 1) //keep right
				{
					int i = 0;
					int row_id = atoi(row_value(row[i++]).c_str());
					
					vecbelongids.push_back(row_id);
				}
			}
			mysql_free_result(result);				//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getuserid_allroot]mysql_query root_users failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool getuserlistinfo(VEC_FILTERINFO& vecfilterinfo, int pagesize, int pagenum, int& total, VEC_USERINFO& vecuserinfo)
	{
		if (simulation) return true;

		//where
		std::string str_where = " where 1 "; //为了统一使用where 1,故加条件关系只能为and
		int usedfilter_count = 0;
		for (VEC_FILTERINFO::iterator it = vecfilterinfo.begin(); it != vecfilterinfo.end(); ++it)
		{
			int filtertype = it->filtertype;
			std::string filterfield = it->filterfield;
			std::string filtervalue = it->filtervalue;
			if (!filterfield.empty() && !filtervalue.empty())
			{
				filterinfo usedfilter;
				usedfilter.filtertype = filtertype;
				usedfilter.filterfield = filterfield;
				usedfilter.filtervalue = filtervalue;
				usedfilter.to_utf8();

				std::string temp;
				char tempbuff[256] = { 0 };
				if (usedfilter.filtertype == 0)//单个值
				{
					snprintf(tempbuff, 256, " and cast(%s as char) like '%%%s%%' ", usedfilter.filterfield.c_str(), usedfilter.filtervalue.c_str());//CAST统一转换字段类型为字符
					temp = tempbuff;
				}
				if (usedfilter.filtertype == 1)//范围值
				{
					snprintf(tempbuff, 256, " and %s between %s ", usedfilter.filterfield.c_str(), usedfilter.filtervalue.c_str());//value值中有 and 关键字
					temp = tempbuff;
				}
				if (usedfilter.filtertype == 2)//枚举值
				{
					snprintf(tempbuff, 256, " and %s in (%s) ", usedfilter.filterfield.c_str(), usedfilter.filtervalue.c_str());
					temp = tempbuff;
				}
				if (usedfilter.filtertype == 3)//完整条件
				{
					snprintf(tempbuff, 256, " and (%s) ", usedfilter.filtervalue.c_str());
					temp = tempbuff;
				}

				str_where += temp;
				usedfilter_count += 1;
			}
		}
		_debug_to(loger_mysql, 0, ("[getuserlistinfo]vecfilterinfo size = %d\n"), usedfilter_count);

		//limit
		std::string str_limit = "";
		if (pagesize > 0 && pagenum > 0)
		{
			int throw_count = (pagenum - 1) * pagesize;
			int need_count = pagesize;

			char temp[256] = { 0 };
			snprintf(temp, 256, " limit %d,%d ", throw_count, need_count);
			str_limit = temp;
		}

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getuserlistinfo]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		//a
		std::string str_sqltotal = "select count(id) from sbt_userinfo";
		str_sqltotal += str_where;
		_debug_to(loger_mysql, 0, ("[getuserlistinfo] sql: %s\n"), str_sqltotal.c_str());
		mysql_query(&mysql, "SET NAMES UTF8");			//support chinese text
		if (!mysql_query(&mysql, str_sqltotal.c_str()))	//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[getuserlistinfo] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 1)//keep right
			{
				int row_total = atoi(row_value(row[0]).c_str());
				total = (row_total > 0) ? (row_total) : (0);
			}
			mysql_free_result(result);				//free result
			_debug_to(loger_mysql, 0, ("[getuserlistinfo]select total success, total=%d\n"), total);
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getuserlistinfo]mysql_query usertotal failed: %s\n"), error.c_str());
		}

		//b
		std::string str_sqlselect = "select id,disabled,usertype,servicetype,rootflag,adminflag,usercode,parentcode,sitecode,loginname,loginpassword,phone,email,projectname,remaintime,deadlinetime from sbt_userinfo";
		str_sqlselect += str_where;
		str_sqlselect += str_limit;
		_debug_to(loger_mysql, 0, ("[getuserlistinfo] sql: %s\n"), str_sqlselect.c_str());
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, str_sqlselect.c_str()))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[getuserlistinfo] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row;							//table row data
			while (row = mysql_fetch_row(result))
			{
				if (row && colnum >= 16) //keep right
				{
					int i = 0;
					int row_id = atoi(row_value(row[i++]).c_str());
					int row_disabled = atoi(row_value(row[i++]).c_str());
					int row_usertype = atoi(row_value(row[i++]).c_str());
					int row_servicetype = atoi(row_value(row[i++]).c_str());
					int row_rootflag = atoi(row_value(row[i++]).c_str());
					int row_adminflag = atoi(row_value(row[i++]).c_str());
					std::string row_usercode = row_value(row[i++]);
					std::string row_parentcode = row_value(row[i++]);
					std::string row_sitecode = row_value(row[i++]);
					std::string row_loginname = row_value(row[i++]);
					std::string row_loginpassword = row_value(row[i++]);
					std::string row_phone = row_value(row[i++]);
					std::string row_email = row_value(row[i++]);
					std::string row_projectname = row_value(row[i++]);
					long long   row_remaintime = atoll(row_value(row[i++]).c_str());
					std::string row_deadlinetime = row_value(row[i++]);

					userinfo useritem;
					useritem.id = row_id;
					useritem.disabled = row_disabled;
					useritem.servicetype = row_servicetype;
					useritem.usertype = row_usertype;
					useritem.rootflag = row_rootflag;
					useritem.adminflag = row_adminflag;
					useritem.userCode = row_usercode;
					useritem.parentCode = row_parentcode;
					useritem.siteCode = row_sitecode;
					useritem.loginName = row_loginname;
					useritem.loginPassword = row_loginpassword;
					useritem.phone = row_phone;
					useritem.email = row_email;
					useritem.projectName = row_projectname;
					useritem.remainTime = row_remaintime;
					useritem.deadlineTime = row_deadlinetime;
					vecuserinfo.push_back(useritem);
				}
			}
			mysql_free_result(result);				//free result

			_debug_to(loger_mysql, 0, ("[getuserlistinfo]select humanlist success\n"));
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getuserlistinfo]mysql_query humanlist failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool deleteuserinfo_id(int userid, std::string& errmsg)
	{
		if (simulation) return true;
		if (userid <= 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[deleteuserinfo_id]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "delete from sbt_userinfo where id = %d", userid);
		_debug_to(loger_mysql, 0, ("[deleteuserinfo_id] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[deletetask_taskid]delete user by userid success, id=%d\n"), userid);
			errmsg = "delete user by userid success";
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql); errmsg = error;
			_debug_to(loger_mysql, 1, ("[deletetask_taskid]delete user by userid failed, id=%d\n"), userid);
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}

	//订单
	bool addindentinfo(indentinfo indentitem, bool update)
	{
		if (simulation) return true;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		indentitem.to_utf8();

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[addindentinfo]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		std::string OPERATION = "";
		char sql_buff[BUFF_SZ] = { 0 };
		if (update)
		{
			OPERATION = "UPDATE";
			//update
			snprintf(sql_buff, BUFF_SZ, "update sbt_indentinfo set belongid=%d,servicetype=%d,operationway=%d,indenttype=%d,indentcontent='%s',createtime='%s' where id=%d",
				indentitem.belongid, indentitem.servicetype, indentitem.operationway, indentitem.indenttype, indentitem.indentcontent.c_str(), indentitem.createtime.c_str(),
				indentitem.id);
		}
		else
		{
			OPERATION = "INSERT";
			//insert 
			snprintf(sql_buff, BUFF_SZ, "insert into sbt_indentinfo (id,belongid,servicetype,operationway,indenttype,indentcontent,createtime) values(%d,%d,%d,%d,%d,'%s','%s')",
				indentitem.id, 
				indentitem.belongid, indentitem.servicetype, indentitem.operationway, indentitem.indenttype, indentitem.indentcontent.c_str(), indentitem.createtime.c_str());
		}

		//run sql
		_debug_to(loger_mysql, 0, ("[addindentinfo] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[addindentinfo]indent id=%d, %s success\n"), indentitem.id, OPERATION.c_str());
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[addindentinfo]indent id=%d, %s failed: %s\n"), indentitem.id, OPERATION.c_str(), error.c_str());
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool getindentlistinfo(VEC_FILTERINFO& vecfilterinfo, int pagesize, int pagenum, int& total, VEC_INDENTINFO& vecindentinfo)
	{
		if (simulation) return true;

		//where
		std::string str_where = " where 1 "; //为了统一使用where 1,故加条件关系只能为and
		int usedfilter_count = 0;
		for (VEC_FILTERINFO::iterator it = vecfilterinfo.begin(); it != vecfilterinfo.end(); ++it)
		{
			int filtertype = it->filtertype;
			std::string filterfield = it->filterfield;
			std::string filtervalue = it->filtervalue;
			if (!filterfield.empty() && !filtervalue.empty())
			{
				filterinfo usedfilter;
				usedfilter.filtertype = filtertype;
				usedfilter.filterfield = filterfield;
				usedfilter.filtervalue = filtervalue;
				usedfilter.to_utf8();

				std::string temp;
				char tempbuff[256] = { 0 };
				if (usedfilter.filtertype == 0)//单个值
				{
					snprintf(tempbuff, 256, " and cast(%s as char) like '%%%s%%' ", usedfilter.filterfield.c_str(), usedfilter.filtervalue.c_str());//CAST统一转换字段类型为字符
					temp = tempbuff;
				}
				if (usedfilter.filtertype == 1)//范围值
				{
					snprintf(tempbuff, 256, " and %s between %s ", usedfilter.filterfield.c_str(), usedfilter.filtervalue.c_str());//value值中有 and 关键字
					temp = tempbuff;
				}
				if (usedfilter.filtertype == 2)//枚举值
				{
					snprintf(tempbuff, 256, " and %s in (%s) ", usedfilter.filterfield.c_str(), usedfilter.filtervalue.c_str());
					temp = tempbuff;
				}
				if (usedfilter.filtertype == 3)//完整条件
				{
					snprintf(tempbuff, 256, " and (%s) ", usedfilter.filtervalue.c_str());
					temp = tempbuff;
				}

				str_where += temp;
				usedfilter_count += 1;
			}
		}
		_debug_to(loger_mysql, 0, ("[getindentlistinfo]vecfilterinfo size = %d\n"), usedfilter_count);

		//limit
		std::string str_limit = "";
		if (pagesize > 0 && pagenum > 0)
		{
			int throw_count = (pagenum - 1) * pagesize;
			int need_count = pagesize;

			char temp[256] = { 0 };
			snprintf(temp, 256, " limit %d,%d ", throw_count, need_count);
			str_limit = temp;
		}

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getindentlistinfo]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		//a
		std::string str_sqltotal = "select count(sbt_indentinfo.id) from sbt_indentinfo left join sbt_userinfo on sbt_indentinfo.belongid = sbt_userinfo.id";
		str_sqltotal += str_where;
		_debug_to(loger_mysql, 0, ("[getindentlistinfo] sql: %s\n"), str_sqltotal.c_str());
		mysql_query(&mysql, "SET NAMES UTF8");			//support chinese text
		if (!mysql_query(&mysql, str_sqltotal.c_str()))	//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[getindentlistinfo] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 1)//keep right
			{
				int row_total = atoi(row_value(row[0]).c_str());
				total = (row_total > 0) ? (row_total) : (0);
			}
			mysql_free_result(result);				//free result
			_debug_to(loger_mysql, 0, ("[getindentlistinfo]select total success, total=%d\n"), total);
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getindentlistinfo]mysql_query usertotal failed: %s\n"), error.c_str());
		}

		//b
		std::string str_sqlselect = "select sbt_indentinfo.id,sbt_indentinfo.belongid,sbt_userinfo.loginname,sbt_indentinfo.servicetype,operationway,indenttype,indentcontent,createtime from sbt_indentinfo left join sbt_userinfo on sbt_indentinfo.belongid = sbt_userinfo.id";
		str_sqlselect += str_where;
		str_sqlselect += str_limit;
		_debug_to(loger_mysql, 0, ("[getindentlistinfo] sql: %s\n"), str_sqlselect.c_str());
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, str_sqlselect.c_str()))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[getindentlistinfo] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row;							//table row data
			while (row = mysql_fetch_row(result))
			{
				if (row && colnum >= 8) //keep right
				{
					int i = 0;
					int			row_id = atoi(row_value(row[i++]).c_str());
					int			row_belongid = atoi(row_value(row[i++]).c_str());
					std::string row_loginname = row_value(row[i++]);
					int			row_servicetype = atoi(row_value(row[i++]).c_str());
					int			row_operationway = atoi(row_value(row[i++]).c_str());
					int			row_indenttype = atoi(row_value(row[i++]).c_str());
					std::string row_indentcontent = row_value(row[i++]);
					std::string row_createtime = row_value(row[i++]);


					indentinfo indentitem;
					indentitem.id = row_id;
					indentitem.belongid = row_belongid;
					indentitem.rootname = row_loginname;
					indentitem.servicetype = row_servicetype;
					indentitem.operationway = row_operationway;
					indentitem.indenttype = row_indenttype;
					indentitem.indentcontent = row_indentcontent;
					indentitem.createtime = row_createtime;
					vecindentinfo.push_back(indentitem);
				}
			}
			mysql_free_result(result);				//free result

			_debug_to(loger_mysql, 0, ("[getindentlistinfo]select humanlist success\n"));
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getindentlistinfo]mysql_query humanlist failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}

	//数字人模型资源
	bool addhumaninfo(humaninfo humanitem, bool update)
	{
		if (simulation) return true;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		humanitem.to_utf8();

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[addhumanlistinfo]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		std::string OPERATION = "";
		char sql_buff[BUFF_SZ] = { 0 };
		if (update)
		{
			OPERATION = "UPDATE";
			//update
			snprintf(sql_buff, BUFF_SZ, "update sbt_humansource set humanname='%s',contentid='%s',sourcefolder='%s',available=%d,speakspeed=%.6f,seriousspeed=%.1f,imagematting='%s',keyframe='%s',foreground='%s',background='%s',foreground2='%s',background2='%s',speakpath='%s',pwgpath='%s',mouthmodefile='%s',facemodefile='%s' where humanid='%s'",
				humanitem.humanname.c_str(), humanitem.contentid.c_str(), humanitem.sourcefolder.c_str(), humanitem.available, humanitem.speakspeed, humanitem.seriousspeed, humanitem.imagematting.c_str(), humanitem.keyframe.c_str(), humanitem.foreground.c_str(), humanitem.background.c_str(), humanitem.foreground2.c_str(), humanitem.background2.c_str(), humanitem.speakmodelpath.c_str(), humanitem.pwgmodelpath.c_str(), humanitem.mouthmodelfile.c_str(), humanitem.facemodelfile.c_str(),
				humanitem.humanid.c_str());//update
		}
		else
		{
			OPERATION = "INSERT";
			int next_id = newgetsequencenextvalue("sbt_humansource", &mysql);
			if (next_id <= 0) return false;

			//insert 
			humanitem.id = next_id;
			snprintf(sql_buff, BUFF_SZ, "insert into sbt_humansource (id,belongid,privilege,humanid,humanname,contentid,sourcefolder,available,speakspeed,seriousspeed,imagematting,keyframe,foreground,background,foreground2,background2,speakpath,pwgpath,mouthmodefile,facemodefile) values(%d, %d, %d, '%s', '%s', '%s', '%s', %d, %.6f, %.6f, '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s')",
				humanitem.id, humanitem.belongid, humanitem.privilege, humanitem.humanid.c_str(),
				humanitem.humanname.c_str(), humanitem.contentid.c_str(), humanitem.sourcefolder.c_str(), humanitem.available, humanitem.speakspeed, humanitem.seriousspeed,humanitem.imagematting.c_str(), humanitem.keyframe.c_str(), humanitem.foreground.c_str(), humanitem.background.c_str(), humanitem.foreground2.c_str(), humanitem.background2.c_str(), humanitem.speakmodelpath.c_str(), humanitem.pwgmodelpath.c_str(),humanitem.mouthmodelfile.c_str(), humanitem.facemodelfile.c_str());
		}

		//run sql
		_debug_to(loger_mysql, 0, ("[addhumanlistinfo] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[addhumanlistinfo]humanid=%d, %s success\n"), humanitem.humanid, OPERATION.c_str());
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[addhumanlistinfo]humanid=%d, %s failed: %s\n"), humanitem.humanid, OPERATION.c_str(), error.c_str());
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool gethumaninfo(std::string humanid, humaninfo& humanitem)
	{
		if (simulation) return true;
		if (humanid.empty()) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(humanid.c_str(), humanid.length(), humanid);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gethumaninfo]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select id,belongid,privilege,humanid,humanname,contentid,sourcefolder,available,speakspeed,seriousspeed,imagematting,keyframe,foreground,background,foreground2,background2,speakpath,pwgpath,mouthmodefile,facemodefile from sbt_humansource where humanid='%s'", humanid.c_str());//select	
		_debug_to(loger_mysql, 0, ("[gethumaninfo] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[gethumaninfo] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 20)//keep right
			{
				int i = 0;
				int row_id = atoi(row_value(row[i++]).c_str());
				int row_belongid = atoi(row_value(row[i++]).c_str());
				int row_privilege = atoi(row_value(row[i++]).c_str());
				std::string row_humanid = row_value(row[i++]);
				std::string row_humanname = row_value(row[i++]); 
				std::string row_contentid = row_value(row[i++]);
				std::string row_sourcefolder = row_value(row[i++]);
				int row_available = atoi(row_value(row[i++]).c_str());
				double row_speakspeed = atof(row_value(row[i++]).c_str());
				double row_seriousspeed = atof(row_value(row[i++]).c_str());
				std::string row_imagematting = row_value(row[i++]);   
				std::string row_keyframe = row_value(row[i++]);       
				std::string row_foreground = row_value(row[i++]);     
				std::string row_background = row_value(row[i++]);   
				std::string row_foreground2 = row_value(row[i++]);
				std::string row_background2 = row_value(row[i++]);
				std::string row_speakmodelpath = row_value(row[i++]); 
				std::string row_pwgmodelpath = row_value(row[i++]);   
				std::string row_mouthmodelfile = row_value(row[i++]); 
				std::string row_facemodelfile = row_value(row[i++]);  

				humanitem.id = row_id;
				humanitem.belongid = row_belongid;
				humanitem.privilege = row_privilege;
				humanitem.humanid = row_humanid;
				humanitem.humanname = row_humanname;
				humanitem.contentid = row_contentid;
				humanitem.sourcefolder = row_sourcefolder;
				humanitem.available = row_available;
				humanitem.speakspeed = row_speakspeed;
				humanitem.seriousspeed = row_seriousspeed;
				humanitem.imagematting = row_imagematting;
				humanitem.keyframe = row_keyframe;
				humanitem.foreground = row_foreground;
				humanitem.background = row_background;
				humanitem.foreground2 = row_foreground2;
				humanitem.background2 = row_background2;
				humanitem.speakmodelpath = row_speakmodelpath;
				humanitem.pwgmodelpath = row_pwgmodelpath;
				humanitem.mouthmodelfile = row_mouthmodelfile;
				humanitem.facemodelfile = row_facemodelfile;
			}
			else
			{
				ret = false;
				_debug_to(loger_mysql, 1, ("[gethumaninfo] select humaninfo count/colnum error\n"));
			}
			mysql_free_result(result);				//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gethumaninfo]mysql_query humaninfo failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool isexisthuman_humanid(std::string humanid)
	{
		if (simulation) return true;
		if (humanid.empty()) return false;

		bool ret = false;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(humanid.c_str(), humanid.length(), humanid);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[isexisthuman_humanid]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select * from sbt_humansource where humanid = '%s'", humanid.c_str());
		_debug_to(loger_mysql, 0, ("[isexisthuman_humanid] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[isexisthuman_humanid] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1)//keep right
				ret = true;

			mysql_free_result(result);				//free result
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;

	}
	bool isavailable_humanid(std::string humanid)
	{
		if (simulation) return true;
		if (humanid.empty()) return false;

		bool ret = false;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(humanid.c_str(), humanid.length(), humanid);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[isavailable_humanid]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select available from sbt_humansource where humanid = '%s'", humanid.c_str());
		_debug_to(loger_mysql, 0, ("[isavailable_humanid] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[isavailable_humanid] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 1)//keep right
			{
				int available = atoi(row_value(row[0]).c_str());
				ret = (available > 0) ? (true) : (false);
			}

			mysql_free_result(result);				//free result
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool gethumaninfo_label(std::string humanid, std::vector<int> vecbelongids, std::string& labelstring)
	{
		if (simulation) return true;
		if (humanid.empty()) return false;
		if (vecbelongids.empty()) return false;

		//where
		std::string str_where = " where 1 "; //为了统一使用where 1,故加条件关系只能为and
		if (!humanid.empty())//区分humanid
		{
			std::string temp; char tempbuff[256] = { 0 };
			snprintf(tempbuff, 256, " and humanid = '%s' ", humanid.c_str());
			temp = tempbuff;
			str_where += temp;
		}
		if (!vecbelongids.empty())//区分用户
		{
			str_where += " and belongid in (";
			for (size_t i = 0; i < vecbelongids.size(); i++)
			{
				std::string temp; char tempbuff[256] = { 0 };
				snprintf(tempbuff, 256, "%d", vecbelongids[i]);
				temp = tempbuff;
				str_where += temp;

				if (i == 9 || i == (vecbelongids.size() - 1))//最多枚举10个id
					break;
				str_where += ",";
			}
			str_where += ") ";
		}

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(humanid.c_str(), humanid.length(), humanid);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gethumaninfo_label]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		std::string str_sql = "select contentid from sbt_humansource";
		str_sql += str_where;
		_debug_to(loger_mysql, 0, ("[gethumaninfo_label] sql: %s\n"), str_sql.c_str());

		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, str_sql.c_str()))	//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[gethumaninfo_label] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 1)//keep right
			{
				int i = 0;
				std::string row_contentid = row_value(row[i++]);
				labelstring = row_contentid;
			}
			else
			{
				ret = false;
				_debug_to(loger_mysql, 1, ("[gethumaninfo_label] select humaninfo_label count/colnum error\n"));
			}
			mysql_free_result(result);				//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gethumaninfo_label]mysql_query humaninfo_label failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool sethumaninfo_label(std::string humanid, std::vector<int> vecbelongids, std::string labelstring)
	{
		if (simulation) return true;
		if (humanid.empty()) return false;
		if (vecbelongids.empty()) return false;

		//where
		std::string str_where = " where 1 "; //为了统一使用where 1,故加条件关系只能为and
		if (!humanid.empty())//区分humanid
		{
			std::string temp; char tempbuff[256] = { 0 };
			snprintf(tempbuff, 256, " and humanid = '%s' ", humanid.c_str());
			temp = tempbuff;
			str_where += temp;
		}
		if (!vecbelongids.empty())//区分用户
		{
			str_where += " and belongid in (";
			for (size_t i = 0; i < vecbelongids.size(); i++)
			{
				std::string temp; char tempbuff[256] = { 0 };
				snprintf(tempbuff, 256, "%d", vecbelongids[i]);
				temp = tempbuff;
				str_where += temp;

				if (i == 9 || i == (vecbelongids.size() - 1))//最多枚举10个id
					break;
				str_where += ",";
			}
			str_where += ") ";
		}

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(humanid.c_str(), humanid.length(), humanid);
		ansi_to_utf8(labelstring.c_str(), labelstring.length(), labelstring);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[sethumaninfo_label]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		std::string str_sql = "";
		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "update sbt_humansource set contentid='%s'", labelstring.c_str()); str_sql = sql_buff;
		str_sql += str_where;
		_debug_to(loger_mysql, 0, ("[sethumaninfo_label] sql: %s\n"), str_sql.c_str());

		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, str_sql.c_str()))	//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[sethumaninfo_label]humanid = %s, update humaninfo_label success\n"), humanid.c_str());
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[sethumaninfo_label]humanid = %s, update humaninfo_label failed: %s\n"), humanid.c_str(), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool gethumaninfo_remaintime(std::string humanid, std::vector<int> vecbelongids, long long& remaintime)
	{
		if (simulation) return true;
		if (humanid.empty()) return false;
		if (vecbelongids.empty()) return false;

		//where
		std::string str_where = " where 1 "; //为了统一使用where 1,故加条件关系只能为and
		if (!humanid.empty())//区分humanid
		{
			std::string temp; char tempbuff[256] = { 0 };
			snprintf(tempbuff, 256, " and humanid = '%s' ", humanid.c_str());
			temp = tempbuff;
			str_where += temp;
		}
		if (!vecbelongids.empty())//区分用户
		{
			str_where += " and belongid in (";
			for (size_t i = 0; i < vecbelongids.size(); i++)
			{
				std::string temp; char tempbuff[256] = { 0 };
				snprintf(tempbuff, 256, "%d", vecbelongids[i]);
				temp = tempbuff;
				str_where += temp;

				if (i == 9 || i == (vecbelongids.size() - 1))//最多枚举10个id
					break;
				str_where += ",";
			}
			str_where += ") ";
		}

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(humanid.c_str(), humanid.length(), humanid);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gethumaninfo_remaintime]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		std::string str_sql = "select remaintime from sbt_humansource";
		str_sql += str_where;
		_debug_to(loger_mysql, 0, ("[gethumaninfo_remaintime] sql: %s\n"), str_sql.c_str());

		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, str_sql.c_str()))	//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[gethumaninfo_remaintime] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 1)//keep right
			{
				int i = 0;
				int row_remain = atoi(row_value(row[i++]).c_str());
				remaintime = max(0, row_remain);
				_debug_to(loger_mysql, 0, ("[gethumaninfo_remaintime]humanid = %s, select humaninfo_remaintime success,remaintime = %lld\n"), humanid.c_str(), remaintime);
			}
			else
			{
				ret = false;
				_debug_to(loger_mysql, 1, ("[gethumaninfo_remaintime]humanid = %s, select humaninfo_remaintime count/colnum error\n"), humanid.c_str());
			}
			mysql_free_result(result);				//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gethumaninfo_remaintime]humanid = %s, query humaninfo_remaintime failed: %s\n"), humanid.c_str(), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool sethumaninfo_remaintime(std::string humanid, std::vector<int> vecbelongids, long long remaintime)
	{
		if (simulation) return true;
		if (humanid.empty()) return false;
		if (vecbelongids.empty()) return false;

		//where
		std::string str_where = " where 1 "; //为了统一使用where 1,故加条件关系只能为and
		if (!humanid.empty())//区分humanid
		{
			std::string temp; char tempbuff[256] = { 0 };
			snprintf(tempbuff, 256, " and humanid = '%s' ", humanid.c_str());
			temp = tempbuff;
			str_where += temp;
		}
		if (!vecbelongids.empty())//区分用户
		{
			str_where += " and belongid in (";
			for (size_t i = 0; i < vecbelongids.size(); i++)
			{
				std::string temp; char tempbuff[256] = { 0 };
				snprintf(tempbuff, 256, "%d", vecbelongids[i]);
				temp = tempbuff;
				str_where += temp;

				if (i == 9 || i == (vecbelongids.size() - 1))//最多枚举10个id
					break;
				str_where += ",";
			}
			str_where += ") ";
		}

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(humanid.c_str(), humanid.length(), humanid);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[sethumaninfo_remaintime]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		std::string str_sql = "";
		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "update sbt_humansource set remaintime=%lld", remaintime); str_sql = sql_buff;
		str_sql += str_where;
		_debug_to(loger_mysql, 0, ("[sethumaninfo_remaintime] sql: %s\n"), str_sql.c_str());

		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, str_sql.c_str()))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[sethumaninfo_remaintime]humanid = %s, update humaninfo_remaintime success\n"), humanid.c_str());
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[sethumaninfo_remaintime]humanid = %s, update humaninfo_remaintime failed: %s\n"), humanid.c_str(), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool gethumaninfo_remaintime(int id, long long& remaintime)
	{
		if (simulation) return true;
		if (id <= 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gethumaninfo_remaintime]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select remaintime from sbt_humansource where id=%d", id);//select	
		_debug_to(loger_mysql, 0, ("[gethumaninfo_remaintime] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[gethumaninfo_remaintime] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 1)//keep right
			{
				int i = 0;
				int row_remain = atoi(row_value(row[i++]).c_str());
				remaintime = max(0, row_remain);
				_debug_to(loger_mysql, 0, ("[gethumaninfo_remaintime]id = %d, select humaninfo_remaintime success,remaintime = %lld\n"), id, remaintime);
			}
			else
			{
				ret = false;
				_debug_to(loger_mysql, 1, ("[gethumaninfo_remaintime]id = %d, select humaninfo_remaintime count/colnum error\n"), id);
			}
			mysql_free_result(result);				//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gethumaninfo_remaintime]id = %d, query humaninfo_remaintime failed: %s\n"), id, error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool sethumaninfo_remaintime(int id, long long remaintime)
	{
		if (simulation) return true;
		if (id <= 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[sethumaninfo_remaintime]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		//
		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "update sbt_humansource set remaintime=%lld where id=%d", remaintime, id);//update
		_debug_to(loger_mysql, 0, ("[sethumaninfo_remaintime] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[sethumaninfo_remaintime]id = %d, update humaninfo_remaintime success\n"), id);
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[sethumaninfo_remaintime]id = %d, update humaninfo_remaintime failed: %s\n"), id, error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}

	bool gethumanid_all(std::vector<int>& vechumanid)
	{
		if (simulation) return true;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gethumancode_all]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select id from sbt_humansource where remaintime > 0");//select	
		_debug_to(loger_mysql, 0, ("[gethumancode_all] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{

			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[gethumancode_all] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row;							//table row data
			while (row = mysql_fetch_row(result))
			{
				if (row && colnum >= 1) //keep right
				{
					int i = 0;
					int row_id = atoi(row_value(row[i++]).c_str());

					vechumanid.push_back(row_id);
				}
			}
			mysql_free_result(result);				//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gethumancode_all]mysql_query human_id failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool gethumanlistinfo(std::string humanid, std::vector<int> vecbelongids, VEC_HUMANINFO& vechumaninfo)
	{
		if (simulation) return true;
		if (vecbelongids.empty()) return false;

		//where
		std::string str_where = " where 1 "; //为了统一使用where 1,故加条件关系只能为and
		if (!humanid.empty())//区分humanid
		{
			std::string temp; char tempbuff[256] = { 0 };
			snprintf(tempbuff, 256, " and humanid = '%s' ", humanid.c_str());
			temp = tempbuff;
			str_where += temp;
		}
		if (!vecbelongids.empty())//区分用户
		{
			str_where += " and belongid in (";
			for (size_t i = 0; i < vecbelongids.size(); i++)
			{
				std::string temp; char tempbuff[256] = { 0 };
				snprintf(tempbuff, 256, "%d", vecbelongids[i]);
				temp = tempbuff;
				str_where += temp;

				if (i == 9 || i == (vecbelongids.size() - 1))//最多枚举10个id
					break;
				str_where += ",";
			}
			str_where += ") ";
		}

		//public
		std::string str_where_public = "";// " or privilege = 0";//公共资源

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(humanid.c_str(), humanid.length(), humanid);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gethumanlistinfo]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		std::string str_sql = "select id,belongid,privilege,humanid,humanname,contentid,sourcefolder,available,speakspeed,seriousspeed,imagematting,keyframe,foreground,background,foreground2,background2,speakpath,pwgpath,mouthmodefile,facemodefile from sbt_humansource";
		str_sql += str_where;
		str_sql += str_where_public;
		_debug_to(loger_mysql, 0, ("[gethumanlistinfo] sql: %s\n"), str_sql.c_str());

		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, str_sql.c_str()))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[gethumanlistinfo] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row;							//table row data
			while (row = mysql_fetch_row(result))
			{
				if (row && colnum >= 20) //keep right
				{
					int i = 0;
					int row_id = atoi(row_value(row[i++]).c_str());
					int row_belongid = atoi(row_value(row[i++]).c_str());
					int row_privilege = atoi(row_value(row[i++]).c_str());
					std::string row_humanid = row_value(row[i++]);
					std::string row_humanname = row_value(row[i++]); 
					std::string row_contentid = row_value(row[i++]);
					std::string row_sourcefolder = row_value(row[i++]);
					int row_available = atoi(row_value(row[i++]).c_str());
					double row_speakspeed = atof(row_value(row[i++]).c_str());
					double row_seriousspeed = atof(row_value(row[i++]).c_str());
					std::string row_imagematting = row_value(row[i++]);   
					std::string row_keyframe = row_value(row[i++]);       
					std::string row_foreground = row_value(row[i++]);     
					std::string row_background = row_value(row[i++]);   
					std::string row_foreground2 = row_value(row[i++]);
					std::string row_background2 = row_value(row[i++]);
					std::string row_speakmodelpath = row_value(row[i++]); 
					std::string row_pwgmodelpath = row_value(row[i++]);   
					std::string row_mouthmodelfile = row_value(row[i++]); 
					std::string row_facemodelfile = row_value(row[i++]);  

					humaninfo humanitem;
					humanitem.id = row_id;
					humanitem.belongid = row_belongid;
					humanitem.privilege = row_privilege;
					humanitem.humanid = row_humanid;
					humanitem.humanname = row_humanname;
					humanitem.contentid = row_contentid;
					humanitem.sourcefolder = row_sourcefolder;
					humanitem.available = row_available;
					humanitem.speakspeed = row_speakspeed;
					humanitem.seriousspeed = row_seriousspeed;
					humanitem.imagematting = row_imagematting;
					humanitem.keyframe = row_keyframe;
					humanitem.foreground = row_foreground;
					humanitem.background = row_background;
					humanitem.foreground2 = row_foreground2;
					humanitem.background2 = row_background2;
					humanitem.speakmodelpath = row_speakmodelpath;
					humanitem.pwgmodelpath = row_pwgmodelpath;
					humanitem.mouthmodelfile = row_mouthmodelfile;
					humanitem.facemodelfile = row_facemodelfile;
					
					vechumaninfo.push_back(humanitem);
				}
			}	
			mysql_free_result(result);				//free result

			_debug_to(loger_mysql, 0, ("[gethumanlistinfo]select humanlist success\n"));
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gethumanlistinfo]mysql_query humanlist failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool deletehuman_humanid(std::string humanid, bool deletefile, std::string& errmsg)
	{
		if (simulation) return true;
		if (humanid.empty()) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(humanid.c_str(), humanid.length(), humanid);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql); errmsg = error;
			_debug_to(loger_mysql, 1, ("[deletehuman_humanid]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		if (deletefile)
		{
			snprintf(sql_buff, BUFF_SZ, "select imagematting,mouthmodefile,facemodefile,keyframe,foreground,background,speakpath,pwgpath from sbt_humansource where humanid = '%s'", humanid.c_str());
			_debug_to(loger_mysql, 0, ("[deletehuman_humanid] sql: %s\n"), sql_buff);
			mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
			if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
			{
				MYSQL_RES* result;						//table data struct
				result = mysql_store_result(&mysql);    //sava dada to result
				int rownum = mysql_num_rows(result);	//get row number
				int colnum = mysql_num_fields(result);  //get col number
				_debug_to(loger_mysql, 0, ("[deletehuman_humanid] rownum=%d,colnum=%d\n"), rownum, colnum);

				MYSQL_ROW row = mysql_fetch_row(result);//table row data
				if (row && rownum >= 1)//keep right
				{
					if (colnum >= 1)
					{
						int i = 0;
						while (colnum--)
						{
							std::string physicalfilepath = row_value(row[i++]);
							if (is_existfile(physicalfilepath.c_str()))
								remove(physicalfilepath.c_str());
							_debug_to(loger_mysql, 0, ("[deletehuman_humanid] delete physical file... [%s]\n"), physicalfilepath);
						}
					}
				}
				mysql_free_result(result);				//free result
				_debug_to(loger_mysql, 0, ("[deletehuman_humanid]select filepath before delete human success\n"));
			}
			else
			{
				ret = false;
				std::string error = mysql_error(&mysql);
				_debug_to(loger_mysql, 1, ("[deletehuman_humanid]mysql_query filepath before delete human failed: %s\n"), error.c_str());
			}
		}

		snprintf(sql_buff, BUFF_SZ, "delete from sbt_humansource where humanid = '%s'", humanid.c_str());
		_debug_to(loger_mysql, 0, ("[deletehuman_humanid] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[deletehuman_humanid]delete human by humanid success, humanid = '%s'\n"), humanid.c_str());
			errmsg = "delete human by humanid success";
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql); errmsg = error;
			_debug_to(loger_mysql, 1, ("[deletehuman_humanid]delete human by humanid failed, humanid = '%s'\n"), humanid.c_str());
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}

	//稿件任务
	bool addtaskinfo(int& taskid, taskinfo taskitem, bool update)
	{
		if (simulation) return true;
		if (taskid < 0) return false;
		if (taskid == 0) update = false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		taskitem.to_utf8();

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[addtaskinfo]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char tempbuff[16] = { 0 };
		std::string speak_speed = ""; snprintf(tempbuff, 16, "%.6f", taskitem.speakspeed); speak_speed = tempbuff;
		std::string ssmltext_md5 = taskitem.ssmltext + speak_speed;
		std::string textguid = md5::getStringMD5(ssmltext_md5);//must before utf8 convert,because isexist input textguid from ansi calc

		//
		std::string OPERATION = "";
		char sql_buff[BUFF_SZ] = { 0 };
		if (update)
		{
			//update
			OPERATION = "UPDATE";
			snprintf(sql_buff, BUFF_SZ, "update sbt_doctask set versionname='%s',version=%d,tasktype=%d,moodtype=%d,speakspeed=%.6f,taskname='%s',createtime='%s',scannedtime='%s',finishtime='%s',priority=%d,islastedit=%d,humanid='%s',humanname='%s',ssmltext='%s',textguid='%s',audiofile='%s',audioformat='%s',audiolength=%d,videofile='%s',keyframe='%s',videoformat='%s',videolength=%d,videowidth=%d,videoheight=%d,videofps=%.2f,foreground='%s',front_left=%.6f,front_right=%.6f,front_top=%.6f,front_bottom=%.6f,background='%s',backaudio='%s',back_volume=%.6f,back_loop=%d,back_start=%d,back_end=%d,wnd_width=%d,wnd_height=%d  where taskid=%d",
				taskitem.versionname.c_str(), taskitem.version, taskitem.tasktype, taskitem.moodtype, taskitem.speakspeed, taskitem.taskname.c_str(), taskitem.createtime.c_str(), taskitem.scannedtime.c_str(), taskitem.finishtime.c_str(), taskitem.priority, taskitem.islastedit, taskitem.humanid.c_str(), taskitem.humanname.c_str(), taskitem.ssmltext.c_str(), textguid.c_str(),
				taskitem.audio_path.c_str(), taskitem.audio_format.c_str(), taskitem.audio_length,
				taskitem.video_path.c_str(), taskitem.video_keyframe.c_str(), taskitem.video_format.c_str(), taskitem.video_length, taskitem.video_width, taskitem.video_height, taskitem.video_fps, taskitem.foreground.c_str(), taskitem.front_left,taskitem.front_right,taskitem.front_top,taskitem.front_bottom,
				taskitem.background.c_str(), taskitem.backaudio.c_str(), taskitem.back_volume, taskitem.back_loop, taskitem.back_start, taskitem.back_end, taskitem.window_width, taskitem.window_height,
				taskid);
		}
		else
		{
			OPERATION = "INSERT";
			int next_id = newgetsequencenextvalue("sbt_doctask", &mysql);
			if (next_id <= 0) return false;

			//insert 
			taskid = next_id;
			taskitem.taskid = next_id;
			snprintf(sql_buff, BUFF_SZ, "insert into sbt_doctask (taskid,belongid,privilege,groupid,versionname,version,tasktype,moodtype,speakspeed,taskname,createtime,scannedtime,finishtime,priority,islastedit,humanid,humanname,ssmltext,textguid,audiofile,audioformat,audiolength,videofile,keyframe,videoformat,videolength,videowidth,videoheight,videofps,foreground,front_left,front_right,front_top,front_bottom,background,backaudio,back_volume,back_loop,back_start,back_end,wnd_width,wnd_height) values(%d, %d, %d, '%s', '%s', %d, %d, %d, %.6f, '%s', '%s', '%s', '%s', %d, %d, '%s', '%s', '%s', '%s', '%s', '%s', %d, '%s', '%s', '%s', %d, %d, %d, %.2f, '%s', %.6f, %.6f, %.6f, %.6f, '%s', '%s', %.6f, %d, %d, %d, %d, %d)",
				taskid, taskitem.belongid, taskitem.privilege, taskitem.groupid.c_str(),
				taskitem.versionname.c_str(), taskitem.version, taskitem.tasktype, taskitem.moodtype, taskitem.speakspeed, taskitem.taskname.c_str(), taskitem.createtime.c_str(), taskitem.scannedtime.c_str(), taskitem.finishtime.c_str(), taskitem.priority, taskitem.islastedit, taskitem.humanid.c_str(), taskitem.humanname.c_str(), taskitem.ssmltext.c_str(), textguid.c_str(),
				taskitem.audio_path.c_str(), taskitem.audio_format.c_str(), taskitem.audio_length,
				taskitem.video_path.c_str(), taskitem.video_keyframe.c_str(), taskitem.video_format.c_str(), taskitem.video_length, taskitem.video_width, taskitem.video_height, taskitem.video_fps, taskitem.foreground.c_str(), taskitem.front_left, taskitem.front_right, taskitem.front_top, taskitem.front_bottom,
				taskitem.background.c_str(), taskitem.backaudio.c_str(), taskitem.back_volume, taskitem.back_loop, taskitem.back_start, taskitem.back_end, taskitem.window_width, taskitem.window_height);
		}

		//run sql
		_debug_to(loger_mysql, 0, ("[addtaskinfo] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[addtaskinfo]task %d, %s success\n"), taskid, OPERATION.c_str());
		}
		else 
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[addtaskinfo]task %d, %s failed: %s\n"), taskid, OPERATION.c_str(), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool gettaskinfo(int taskid, taskinfo& taskitem)
	{
		if (simulation) return true;
		if (taskid <= 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gettaskinfo]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select taskid,belongid,privilege,groupid,versionname,version,tasktype,moodtype,speakspeed,taskname,state,progress,createtime,scannedtime,finishtime,priority,islastedit,humanid,humanname,ssmltext,audiofile,audioformat,audiolength,videofile,keyframe,videoformat,videolength,videowidth,videoheight,videofps,foreground,front_left,front_right,front_top,front_bottom,background,backaudio,back_volume,back_loop,back_start,back_end,wnd_width,wnd_height from sbt_doctask where taskid=%d", taskid);//select	
		_debug_to(loger_mysql, 0, ("[gettaskinfo] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[gettaskinfo] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 43)//keep right
			{
				int i = 0;
				int row_taskid = atoi(row_value(row[i++]).c_str());
				int row_belongid = atoi(row_value(row[i++]).c_str());
				int row_privilege = atoi(row_value(row[i++]).c_str());
				std::string row_groupid = row_value(row[i++]);
				std::string row_versionname = row_value(row[i++]);
				int row_version = atoi(row_value(row[i++]).c_str());
				int row_tasktype = atoi(row_value(row[i++]).c_str());
				int row_moodtype = atoi(row_value(row[i++]).c_str());
				double row_speakspeed = atof(row_value(row[i++]).c_str());
				std::string row_taskname  = row_value(row[i++]);   
				int row_taskstate = atoi(row_value(row[i++]).c_str());
				int row_taskprogress = atoi(row_value(row[i++]).c_str());
				std::string row_createtime = row_value(row[i++]);
				std::string row_scannedtime = row_value(row[i++]);
				std::string row_finishtime = row_value(row[i++]);
				int row_priority = atoi(row_value(row[i++]).c_str());
				int row_islastedit = atoi(row_value(row[i++]).c_str());
				std::string row_humanid   = row_value(row[i++]);
				std::string row_humanname = row_value(row[i++]);  
				std::string row_ssmltext = row_value(row[i++]);   row_ssmltext = jsonstr_replace(row_ssmltext);
				std::string row_audiofile = row_value(row[i++]);
				std::string row_audioformat = row_value(row[i++]);
				int			row_audiolength = atoi(row_value(row[i++]).c_str());
				std::string row_videofile   = row_value(row[i++]);
				std::string row_videokeyframe = row_value(row[i++]);
				std::string row_videoformat = row_value(row[i++]);
				int			row_videolength = atoi(row_value(row[i++]).c_str());
				int			row_videowidth  = atoi(row_value(row[i++]).c_str());
				int			row_videoheight = atoi(row_value(row[i++]).c_str());
				double		row_videofps	= atof(row_value(row[i++]).c_str());
				std::string row_foreground = row_value(row[i++]).c_str();
				double		row_front_left = atof(row_value(row[i++]).c_str());
				double		row_front_right = atof(row_value(row[i++]).c_str());
				double		row_front_top = atof(row_value(row[i++]).c_str());
				double		row_front_bottom = atof(row_value(row[i++]).c_str());
				std::string row_background = row_value(row[i++]).c_str();
				std::string row_backaudio = row_value(row[i++]).c_str();
				double		row_back_volume = atof(row_value(row[i++]).c_str());
				int			row_back_loop = atoi(row_value(row[i++]).c_str());
				int			row_back_start = atoi(row_value(row[i++]).c_str());
				int			row_back_end = atoi(row_value(row[i++]).c_str());
				int			row_wnd_width = atoi(row_value(row[i++]).c_str());
				int			row_wnd_height = atoi(row_value(row[i++]).c_str());

				taskitem.taskid = row_taskid;
				taskitem.belongid = row_belongid;
				taskitem.privilege = row_privilege;
				taskitem.groupid = row_groupid;
				taskitem.versionname = row_versionname;
				taskitem.version = row_version;
				taskitem.tasktype = row_tasktype;
				taskitem.moodtype = row_moodtype;
				taskitem.speakspeed = row_speakspeed;
				taskitem.taskname = row_taskname;
				taskitem.taskstate = row_taskstate;
				taskitem.taskprogress = row_taskprogress;
				taskitem.createtime = row_createtime;
				taskitem.scannedtime = row_scannedtime;
				taskitem.finishtime = row_finishtime;
				taskitem.priority = row_priority;
				taskitem.islastedit = row_islastedit;
				taskitem.humanid = row_humanid;
				taskitem.humanname = row_humanname;
				taskitem.ssmltext = row_ssmltext;
				taskitem.audio_path = row_audiofile;
				taskitem.audio_format = row_audioformat;
				taskitem.audio_length = row_audiolength;
				taskitem.video_path = row_videofile;
				taskitem.video_keyframe = row_videokeyframe;
				taskitem.video_format = row_videoformat;
				taskitem.video_length = row_videolength;
				taskitem.video_width = row_videowidth;
				taskitem.video_height = row_videoheight;
				taskitem.video_fps = row_videofps;
				taskitem.foreground = row_foreground;
				taskitem.front_left = row_front_left;
				taskitem.front_right = row_front_right;
				taskitem.front_top = row_front_top;
				taskitem.front_bottom = row_front_bottom;
				taskitem.background = row_background;
				taskitem.backaudio = row_backaudio;
				taskitem.back_volume = row_back_volume;
				taskitem.back_loop = row_back_loop;
				taskitem.back_start = row_back_start;
				taskitem.back_end = row_back_end;
				taskitem.window_width = row_wnd_width;
				taskitem.window_height = row_wnd_height;
			}
			else
			{
				ret = false;
				_debug_to(loger_mysql, 1, ("[gettaskinfo] select taskinfo count/colnum error\n"));
			}
			mysql_free_result(result);				//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gettaskinfo]mysql_query taskinfo failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool clearscannedtime_id(int taskid)
	{
		if (simulation) return true;
		if (taskid <= 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[clearscannedtime_id]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		//
		std::string default_scannedtime = "";
		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "update sbt_doctask set scannedtime='%s' where taskid=%d", default_scannedtime.c_str(), taskid);//clear
		_debug_to(loger_mysql, 0, ("[clearscannedtime_id] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[clearscannedtime_id]task %d, clear scannedtime success\n"), taskid);
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[clearscannedtime_id]task %d, clear scannedtime failed: %s\n"), taskid, error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool setscannedtime_nextrun(std::string scannedtime)
	{
		if (simulation) return true;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(scannedtime.c_str(), scannedtime.length(), scannedtime);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setscannedtime_nextrun]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		//
		std::string default_scannedtime = "";
		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "update sbt_doctask set scannedtime='%s' where state=255 and scannedtime='%s' order by priority desc limit 1", scannedtime.c_str(), default_scannedtime.c_str());//update
		//_debug_to(loger_mysql, 0, ("[setscannedtime_nextrun] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			//_debug_to(loger_mysql, 0, ("[setscannedtime_nextrun] update task scannedtime success\n"));
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setscannedtime_nextrun] update task scannedtime failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool gettaskinfo_nextrun(std::string scannedtime, taskinfo& taskitem)
	{
		if (simulation) return true;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(scannedtime.c_str(), scannedtime.length(), scannedtime);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gettaskinfo_nextrun]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select taskid,belongid,privilege,groupid,versionname,version,tasktype,moodtype,speakspeed,taskname,state,progress,createtime,scannedtime,finishtime,priority,islastedit,humanid,humanname,ssmltext,audiofile,audioformat,audiolength,videofile,keyframe,videoformat,videolength,videowidth,videoheight,videofps,foreground,front_left,front_right,front_top,front_bottom,background,backaudio,back_volume,back_loop,back_start,back_end,wnd_width,wnd_height from sbt_doctask where state=255 and scannedtime='%s'", scannedtime.c_str());//select	
		//_debug_to(loger_mysql, 0, ("[gettaskinfo_nextrun] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			//_debug_to(loger_mysql, 0, ("[gettaskinfo_nextrun] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 43)//keep right
			{
				int i = 0;
				int row_taskid = atoi(row_value(row[i++]).c_str());
				int row_belongid = atoi(row_value(row[i++]).c_str());
				int row_privilege = atoi(row_value(row[i++]).c_str());
				std::string row_groupid = row_value(row[i++]);
				std::string row_versionname = row_value(row[i++]);
				int row_version = atoi(row_value(row[i++]).c_str());
				int row_tasktype = atoi(row_value(row[i++]).c_str());
				int row_moodtype = atoi(row_value(row[i++]).c_str());
				double row_speakspeed = atof(row_value(row[i++]).c_str());
				std::string row_taskname = row_value(row[i++]);
				int row_taskstate = atoi(row_value(row[i++]).c_str());
				int row_taskprogress = atoi(row_value(row[i++]).c_str());
				std::string row_createtime = row_value(row[i++]);
				std::string row_scannedtime = row_value(row[i++]);
				std::string row_finishtime = row_value(row[i++]);
				int row_priority = atoi(row_value(row[i++]).c_str());
				int row_islastedit = atoi(row_value(row[i++]).c_str());
				std::string row_humanid = row_value(row[i++]);
				std::string row_humanname = row_value(row[i++]);
				std::string row_ssmltext = row_value(row[i++]);   row_ssmltext = jsonstr_replace(row_ssmltext);
				std::string row_audiofile = row_value(row[i++]);
				std::string row_audioformat = row_value(row[i++]);
				int			row_audiolength = atoi(row_value(row[i++]).c_str());
				std::string row_videofile = row_value(row[i++]);
				std::string row_videokeyframe = row_value(row[i++]);
				std::string row_videoformat = row_value(row[i++]);
				int			row_videolength = atoi(row_value(row[i++]).c_str());
				int			row_videowidth = atoi(row_value(row[i++]).c_str());
				int			row_videoheight = atoi(row_value(row[i++]).c_str());
				double		row_videofps = atof(row_value(row[i++]).c_str());
				std::string row_foreground = row_value(row[i++]).c_str();
				double		row_front_left = atof(row_value(row[i++]).c_str());
				double		row_front_right = atof(row_value(row[i++]).c_str());
				double		row_front_top = atof(row_value(row[i++]).c_str());
				double		row_front_bottom = atof(row_value(row[i++]).c_str());
				std::string row_background = row_value(row[i++]).c_str();
				std::string row_backaudio = row_value(row[i++]).c_str();
				double		row_back_volume = atof(row_value(row[i++]).c_str());
				int			row_back_loop = atoi(row_value(row[i++]).c_str());
				int			row_back_start = atoi(row_value(row[i++]).c_str());
				int			row_back_end = atoi(row_value(row[i++]).c_str());
				int			row_wnd_width = atoi(row_value(row[i++]).c_str());
				int			row_wnd_height = atoi(row_value(row[i++]).c_str());

				taskitem.taskid = row_taskid;
				taskitem.belongid = row_belongid;
				taskitem.privilege = row_privilege;
				taskitem.groupid = row_groupid;
				taskitem.versionname = row_versionname;
				taskitem.version = row_version;
				taskitem.tasktype = row_tasktype;
				taskitem.moodtype = row_moodtype;
				taskitem.speakspeed = row_speakspeed;
				taskitem.taskname = row_taskname;
				taskitem.taskstate = row_taskstate;
				taskitem.taskprogress = row_taskprogress;
				taskitem.createtime = row_createtime;
				taskitem.scannedtime = row_scannedtime;
				taskitem.finishtime = row_finishtime;
				taskitem.priority = row_priority;
				taskitem.islastedit = row_islastedit;
				taskitem.humanid = row_humanid;
				taskitem.humanname = row_humanname;
				taskitem.ssmltext = row_ssmltext;
				taskitem.audio_path = row_audiofile;
				taskitem.audio_format = row_audioformat;
				taskitem.audio_length = row_audiolength;
				taskitem.video_path = row_videofile;
				taskitem.video_keyframe = row_videokeyframe;
				taskitem.video_format = row_videoformat;
				taskitem.video_length = row_videolength;
				taskitem.video_width = row_videowidth;
				taskitem.video_height = row_videoheight;
				taskitem.video_fps = row_videofps;
				taskitem.foreground = row_foreground;
				taskitem.front_left = row_front_left;
				taskitem.front_right = row_front_right;
				taskitem.front_top = row_front_top;
				taskitem.front_bottom = row_front_bottom;
				taskitem.background = row_background;
				taskitem.backaudio = row_backaudio;
				taskitem.back_volume = row_back_volume;
				taskitem.back_loop = row_back_loop;
				taskitem.back_start = row_back_start;
				taskitem.back_end = row_back_end;
				taskitem.window_width = row_wnd_width;
				taskitem.window_height = row_wnd_height;

				_debug_to(loger_mysql, 0, ("[gettaskinfo_nextrun] select taskinfo success\n"));
			}
			else
			{
				ret = false;
				//_debug_to(loger_mysql, 0, ("[gettaskinfo_nextrun] select taskinfo count/colnum error\n"));
			}
			mysql_free_result(result);				//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gettaskinfo_nextrun]mysql_query taskinfo failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool setaudiopath(int taskid, std::string audiopath)
	{
		if (simulation) return true;
		if (taskid <= 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(audiopath.c_str(), audiopath.length(), audiopath);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setaudiopath]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		//
		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "update sbt_doctask set audiofile='%s' where taskid=%d", audiopath.c_str(), taskid);//update
		_debug_to(loger_mysql, 0, ("[setaudiopath] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[setaudiopath]task %d, update audiopath success\n"), taskid);
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setaudiopath]task %d, update audiopath failed: %s\n"), taskid, error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool setvideopath(int taskid, std::string videopath)
	{
		if (simulation) return true;
		if (taskid <= 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(videopath.c_str(), videopath.length(), videopath);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setvideopath]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		//
		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "update sbt_doctask set videofile='%s' where taskid=%d", videopath.c_str(), taskid);//update
		_debug_to(loger_mysql, 0, ("[setvideopath] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[setvideopath]task %d, update videopath success\n"), taskid);
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setvideopath]task %d, update videopath failed: %s\n"), taskid, error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool setfinishtime(int taskid, std::string finishtime)
	{
		if (simulation) return true;
		if (taskid <= 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(finishtime.c_str(), finishtime.length(), finishtime);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setfinishtime]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		//
		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "update sbt_doctask set finishtime='%s' where taskid=%d", finishtime.c_str(), taskid);//update
		_debug_to(loger_mysql, 0, ("[setfinishtime] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[setfinishtime]task %d, update finishtime success\n"), taskid);
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setfinishtime]task %d, update finishtime failed: %s\n"), taskid, error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool setkeyframepath(int taskid, std::string keyframepath)
	{
		if (simulation) return true;
		if (taskid <= 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(keyframepath.c_str(), keyframepath.length(), keyframepath);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setkeyframepath]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		//
		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "update sbt_doctask set keyframe='%s' where taskid=%d", keyframepath.c_str(), taskid);//update
		_debug_to(loger_mysql, 0, ("[setkeyframepath] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[setkeyframepath]task %d, update keyframe success\n"), taskid);
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setkeyframepath]task %d, update keyframe failed: %s\n"), taskid, error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool setpriority(int taskid, int priority)
	{
		if (simulation) return true;
		if (taskid <= 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setpriority]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		//
		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "update sbt_doctask set priority=%d where taskid=%d", priority, taskid);//update
		_debug_to(loger_mysql, 0, ("[setpriority] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[setpriority]task %d, update priority success\n"), taskid);
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setpriority]task %d, update priority failed: %s\n"), taskid, error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool settasklastedit(std::string groupid, int taskid)
	{
		if (simulation) return true;
		if (groupid.empty() || taskid <= 0) return true;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(groupid.c_str(), groupid.length(), groupid);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[settasklastedit]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		//
		std::string str_update_disable = "", str_update_enable = "";
		char buff0[BUFF_SZ] = { 0 };
		snprintf(buff0, BUFF_SZ, "update sbt_doctask set islastedit = 0 where groupid = '%s';", groupid.c_str());//all close
		str_update_disable = buff0;

		char buff1[BUFF_SZ] = { 0 };
		snprintf(buff1, BUFF_SZ, "update sbt_doctask set islastedit = 1 where groupid = '%s' and taskid = %d;", groupid.c_str(), taskid);//open one task
		str_update_enable = buff1;

		_debug_to(loger_mysql, 0, ("[settasklastedit] sql: %s\n"), str_update_disable);
		mysql_query(&mysql, "SET NAMES UTF8");							//support chinese text
		if (!mysql_query(&mysql, str_update_disable.c_str()))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[settasklastedit] sql: %s\n"), str_update_enable);
			if (!mysql_query(&mysql, str_update_enable.c_str()))			//success return 0,failed return random number
			{
				_debug_to(loger_mysql, 0, ("[settasklastedit]open task %d, update lastedit success\n"), taskid);
			}
			else
			{
				ret = false;
				std::string error = mysql_error(&mysql);
				_debug_to(loger_mysql, 1, ("[settasklastedit]open task %d, update lastedit failed: %s\n"), taskid, error.c_str());
			}
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[settasklastedit]close all, update lastedit failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool isexisttask_taskid(int taskid)
	{
		if (simulation) return true;
		if (taskid <= 0) return false;

		bool ret = false;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[isexisttask]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select * from sbt_doctask where taskid = %d", taskid);
		_debug_to(loger_mysql, 0, ("[isexisttask] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[isexisttask] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1)//keep right
				ret = true;

			mysql_free_result(result);				//free result
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	int  gettasknextversion(std::string groupid)
	{
		if (simulation) return 0;
		if (groupid.empty()) return 0;

		bool ret = true; int maxversion = 0;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(groupid.c_str(), groupid.length(), groupid);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getmergeprogress]MySQL database connect failed: %s\n"), error.c_str());
			return 0;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select max(version) from sbt_doctask where groupid = '%s'", groupid.c_str());
		_debug_to(loger_mysql, 0, ("[getmergeprogress] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[getmergeprogress] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 1)//keep right
			{
				maxversion = atoi(row[0])+1;
			}
			else
			{
				ret = false;
				_debug_to(loger_mysql, 1, ("[getmergeprogress]groupid = %s, select mergetask count/colnum error\n"), groupid.c_str());
			}
			mysql_free_result(result);				//free result

			_debug_to(loger_mysql, 0, ("[getmergeprogress]groupid = %s, select mergetask progress success, progress=%d\n"), groupid.c_str(), maxversion);
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getmergeprogress]groupid = %s, select progress failed: %s\n"), groupid.c_str(), error.c_str());
		}

		//=====================
		mysql_close(&mysql);	//close connect
		if (!ret) return 0;

		return maxversion;
	}

	bool gettaskhistoryinfo(VEC_FILTERINFO& vecfilterinfo, std::string order_key , int order_way, int pagesize, int pagenum, int& total, VEC_TASKINFO& vectaskhistory, std::vector<int> vecbelongids)
	{
		if (simulation) return true;

		//where
		std::string str_where = " where 1 "; //为了统一使用where 1,故加条件关系只能为and
		int usedfilter_count = 0;
		for (VEC_FILTERINFO::iterator it = vecfilterinfo.begin(); it != vecfilterinfo.end(); ++it)
		{
			int filtertype = it->filtertype;
			std::string filterfield = it->filterfield;
			std::string filtervalue = it->filtervalue;
			if (!filterfield.empty() && !filtervalue.empty())
			{
				filterinfo usedfilter;
				usedfilter.filtertype = filtertype;
				usedfilter.filterfield = filterfield;
				usedfilter.filtervalue = filtervalue;
				usedfilter.to_utf8();

				std::string temp;
				char tempbuff[256] = { 0 };
				if (usedfilter.filtertype == 0)//单个值
				{
					snprintf(tempbuff, 256, " and cast(%s as char) like '%%%s%%' ", usedfilter.filterfield.c_str(), usedfilter.filtervalue.c_str());//CAST统一转换字段类型为字符
					temp = tempbuff;
				}
				if (usedfilter.filtertype == 1)//范围值
				{
					snprintf(tempbuff, 256, " and %s between %s ", usedfilter.filterfield.c_str(), usedfilter.filtervalue.c_str());//value值中有 and 关键字
					temp = tempbuff;
				}
				if (usedfilter.filtertype == 2)//枚举值
				{
					snprintf(tempbuff, 256, " and %s in (%s) ", usedfilter.filterfield.c_str(), usedfilter.filtervalue.c_str());
					temp = tempbuff;
				}
				if (usedfilter.filtertype == 3)//完整条件
				{
					snprintf(tempbuff, 256, " and (%s) ", usedfilter.filtervalue.c_str());
					temp = tempbuff;
				}

				str_where += temp;
				usedfilter_count += 1;
			}
		}
		_debug_to(loger_mysql, 0, ("[gettaskhistoryinfo]vecfilterinfo size = %d\n"), usedfilter_count);

		if (!vecbelongids.empty())//区分用户
		{
			str_where += " and belongid in (";
			for (size_t i = 0; i < vecbelongids.size(); i++)
			{
				std::string temp; char tempbuff[256] = { 0 };
				snprintf(tempbuff, 256, "%d", vecbelongids[i]);
				temp = tempbuff;
				str_where += temp;

				if (i == 9 || i == (vecbelongids.size() - 1))//最多枚举10个id
					break;
				str_where += ",";
			}
			str_where += ") ";
		}

		//只查询用户最后编辑的任务
		str_where += " and islastedit = 1 ";

		//public
		std::string str_where_public = "";// " or privilege = 0";//公共资源

		//order
		std::string str_order = "";
		if (!order_key.empty())
		{
			char temp[256] = { 0 };
			std::string orderway = (order_way == 0) ? ("asc") : ("desc");
			snprintf(temp, 256, " order by %s %s ", order_key.c_str(), orderway.c_str());
			str_order = temp;
		}

		//limit
		std::string str_limit = "";
		if (pagesize > 0 && pagenum > 0)
		{
			int throw_count = (pagenum-1)* pagesize;
			int need_count  = pagesize;

			char temp[256] = { 0 };
			snprintf(temp, 256, " limit %d,%d ", throw_count, need_count);
			str_limit = temp;
		}

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(order_key.c_str(), order_key.length(), order_key);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gettaskhistoryinfo]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		//a
		std::string str_sqltotal = "select count(taskid) from sbt_doctask";
		str_sqltotal += str_where;
		str_sqltotal += str_where_public;
		_debug_to(loger_mysql, 0, ("[gettaskhistoryinfo] sql: %s\n"), str_sqltotal.c_str());
		mysql_query(&mysql, "SET NAMES UTF8");			//support chinese text
		if (!mysql_query(&mysql, str_sqltotal.c_str()))	//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[gettaskhistoryinfo] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 1)//keep right
			{
				int row_total = atoi(row_value(row[0]).c_str());
				total = (row_total > 0) ? (row_total) : (0);
			}
			mysql_free_result(result);				//free result
			_debug_to(loger_mysql, 0, ("[gettaskhistoryinfo]select total success, total=%d\n"), total);
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gettaskhistoryinfo]mysql_query total failed: %s\n"), error.c_str());
		}

		//b
		std::string str_sqlselect = "select taskid,belongid,privilege,groupid,versionname,version,tasktype,moodtype,speakspeed,taskname,state,progress,createtime,scannedtime,finishtime,priority,islastedit,humanid,humanname,ssmltext,audiofile,audioformat,audiolength,videofile,keyframe,videoformat,videolength,videowidth,videoheight,videofps,foreground,front_left,front_right,front_top,front_bottom,background,backaudio,back_volume,back_loop,back_start,back_end,wnd_width,wnd_height  from sbt_doctask";
		str_sqlselect += str_where;
		str_sqlselect += str_where_public;
		str_sqlselect += str_order;
		str_sqlselect += str_limit;
		_debug_to(loger_mysql, 0, ("[gettaskhistoryinfo] sql: %s\n"), str_sqlselect.c_str());
		mysql_query(&mysql, "SET NAMES UTF8");				//support chinese text
		if (!mysql_query(&mysql, str_sqlselect.c_str()))	//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[gettaskhistoryinfo] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row;							//table row data
			while (row = mysql_fetch_row(result))
			{
				if (row && colnum >= 43)//keep right
				{
					int i = 0;
					int row_taskid = atoi(row_value(row[i++]).c_str());
					int row_belongid = atoi(row_value(row[i++]).c_str());
					int row_privilege = atoi(row_value(row[i++]).c_str());
					std::string row_groupid = row_value(row[i++]);
					std::string row_versionname = row_value(row[i++]);
					int row_version = atoi(row_value(row[i++]).c_str());
					int row_tasktype = atoi(row_value(row[i++]).c_str());
					int row_moodtype = atoi(row_value(row[i++]).c_str());
					double row_speakspeed = atof(row_value(row[i++]).c_str());
					std::string row_taskname = row_value(row[i++]);   
					int row_taskstate = atoi(row_value(row[i++]).c_str());
					int row_taskprogress = atoi(row_value(row[i++]).c_str());
					std::string row_createtime = row_value(row[i++]);
					std::string row_scannedtime = row_value(row[i++]);
					std::string row_finishtime = row_value(row[i++]);
					int row_priority = atoi(row_value(row[i++]).c_str());
					int row_islastedit = atoi(row_value(row[i++]).c_str());
					std::string row_humanid = row_value(row[i++]);
					std::string row_humanname = row_value(row[i++]);  
					std::string row_ssmltext = row_value(row[i++]);   row_ssmltext = jsonstr_replace(row_ssmltext);
					std::string row_audiofile = row_value(row[i++]);
					std::string row_audioformat = row_value(row[i++]);
					int			row_audiolength = atoi(row_value(row[i++]).c_str());
					std::string row_videofile = row_value(row[i++]);
					std::string row_videokeyframe = row_value(row[i++]);
					std::string row_videoformat = row_value(row[i++]);
					int			row_videolength = atoi(row_value(row[i++]).c_str());
					int			row_videowidth = atoi(row_value(row[i++]).c_str());
					int			row_videoheight = atoi(row_value(row[i++]).c_str());
					double		row_videofps = atof(row_value(row[i++]).c_str());
					std::string row_foreground = row_value(row[i++]).c_str();
					double		row_front_left = atof(row_value(row[i++]).c_str());
					double		row_front_right = atof(row_value(row[i++]).c_str());
					double		row_front_top = atof(row_value(row[i++]).c_str());
					double		row_front_bottom = atof(row_value(row[i++]).c_str());
					std::string row_background = row_value(row[i++]).c_str();
					std::string row_backaudio = row_value(row[i++]).c_str();
					double		row_back_volume = atof(row_value(row[i++]).c_str());
					int			row_back_loop = atoi(row_value(row[i++]).c_str());
					int			row_back_start = atoi(row_value(row[i++]).c_str());
					int			row_back_end = atoi(row_value(row[i++]).c_str());
					int			row_wnd_width = atoi(row_value(row[i++]).c_str());
					int			row_wnd_height = atoi(row_value(row[i++]).c_str());
					
					taskinfo historyitem;
					historyitem.taskid = row_taskid;
					historyitem.belongid = row_belongid;
					historyitem.privilege = row_privilege;
					historyitem.groupid = row_groupid;
					historyitem.versionname = row_versionname;
					historyitem.version = row_version;
					historyitem.tasktype = row_tasktype;
					historyitem.moodtype = row_moodtype;
					historyitem.speakspeed = row_speakspeed;
					historyitem.taskname = row_taskname;
					historyitem.taskstate = row_taskstate;
					historyitem.taskprogress = row_taskprogress;
					historyitem.createtime = row_createtime;
					historyitem.scannedtime = row_scannedtime;
					historyitem.finishtime = row_finishtime;
					historyitem.priority = row_priority;
					historyitem.islastedit = row_islastedit;
					historyitem.humanid = row_humanid;
					historyitem.humanname = row_humanname;
					historyitem.ssmltext = row_ssmltext;
					historyitem.audio_path = row_audiofile;
					historyitem.audio_format = row_audioformat;
					historyitem.audio_length = row_audiolength;
					historyitem.video_path = row_videofile;
					historyitem.video_keyframe = row_videokeyframe;
					historyitem.video_format = row_videoformat;
					historyitem.video_length = row_videolength;
					historyitem.video_width = row_videowidth;
					historyitem.video_height = row_videoheight;
					historyitem.video_fps = row_videofps;
					historyitem.foreground = row_foreground;
					historyitem.front_left = row_front_left;
					historyitem.front_right = row_front_right;
					historyitem.front_top = row_front_top;
					historyitem.front_bottom = row_front_bottom;
					historyitem.background = row_background;
					historyitem.backaudio = row_backaudio;
					historyitem.back_volume = row_back_volume;
					historyitem.back_loop = row_back_loop;
					historyitem.back_start = row_back_start;
					historyitem.back_end = row_back_end;
					historyitem.window_width = row_wnd_width;
					historyitem.window_height = row_wnd_height;

					vectaskhistory.push_back(historyitem);
				}
			}
			mysql_free_result(result);				//free result

			_debug_to(loger_mysql, 0, ("[gettaskhistoryinfo]select taskhistory success\n"));
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gettaskhistoryinfo]mysql_query taskhistory failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool gettaskgroupinfo(std::string groupid, VEC_TASKINFO& vectaskgroup)
	{
		if (simulation) return true;
		if (groupid.empty()) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(groupid.c_str(), groupid.length(), groupid);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gettaskgroupinfo]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		//where
		std::string str_where = "";
		char temp[256] = { 0 };
		snprintf(temp, 256, " where groupid = '%s'", groupid.c_str());
		str_where = temp;

		//order
		std::string str_order = " order by version asc";

		std::string str_sqlselect = "select taskid,belongid,privilege,groupid,versionname,version,tasktype,moodtype,speakspeed,taskname,state,progress,createtime,scannedtime,finishtime,priority,islastedit,humanid,humanname,ssmltext,audiofile,audioformat,audiolength,videofile,keyframe,videoformat,videolength,videowidth,videoheight,videofps,foreground,front_left,front_right,front_top,front_bottom,background,backaudio,back_volume,back_loop,back_start,back_end,wnd_width,wnd_height  from sbt_doctask";
		str_sqlselect += str_where;
		str_sqlselect += str_order;

		_debug_to(loger_mysql, 0, ("[gettaskgroupinfo] sql: %s\n"), str_sqlselect.c_str());
		mysql_query(&mysql, "SET NAMES UTF8");				//support chinese text
		if (!mysql_query(&mysql, str_sqlselect.c_str()))	//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[gettaskgroupinfo] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row;							//table row data
			while (row = mysql_fetch_row(result))
			{
				if (row && colnum >= 43)//keep right
				{
					int i = 0;
					int row_taskid = atoi(row_value(row[i++]).c_str());
					int row_belongid = atoi(row_value(row[i++]).c_str());
					int row_privilege = atoi(row_value(row[i++]).c_str());
					std::string row_groupid = row_value(row[i++]);
					std::string row_versionname = row_value(row[i++]);
					int row_version = atoi(row_value(row[i++]).c_str());
					int row_tasktype = atoi(row_value(row[i++]).c_str());
					int row_moodtype = atoi(row_value(row[i++]).c_str());
					double row_speakspeed = atof(row_value(row[i++]).c_str());
					std::string row_taskname = row_value(row[i++]);
					int row_taskstate = atoi(row_value(row[i++]).c_str());
					int row_taskprogress = atoi(row_value(row[i++]).c_str());
					std::string row_createtime = row_value(row[i++]);
					std::string row_scannedtime = row_value(row[i++]);
					std::string row_finishtime = row_value(row[i++]);
					int row_priority = atoi(row_value(row[i++]).c_str());
					int row_islastedit = atoi(row_value(row[i++]).c_str());
					std::string row_humanid = row_value(row[i++]);
					std::string row_humanname = row_value(row[i++]);
					std::string row_ssmltext = row_value(row[i++]);   row_ssmltext = jsonstr_replace(row_ssmltext);
					std::string row_audiofile = row_value(row[i++]);
					std::string row_audioformat = row_value(row[i++]);
					int			row_audiolength = atoi(row_value(row[i++]).c_str());
					std::string row_videofile = row_value(row[i++]);
					std::string row_videokeyframe = row_value(row[i++]);
					std::string row_videoformat = row_value(row[i++]);
					int			row_videolength = atoi(row_value(row[i++]).c_str());
					int			row_videowidth = atoi(row_value(row[i++]).c_str());
					int			row_videoheight = atoi(row_value(row[i++]).c_str());
					double		row_videofps = atof(row_value(row[i++]).c_str());
					std::string row_foreground = row_value(row[i++]).c_str();
					double		row_front_left = atof(row_value(row[i++]).c_str());
					double		row_front_right = atof(row_value(row[i++]).c_str());
					double		row_front_top = atof(row_value(row[i++]).c_str());
					double		row_front_bottom = atof(row_value(row[i++]).c_str());
					std::string row_background = row_value(row[i++]).c_str();
					std::string row_backaudio = row_value(row[i++]).c_str();
					double		row_back_volume = atof(row_value(row[i++]).c_str());
					int			row_back_loop = atoi(row_value(row[i++]).c_str());
					int			row_back_start = atoi(row_value(row[i++]).c_str());
					int			row_back_end = atoi(row_value(row[i++]).c_str());
					int			row_wnd_width = atoi(row_value(row[i++]).c_str());
					int			row_wnd_height = atoi(row_value(row[i++]).c_str());

					taskinfo historyitem;
					historyitem.taskid = row_taskid;
					historyitem.belongid = row_belongid;
					historyitem.privilege = row_privilege;
					historyitem.groupid = row_groupid;
					historyitem.versionname = row_versionname;
					historyitem.version = row_version;
					historyitem.tasktype = row_tasktype;
					historyitem.moodtype = row_moodtype;
					historyitem.speakspeed = row_speakspeed;
					historyitem.taskname = row_taskname;
					historyitem.taskstate = row_taskstate;
					historyitem.taskprogress = row_taskprogress;
					historyitem.createtime = row_createtime;
					historyitem.scannedtime = row_scannedtime;
					historyitem.finishtime = row_finishtime;
					historyitem.priority = row_priority;
					historyitem.islastedit = row_islastedit;
					historyitem.humanid = row_humanid;
					historyitem.humanname = row_humanname;
					historyitem.ssmltext = row_ssmltext;
					historyitem.audio_path = row_audiofile;
					historyitem.audio_format = row_audioformat;
					historyitem.audio_length = row_audiolength;
					historyitem.video_path = row_videofile;
					historyitem.video_keyframe = row_videokeyframe;
					historyitem.video_format = row_videoformat;
					historyitem.video_length = row_videolength;
					historyitem.video_width = row_videowidth;
					historyitem.video_height = row_videoheight;
					historyitem.video_fps = row_videofps;
					historyitem.foreground = row_foreground;
					historyitem.front_left = row_front_left;
					historyitem.front_right = row_front_right;
					historyitem.front_top = row_front_top;
					historyitem.front_bottom = row_front_bottom;
					historyitem.background = row_background;
					historyitem.backaudio = row_backaudio;
					historyitem.back_volume = row_back_volume;
					historyitem.back_loop = row_back_loop;
					historyitem.back_start = row_back_start;
					historyitem.back_end = row_back_end;
					historyitem.window_width = row_wnd_width;
					historyitem.window_height = row_wnd_height;

					vectaskgroup.push_back(historyitem);
				}
			}
			mysql_free_result(result);				//free result
			_debug_to(loger_mysql, 0, ("[gettaskgroupinfo]select taskgroupinfo[groupid = %s] success\n"), groupid.c_str());
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gettaskgroupinfo]mysql_query taskgroupinfo[groupid = %s] failed: %s\n"), groupid.c_str(), error.c_str());
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;

	}
	bool gettasklistinfo(VEC_FILTERINFO& vecfilterinfo, int pagesize, int pagenum, int& total, VEC_TASKINFO& vectasklist)
	{
		if (simulation) return true;

		//where
		std::string str_where = " where 1 "; //为了统一使用where 1,故加条件关系只能为and
		int usedfilter_count = 0;
		for (VEC_FILTERINFO::iterator it = vecfilterinfo.begin(); it != vecfilterinfo.end(); ++it)
		{
			int filtertype = it->filtertype;
			std::string filterfield = it->filterfield;
			std::string filtervalue = it->filtervalue;
			if (!filterfield.empty() && !filtervalue.empty())
			{
				filterinfo usedfilter;
				usedfilter.filtertype = filtertype;
				usedfilter.filterfield = filterfield;
				usedfilter.filtervalue = filtervalue;
				usedfilter.to_utf8();

				std::string temp;
				char tempbuff[256] = { 0 };
				if (usedfilter.filtertype == 0)//单个值
				{
					snprintf(tempbuff, 256, " and cast(%s as char) like '%%%s%%' ", usedfilter.filterfield.c_str(), usedfilter.filtervalue.c_str());//CAST统一转换字段类型为字符
					temp = tempbuff;
				}
				if (usedfilter.filtertype == 1)//范围值
				{
					snprintf(tempbuff, 256, " and %s between %s ", usedfilter.filterfield.c_str(), usedfilter.filtervalue.c_str());//value值中有 and 关键字
					temp = tempbuff;
				}
				if (usedfilter.filtertype == 2)//枚举值
				{
					snprintf(tempbuff, 256, " and %s in (%s) ", usedfilter.filterfield.c_str(), usedfilter.filtervalue.c_str());
					temp = tempbuff;
				}
				if (usedfilter.filtertype == 3)//完整条件
				{
					snprintf(tempbuff, 256, " and (%s) ", usedfilter.filtervalue.c_str());
					temp = tempbuff;
				}

				str_where += temp;
				usedfilter_count += 1;
			}
		}
		_debug_to(loger_mysql, 0, ("[gettasklistinfo]vecfilterinfo size = %d\n"), usedfilter_count);

		//order
		std::string str_order = " order by createtime desc ";

		//limit
		std::string str_limit = "";
		if (pagesize > 0 && pagenum > 0)
		{
			int throw_count = (pagenum - 1) * pagesize;
			int need_count = pagesize;

			char temp[256] = { 0 };
			snprintf(temp, 256, " limit %d,%d ", throw_count, need_count);
			str_limit = temp;
		}

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gettasklistinfo]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		//a
		std::string str_sqltotal = "select count(taskid) from sbt_doctask left join sbt_userinfo on sbt_doctask.belongid = sbt_userinfo.id";
		str_sqltotal += str_where;
		_debug_to(loger_mysql, 0, ("[gettasklistinfo] sql: %s\n"), str_sqltotal.c_str());
		mysql_query(&mysql, "SET NAMES UTF8");			//support chinese text
		if (!mysql_query(&mysql, str_sqltotal.c_str()))	//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[gettasklistinfo] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 1)//keep right
			{
				int row_total = atoi(row_value(row[0]).c_str());
				total = (row_total > 0) ? (row_total) : (0);
			}
			mysql_free_result(result);				//free result
			_debug_to(loger_mysql, 0, ("[gettasklistinfo]select total success, total=%d\n"), total);
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gettasklistinfo]mysql_query total failed: %s\n"), error.c_str());
		}

		//b
		std::string str_sqlselect = "select taskid,belongid,privilege,groupid,versionname,version,tasktype,moodtype,speakspeed,taskname,state,progress,createtime,scannedtime,finishtime,priority,islastedit,humanid,humanname,ssmltext,audiofile,audioformat,audiolength,videofile,keyframe,videoformat,videolength,videowidth,videoheight,videofps,foreground,front_left,front_right,front_top,front_bottom,background,backaudio,back_volume,back_loop,back_start,back_end,wnd_width,wnd_height from sbt_doctask left join sbt_userinfo on sbt_doctask.belongid = sbt_userinfo.id";
		str_sqlselect += str_where;
		str_sqlselect += str_order;
		str_sqlselect += str_limit;
		_debug_to(loger_mysql, 0, ("[gettasklistinfo] sql: %s\n"), str_sqlselect.c_str());
		mysql_query(&mysql, "SET NAMES UTF8");				//support chinese text
		if (!mysql_query(&mysql, str_sqlselect.c_str()))	//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[gettasklistinfo] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row;							//table row data
			while (row = mysql_fetch_row(result))
			{
				if (row && colnum >= 43)//keep right
				{
					int i = 0;
					int row_taskid = atoi(row_value(row[i++]).c_str());
					int row_belongid = atoi(row_value(row[i++]).c_str());
					int row_privilege = atoi(row_value(row[i++]).c_str());
					std::string row_groupid = row_value(row[i++]);
					std::string row_versionname = row_value(row[i++]);
					int row_version = atoi(row_value(row[i++]).c_str());
					int row_tasktype = atoi(row_value(row[i++]).c_str());
					int row_moodtype = atoi(row_value(row[i++]).c_str());
					double row_speakspeed = atof(row_value(row[i++]).c_str());
					std::string row_taskname = row_value(row[i++]);
					int row_taskstate = atoi(row_value(row[i++]).c_str());
					int row_taskprogress = atoi(row_value(row[i++]).c_str());
					std::string row_createtime = row_value(row[i++]);
					std::string row_scannedtime = row_value(row[i++]);
					std::string row_finishtime = row_value(row[i++]);
					int row_priority = atoi(row_value(row[i++]).c_str());
					int row_islastedit = atoi(row_value(row[i++]).c_str());
					std::string row_humanid = row_value(row[i++]);
					std::string row_humanname = row_value(row[i++]);
					std::string row_ssmltext = row_value(row[i++]);   row_ssmltext = jsonstr_replace(row_ssmltext);
					std::string row_audiofile = row_value(row[i++]);
					std::string row_audioformat = row_value(row[i++]);
					int			row_audiolength = atoi(row_value(row[i++]).c_str());
					std::string row_videofile = row_value(row[i++]);
					std::string row_videokeyframe = row_value(row[i++]);
					std::string row_videoformat = row_value(row[i++]);
					int			row_videolength = atoi(row_value(row[i++]).c_str());
					int			row_videowidth = atoi(row_value(row[i++]).c_str());
					int			row_videoheight = atoi(row_value(row[i++]).c_str());
					double		row_videofps = atof(row_value(row[i++]).c_str());
					std::string row_foreground = row_value(row[i++]).c_str();
					double		row_front_left = atof(row_value(row[i++]).c_str());
					double		row_front_right = atof(row_value(row[i++]).c_str());
					double		row_front_top = atof(row_value(row[i++]).c_str());
					double		row_front_bottom = atof(row_value(row[i++]).c_str());
					std::string row_background = row_value(row[i++]).c_str();
					std::string row_backaudio = row_value(row[i++]).c_str();
					double		row_back_volume = atof(row_value(row[i++]).c_str());
					int			row_back_loop = atoi(row_value(row[i++]).c_str());
					int			row_back_start = atoi(row_value(row[i++]).c_str());
					int			row_back_end = atoi(row_value(row[i++]).c_str());
					int			row_wnd_width = atoi(row_value(row[i++]).c_str());
					int			row_wnd_height = atoi(row_value(row[i++]).c_str());

					taskinfo taskitem;
					taskitem.taskid = row_taskid;
					taskitem.belongid = row_belongid;
					taskitem.privilege = row_privilege;
					taskitem.groupid = row_groupid;
					taskitem.versionname = row_versionname;
					taskitem.version = row_version;
					taskitem.tasktype = row_tasktype;
					taskitem.moodtype = row_moodtype;
					taskitem.speakspeed = row_speakspeed;
					taskitem.taskname = row_taskname;
					taskitem.taskstate = row_taskstate;
					taskitem.taskprogress = row_taskprogress;
					taskitem.createtime = row_createtime;
					taskitem.scannedtime = row_scannedtime;
					taskitem.finishtime = row_finishtime;
					taskitem.priority = row_priority;
					taskitem.islastedit = row_islastedit;
					taskitem.humanid = row_humanid;
					taskitem.humanname = row_humanname;
					taskitem.ssmltext = row_ssmltext;
					taskitem.audio_path = row_audiofile;
					taskitem.audio_format = row_audioformat;
					taskitem.audio_length = row_audiolength;
					taskitem.video_path = row_videofile;
					taskitem.video_keyframe = row_videokeyframe;
					taskitem.video_format = row_videoformat;
					taskitem.video_length = row_videolength;
					taskitem.video_width = row_videowidth;
					taskitem.video_height = row_videoheight;
					taskitem.video_fps = row_videofps;
					taskitem.foreground = row_foreground;
					taskitem.front_left = row_front_left;
					taskitem.front_right = row_front_right;
					taskitem.front_top = row_front_top;
					taskitem.front_bottom = row_front_bottom;
					taskitem.background = row_background;
					taskitem.backaudio = row_backaudio;
					taskitem.back_volume = row_back_volume;
					taskitem.back_loop = row_back_loop;
					taskitem.back_start = row_back_start;
					taskitem.back_end = row_back_end;
					taskitem.window_width = row_wnd_width;
					taskitem.window_height = row_wnd_height;

					vectasklist.push_back(taskitem);
				}
			}
			mysql_free_result(result);				//free result

			_debug_to(loger_mysql, 0, ("[gettasklistinfo]select tasklist success\n"));
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gettasklistinfo]mysql_query tasklist failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool deletetask_taskid(int taskid, bool deletefile, std::string& errmsg)
	{
		if (simulation) return true;
		if (taskid <= 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql); errmsg = error;
			_debug_to(loger_mysql, 1, ("[deletetask_taskid]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		if (deletefile)
		{
			snprintf(sql_buff, BUFF_SZ, "select audiofile,videofile,keyframe from sbt_doctask where taskid = %d", taskid);
			_debug_to(loger_mysql, 0, ("[deletetask_taskid] sql: %s\n"), sql_buff);
			mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
			if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
			{
				MYSQL_RES* result;						//table data struct
				result = mysql_store_result(&mysql);    //sava dada to result
				int rownum = mysql_num_rows(result);	//get row number
				int colnum = mysql_num_fields(result);  //get col number
				_debug_to(loger_mysql, 0, ("[deletetask_taskid] rownum=%d,colnum=%d\n"), rownum, colnum);

				MYSQL_ROW row = mysql_fetch_row(result);//table row data
				if (row && rownum >= 1)//keep right
				{
					int i = 0;
					while (colnum--)
					{
						std::string physicalfilepath = row_value(row[i++]);
						if (is_existfile(physicalfilepath.c_str()))
							remove(physicalfilepath.c_str());
						_debug_to(loger_mysql, 0, ("[deletetask_taskid] delete physical file... [%s]\n"), physicalfilepath);
					}
				}
				mysql_free_result(result);				//free result
			}
			else
			{
				ret = false;
				std::string error = mysql_error(&mysql);
				_debug_to(loger_mysql, 1, ("[deletetask_taskid]mysql_query filepath before delete task failed: %s\n"), error.c_str());
			}
		}

		snprintf(sql_buff, BUFF_SZ, "delete from sbt_doctask where taskid = %d", taskid);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		_debug_to(loger_mysql, 0, ("[deletetask_taskid] sql: %s\n"), sql_buff);
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[deletetask_taskid]delete task by taskid success, taskid=%d\n"), taskid);
			errmsg = "delete task by taskid success";
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql); errmsg = error;
			_debug_to(loger_mysql, 1, ("[deletetask_taskid]delete task by taskid failed, taskid=%d\n"), taskid);
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}

	//背景资源
	bool addtasksource(tasksourceinfo tasksourceitem, bool update)
	{
		if (simulation) return true;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		tasksourceitem.to_utf8();

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[addtasksource]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		//
		std::string OPERATION = "";
		char sql_buff[BUFF_SZ] = { 0 };
		if (update)
		{
			//update
			OPERATION = "UPDATE";
			snprintf(sql_buff, BUFF_SZ, "update sbt_doctasksource set sourcetype=%d,sourcekeyframe='%s',sourcewidth=%d,sourceheight=%d,createtime='%s'  where sourcepath='%s'",
				tasksourceitem.sourcetype, tasksourceitem.sourcekeyframe.c_str(), tasksourceitem.sourcewidth, tasksourceitem.sourceheight, tasksourceitem.createtime.c_str(),
				tasksourceitem.sourcepath.c_str());
		}
		else
		{
			OPERATION = "INSERT";
			int next_id = newgetsequencenextvalue("sbt_doctasksource", &mysql);
			if (next_id <= 0) return false;

			//insert
			snprintf(sql_buff, BUFF_SZ, "insert into sbt_doctasksource (id,belongid,privilege,sourcetype,sourcepath,sourcekeyframe,sourcewidth,sourceheight,createtime) values(%d, %d, %d, %d, '%s', '%s', %d, %d, '%s')",
				next_id, tasksourceitem.belongid, tasksourceitem.privilege,
				tasksourceitem.sourcetype, tasksourceitem.sourcepath.c_str(), tasksourceitem.sourcekeyframe.c_str(), tasksourceitem.sourcewidth, tasksourceitem.sourceheight, tasksourceitem.createtime.c_str());
		}
		
		//run sql
		_debug_to(loger_mysql, 0, ("[addtasksource] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[addtasksource]%s source[%s] success\n"), OPERATION.c_str(), tasksourceitem.sourcepath.c_str());
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[addtasksource]%s source[%s] failed: %s\n"), OPERATION.c_str(), tasksourceitem.sourcepath.c_str(), error.c_str());
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool gettasksource(int id, tasksourceinfo& tasksourceitem)
	{
		if (simulation) return true;
		if (id <= 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gettasksource]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select id,belongid,privilege,sourcetype,sourcepath,sourcekeyframe,sourcewidth,sourceheight,createtime from sbt_doctasksource where id=%d", id);//select	
		_debug_to(loger_mysql, 0, ("[gettasksource] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[gettasksource] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && colnum >= 9) //keep right
			{
				int i = 0;
				int row_id = atoi(row_value(row[i++]).c_str());
				int row_belongid = atoi(row_value(row[i++]).c_str());
				int row_privilege = atoi(row_value(row[i++]).c_str());
				int row_sourcetype = atoi(row_value(row[i++]).c_str());
				std::string row_sourcepath = row_value(row[i++]); 
				std::string row_sourcekeyframe = row_value(row[i++]); 
				int row_sourcewidth = atoi(row_value(row[i++]).c_str());
				int row_sourceheight = atoi(row_value(row[i++]).c_str());
				std::string row_createtime = row_value(row[i++]);

				tasksourceitem.id = row_id;
				tasksourceitem.belongid = row_belongid;
				tasksourceitem.privilege = row_privilege;
				tasksourceitem.sourcetype = row_sourcetype;
				tasksourceitem.sourcepath = row_sourcepath;
				tasksourceitem.sourcekeyframe = row_sourcekeyframe;
				tasksourceitem.sourcewidth = row_sourcewidth;
				tasksourceitem.sourceheight = row_sourceheight;
				tasksourceitem.createtime = row_createtime;
			}
			else
			{
				ret = false;
				_debug_to(loger_mysql, 1, ("[gettasksource] select tasksource count/colnum error\n"));
			}
			mysql_free_result(result);				//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gettasksource]mysql_query tasksource failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool isexisttasksource_path(std::string sourcepath)
	{
		if (simulation) return true;
		if (sourcepath.empty()) return false;

		bool ret = false;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(sourcepath.c_str(), sourcepath.length(), sourcepath);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[isexisttasksource_path]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select * from sbt_doctasksource where sourcepath = '%s'", sourcepath.c_str());
		_debug_to(loger_mysql, 0, ("[isexisttasksource_path] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[isexisttasksource_path] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1)//keep right
				ret = true;

			mysql_free_result(result);				//free result
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}

	bool gettasksourcelist(std::vector<int> vecbelongids, VEC_TASKSOURCEINFO& vectasksource)
	{
		if (simulation) return true;
		if (vecbelongids.empty()) return false;

		//where
		std::string str_where = " where 1 "; //为了统一使用where 1,故加条件关系只能为and
		if (!vecbelongids.empty())//区分用户
		{
			str_where += " and belongid in (";
			for (size_t i = 0; i < vecbelongids.size(); i++)
			{
				std::string temp; char tempbuff[256] = { 0 };
				snprintf(tempbuff, 256, "%d", vecbelongids[i]);
				temp = tempbuff;
				str_where += temp;

				if (i == 9 || i == (vecbelongids.size() - 1))//最多枚举10个id
					break;
				str_where += ",";
			}
			str_where += ") ";
		}

		//public
		std::string str_where_public = "";// " or privilege = 0";//公共资源

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gettasksourcelist]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		std::string str_sql = "select id,belongid,privilege,sourcetype,sourcepath,sourcekeyframe,sourcewidth,sourceheight,createtime from sbt_doctasksource";//select
		str_sql += str_where;
		str_sql += str_where_public;
		_debug_to(loger_mysql, 0, ("[gettasksourcelist] sql: %s\n"), str_sql.c_str());

		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, str_sql.c_str()))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[gettasksourcelist] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row;							//table row data
			while (row = mysql_fetch_row(result))
			{
				if (row && colnum >= 9) //keep right
				{
					int i = 0;
					int row_id = atoi(row_value(row[i++]).c_str());
					int row_belongid = atoi(row_value(row[i++]).c_str());
					int row_privilege = atoi(row_value(row[i++]).c_str());
					int row_sourcetype = atoi(row_value(row[i++]).c_str());
					std::string row_sourcepath = row_value(row[i++]); 
					std::string row_sourcekeyframe = row_value(row[i++]); 
					int row_sourcewidth = atoi(row_value(row[i++]).c_str());
					int row_sourceheight = atoi(row_value(row[i++]).c_str());
					std::string row_createtime = row_value(row[i++]);

					tasksourceinfo tasksourceitem;
					tasksourceitem.id = row_id;
					tasksourceitem.belongid = row_belongid;
					tasksourceitem.privilege = row_privilege;
					tasksourceitem.sourcetype = row_sourcetype;
					tasksourceitem.sourcepath = row_sourcepath;
					tasksourceitem.sourcekeyframe = row_sourcekeyframe;
					tasksourceitem.sourcewidth = row_sourcewidth;
					tasksourceitem.sourceheight = row_sourceheight;
					tasksourceitem.createtime = row_createtime;
					vectasksource.push_back(tasksourceitem);
				}
			}
			mysql_free_result(result);				//free result

			_debug_to(loger_mysql, 0, ("[gettasksourcelist]select tasksourcelist success\n"));
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[gettasksourcelist]mysql_query tasksourcelist failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool deletetasksource_id(int id, bool deletefile, std::string& errmsg)
	{
		if (simulation) return true;
		if (id <= 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql); errmsg = error;
			_debug_to(loger_mysql, 1, ("[deletetasksource_id]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		if (deletefile)
		{
			snprintf(sql_buff, BUFF_SZ, "select sourcepath,sourcekeyframe from sbt_doctasksource where id = %d", id);
			_debug_to(loger_mysql, 0, ("[deletetasksource_id] sql: %s\n"), sql_buff);
			mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
			if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
			{
				MYSQL_RES* result;						//table data struct
				result = mysql_store_result(&mysql);    //sava dada to result
				int rownum = mysql_num_rows(result);	//get row number
				int colnum = mysql_num_fields(result);  //get col number
				_debug_to(loger_mysql, 0, ("[deletetasksource_id] rownum=%d,colnum=%d\n"), rownum, colnum);

				MYSQL_ROW row = mysql_fetch_row(result);//table row data
				if (row && rownum >= 1)//keep right
				{
					int i = 0;
					while (colnum--)
					{
						std::string physicalfilepath = row_value(row[i++]);
						if (is_existfile(physicalfilepath.c_str()))
							remove(physicalfilepath.c_str());
						_debug_to(loger_mysql, 0, ("[deletetasksource_id] delete physical file... [%s]\n"), physicalfilepath);
					}
				}
				mysql_free_result(result);				//free result
			}
			else
			{
				ret = false;
				std::string error = mysql_error(&mysql);
				_debug_to(loger_mysql, 1, ("[deletetasksource_id]mysql_query filepath before delete task failed: %s\n"), error.c_str());
			}
		}

		snprintf(sql_buff, BUFF_SZ, "delete from sbt_doctasksource where id = %d", id);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		_debug_to(loger_mysql, 0, ("[deletetasksource_id] sql: %s\n"), sql_buff);
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[deletetasksource_id]delete tasksource by id success, id=%d\n"), id);
			errmsg = "delete tasksource by id success";
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql); errmsg = error;
			_debug_to(loger_mysql, 1, ("[deletetasksource_id]delete tasksource by id failed, id=%d\n"), id);
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}

	//动作资源
	bool addhumanaction(humanactioninfo humanactionitem)
	{
		if (simulation) return true;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		humanactionitem.to_utf8();

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[addhumanaction]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		int next_id = newgetsequencenextvalue("sbt_humanaction", &mysql);
		if (next_id <= 0) return false;

		//insert
		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "insert into sbt_humanaction (id,humanid,actionname,actionkeyframe,actionvideo,actionduration,videowidth,videoheight) values(%d, '%s', '%s', '%s', '%s', %d, %d, %d)",
			next_id, humanactionitem.humanid.c_str(), humanactionitem.actionname.c_str(), humanactionitem.actionkeyframe.c_str(), humanactionitem.actionvideo.c_str(), humanactionitem.actionduration, humanactionitem.videowidth, humanactionitem.videoheight);
		_debug_to(loger_mysql, 0, ("[addhumanaction] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[addhumanaction]id = %d, insert success\n"), next_id);
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[addhumanaction]id = %d, insert failed: %s\n"), next_id, error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	bool getactionlist(std::string humanid, VEC_HUMANACTIONINFO& vechumanaction)
	{
		if (simulation) return true;
		if (humanid.empty()) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		ansi_to_utf8(humanid.c_str(), humanid.length(), humanid);

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getactionlist]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select id,humanid,actionname,actionkeyframe,actionvideo,actionduration,videowidth,videoheight from sbt_humanaction where humanid='%s'", humanid.c_str());

		//run sql
		_debug_to(loger_mysql, 0, ("[addhumanaction] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[getactionlist] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row;							//table row data
			while (row = mysql_fetch_row(result))
			{
				if (row && colnum >= 8) //keep right
				{
					int i = 0;
					int row_id = atoi(row_value(row[i++]).c_str());
					std::string row_humanid = row_value(row[i++]);
					std::string row_actionname = row_value(row[i++]);
					std::string row_actionkeyframe = row_value(row[i++]);
					std::string row_actionvideo = row_value(row[i++]);
					int row_videoduration = atoi(row_value(row[i++]).c_str());
					int row_videowidth = atoi(row_value(row[i++]).c_str());
					int row_videoheight = atoi(row_value(row[i++]).c_str());

					humanactioninfo humanactionitem;
					humanactionitem.id = row_id;
					humanactionitem.humanid = row_humanid;
					humanactionitem.actionname = row_actionname;
					humanactionitem.actionkeyframe = row_actionkeyframe;
					humanactionitem.actionvideo = row_actionvideo;
					humanactionitem.actionduration = row_videoduration;
					humanactionitem.videowidth = row_videowidth;
					humanactionitem.videoheight = row_videoheight;
					vechumanaction.push_back(humanactionitem);
				}
			}
			mysql_free_result(result);				//free result

			_debug_to(loger_mysql, 0, ("[getactionlist]select actionlist success\n"));
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getactionlist]mysql_query actionlist failed: %s\n"), error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}

	//原始视频
	bool addoriginalvdo(originalvdoinfo originalvdoitem)
	{
		if (simulation) return true;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//string covert to utf8
		originalvdoitem.to_utf8();

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[addoriginalvdo]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		int next_id = newgetsequencenextvalue("sbt_humanvideo", &mysql);
		if (next_id <= 0) return false;

		//insert 
		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "insert into sbt_humanvideo (id,humanid,videofile,moodtype,duration,fromtype) values(%d, '%s', '%s', %d, %d, %d)",
			next_id, originalvdoitem.humanid.c_str(), originalvdoitem.videofile.c_str(), originalvdoitem.moodtype, originalvdoitem.duration, originalvdoitem.fromtype);
		_debug_to(loger_mysql, 0, ("[addoriginalvdo] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			_debug_to(loger_mysql, 0, ("[addoriginalvdo]id = %d, insert success\n"), next_id);
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[addoriginalvdo]id = %d, insert failed: %s\n"), next_id, error.c_str());
		}
		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}

	//任务进度
	int  getmergeprogress(int taskid)
	{
		if (simulation) return 0;
		if (taskid <= 0) return 0;

		bool ret = true; int nprogress = 0;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getmergeprogress]MySQL database connect failed: %s\n"), error.c_str());
			return 0;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select sbt_doctask.progress from sbt_doctask where taskid = %d", taskid);
		_debug_to(loger_mysql, 0, ("[getmergeprogress] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[getmergeprogress] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 1)//keep right
			{
				nprogress = atoi(row[0]);
			}
			else
			{
				ret = false;
				_debug_to(loger_mysql, 1, ("[getmergeprogress]task %d, select mergetask count/colnum error\n"), taskid);
			}
			mysql_free_result(result);				//free result

			_debug_to(loger_mysql, 0, ("[getmergeprogress]task %d, select mergetask progress success, progress=%d\n"), taskid, nprogress);
		}
		else 
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getmergeprogress]task %d, select progress failed: %s\n"), taskid, error.c_str());
		}
		
		//=====================
		mysql_close(&mysql);	//close connect
		if (!ret) return -1;

		return nprogress;
	}
	bool setmergeprogress(int taskid, int nprogress)
	{
		if (simulation) return true;
		if (taskid <= 0 || nprogress < 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setmergeprogress]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select sbt_doctask.progress from sbt_doctask where taskid = %d", taskid);
		_debug_to(loger_mysql, 0, ("[setmergeprogress] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");			//support chinese text
		if (!mysql_query(&mysql, sql_buff))				//success return 0,failed return random number
		{
			MYSQL_RES* result;							//table data struct
			result = mysql_store_result(&mysql);		//sava dada to result

			snprintf(sql_buff, BUFF_SZ, "update sbt_doctask set progress=%d where taskid = %d", nprogress, taskid);	//update
			_debug_to(loger_mysql, 0, ("[setmergeprogress] sql: %s\n"), sql_buff);
			mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
			if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
			{
				_debug_to(loger_mysql, 0, ("[setmergeprogress]task %d, update progress success, progress=%d\n"), taskid, nprogress);
			}
			else
			{
				ret = false;
				std::string error = mysql_error(&mysql);
				_debug_to(loger_mysql, 1, ("[setmergeprogress]task %d, update progress failed: %s\n"), taskid, error.c_str());
			}
			mysql_free_result(result);					//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setmergeprogress]task %d, select progress failed: %s\n"), taskid, error.c_str());
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}
	//-1=waitmerge,0=merging,1=mergesuccess,2=mergefailed,3=movefailed
	int  getmergestate(int taskid)
	{
		if (simulation) return 0;
		if (taskid <= 0) return -1;

		bool ret = true; int nstate = 0;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getmergestate]MySQL database connect failed: %s\n"), error.c_str());
			return 0;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select sbt_doctask.state from sbt_doctask where taskid = %d", taskid);
		_debug_to(loger_mysql, 0, ("[getmergestate] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
		if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
		{
			MYSQL_RES* result;						//table data struct
			result = mysql_store_result(&mysql);    //sava dada to result
			int rownum = mysql_num_rows(result);	//get row number
			int colnum = mysql_num_fields(result);  //get col number
			_debug_to(loger_mysql, 0, ("[getmergestate] rownum=%d,colnum=%d\n"), rownum, colnum);

			MYSQL_ROW row = mysql_fetch_row(result);//table row data
			if (row && rownum >= 1 && colnum >= 1)//keep right
			{
				nstate = atoi(row[0]);
			}
			mysql_free_result(result);				//free result

			_debug_to(loger_mysql, 0, ("[getmergestate]task %d, select mergetask state success, state=%d\n"), taskid, nstate);
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[getmergestate]task %d, select mergetask state failed: %s\n"), taskid, error.c_str());
		}

		//=====================
		mysql_close(&mysql);	//close connect
		if (!ret) return -1;

		return nstate;
	}
	bool setmergestate(int taskid, int nstate)
	{
		if (simulation) return true;
		if (taskid <= 0) return false;

		bool ret = true;
		MYSQL mysql;
		mysql_init(&mysql);		//init MYSQL

		//=====================
		if (!mysql_real_connect(&mysql, g_database_ip.c_str(), g_database_username.c_str(), g_database_password.c_str(), g_database_dbname.c_str(), g_database_port, NULL, 0)) //connect mysql
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setmergestate]MySQL database connect failed: %s\n"), error.c_str());
			return false;
		}

		char sql_buff[BUFF_SZ] = { 0 };
		snprintf(sql_buff, BUFF_SZ, "select sbt_doctask.state from sbt_doctask where taskid = %d", taskid);
		_debug_to(loger_mysql, 0, ("[setmergestate] sql: %s\n"), sql_buff);
		mysql_query(&mysql, "SET NAMES UTF8");			//support chinese text
		if (!mysql_query(&mysql, sql_buff))				//success return 0,failed return random number
		{
			MYSQL_RES* result;							//table data struct
			result = mysql_store_result(&mysql);		//sava dada to result

			snprintf(sql_buff, BUFF_SZ, "update sbt_doctask set state=%d where taskid = %d", nstate, taskid);	//update
			_debug_to(loger_mysql, 0, ("[setmergestate] sql: %s\n"), sql_buff);
			mysql_query(&mysql, "SET NAMES UTF8");		//support chinese text
			if (!mysql_query(&mysql, sql_buff))			//success return 0,failed return random number
			{
				_debug_to(loger_mysql, 0, ("[setmergestate]task %d, update taskstate success, state=%d\n"), taskid, nstate);
			}
			else
			{
				ret = false;
				std::string error = mysql_error(&mysql);
				_debug_to(loger_mysql, 1, ("[setmergestate]task %d, update taskstate failed: %s\n"), taskid, error.c_str());
			}
			mysql_free_result(result);					//free result
		}
		else
		{
			ret = false;
			std::string error = mysql_error(&mysql);
			_debug_to(loger_mysql, 1, ("[setmergestate]task %d, select state failed: %s\n"), taskid, error.c_str());
		}

		//=====================
		mysql_close(&mysql);	//close connect

		return ret;
	}

};