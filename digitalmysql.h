#pragma once
#include <string>
#include <vector>

#include "public.h"
#include "mysql.h"


namespace digitalmysql
{
	bool getconfig_mysql(std::string configfilepath, std::string& error);

	typedef struct _filterinfo
	{
		int			filtertype;//0=like, 1=between+and, 2=in, 3=������������������
		std::string filterfield;
		std::string filtervalue;

		_filterinfo()
		{
			filtertype = 0;
			filterfield = "";
			filtervalue = "";
		}

		void to_utf8()
		{
			ansi_to_utf8(filterfield.c_str(), filterfield.length(), filterfield);
			ansi_to_utf8(filtervalue.c_str(), filtervalue.length(), filtervalue);
		}
	} filterinfo, * pfilterinfo;
	typedef std::vector<filterinfo> VEC_FILTERINFO;

	//�û�
	typedef struct _userinfo
	{
		unsigned int id;
		int			 disabled;			//�˻��������
		int			 usertype;			//���ֱ����û��������û�
		int			 servicetype;		//�Ʒ�������
		int			 rootflag;			//root�����û����˻�������
		int			 adminflag;			//�Ƿ���admin;0=��ͨ�û�,1=��������Ա,2=��ģ����Ա,3=ƽ̨�û�����Ա
		std::string  userCode;
		std::string  parentCode;
		std::string  siteCode;
		std::string  loginName;
		std::string  loginPassword;
		std::string  phone;
		std::string  email;
		std::string  projectName;
		long long    remainTime;
		std::string  deadlineTime;

		_userinfo()
		{
			id = 0;
			disabled = 1;
			usertype = -1;
			servicetype = -1;
			rootflag = 0;
			adminflag = 0;
			userCode = "";
			parentCode = "";
			siteCode = "";
			loginName = "";
			loginPassword ="";
			phone = "";
			email = "";
			projectName = "";
			remainTime = 0;
			deadlineTime = "";
		}

		void to_utf8()
		{
			ansi_to_utf8(userCode.c_str(), userCode.length(), userCode);
			ansi_to_utf8(parentCode.c_str(), parentCode.length(), parentCode);
			ansi_to_utf8(siteCode.c_str(), siteCode.length(), siteCode);
			ansi_to_utf8(loginName.c_str(), loginName.length(), loginName);
			ansi_to_utf8(loginPassword.c_str(), loginPassword.length(), loginPassword);
			ansi_to_utf8(phone.c_str(), phone.length(), phone);
			ansi_to_utf8(email.c_str(), email.length(), email);
			ansi_to_utf8(projectName.c_str(), projectName.length(), projectName);
			ansi_to_utf8(deadlineTime.c_str(), deadlineTime.length(), deadlineTime);
		}
	}userinfo;
	bool adduserinfo(userinfo useritem, bool update = false);
	bool getuserinfo(int userid, userinfo& useritem);
	bool getuserinfo(std::string name, std::string password, userinfo& useritem);
	bool getusername_id(int userid, std::string& username);
	bool getusercode_id(int userid, std::string& usercode);
	bool getuserid_likename(std::string username, int& userid);
	bool getuserid_parent(int childid, int& parentid);
	bool getuserid_childs(int parentid, std::vector<int>& vecchildid);
	bool isexistuser_id(int userid);
	bool isexistuser_usercode(std::string usercode);
	bool isrootuser_id(int userid);
	bool isvaliduser_id(int userid);
	bool isvaliduser_namepwd(std::string name, std::string password);
	bool islocaluser_namepwd(std::string name, std::string password);
	bool getuserinfo_adminflag(std::string usercode, int& adminflag);
	bool setuserinfo_disable(std::string usercode, int disabled);
	bool setuserinfo_deadlinetime(std::string usercode, std::string deadlinetime);
	bool getuserinfo_remaintime(std::string usercode, long long& remaintime);
	bool setuserinfo_remaintime(std::string usercode, long long remaintime);

	typedef std::vector<userinfo> VEC_USERINFO;
	bool getuserid_allroot(std::vector<int>& vecbelongids);
	bool getuserlistinfo(VEC_FILTERINFO& vecfilterinfo, int pagesize, int pagenum, int& total, VEC_USERINFO& vecuserinfo);
	bool deleteuserinfo_id(int userid, std::string& errmsg);

	//����
	typedef struct _indentinfo
	{
		int			id;
		int			belongid;
		std::string rootname;		//from sbt_userinfo
		int			servicetype;
		int			operationway;
		int			indenttype;
		std::string indentcontent;
		std::string createtime;

 		_indentinfo()
		{
			id = 0;
			belongid = 0;
			rootname = "";
			servicetype = 0;
			operationway = 0;
			indenttype = 0;
			indentcontent = "";
			createtime = "";
		}

		void to_utf8()
		{
			ansi_to_utf8(rootname.c_str(), rootname.length(), rootname);
			ansi_to_utf8(indentcontent.c_str(), indentcontent.length(), indentcontent);
			ansi_to_utf8(createtime.c_str(), createtime.length(), createtime);
		}
	} indentinfo, * pindentinfo;
	typedef std::vector<indentinfo> VEC_INDENTINFO;
	bool addindentinfo(indentinfo indentitem, bool update = false);
	bool getindentlistinfo(VEC_FILTERINFO& vecfilterinfo, int pagesize, int pagenum, int& total, VEC_INDENTINFO& vecindentinfo);

	//������ģ����Դ
	typedef struct _humaninfo
	{
		int id;
		int belongid;		//�����û�
		int privilege;		//Ȩ��,0=����,1=˽��

		std::string humanid;
		std::string humanname;
		std::string contentid;		//�����ǩ
		std::string sourcefolder;
		int			available;
		double		speakspeed;
		double		seriousspeed;
		std::string imagematting;
		std::string keyframe;
		std::string foreground;
		std::string background;
		std::string foreground2;
		std::string background2;
		std::string speakmodelpath;
		std::string pwgmodelpath;
		std::string mouthmodelfile;
		std::string facemodelfile;

		_humaninfo()
		{
			id = -1;
			belongid = 0;
			privilege = 1;

			humanid = "";
			humanname = "";
			contentid = "";
			sourcefolder = "";
			available = 0;
			speakspeed = 1.0;
			seriousspeed = 0.8;
			imagematting = "";
			keyframe = "";
			foreground = "";
			background = "";
			foreground2 = "";
			background2 = "";
			speakmodelpath = "";
			pwgmodelpath = "";
			mouthmodelfile = "";
			facemodelfile = "";
		}

		void to_utf8()
		{
			ansi_to_utf8(humanid.c_str(), humanid.length(), humanid);
			ansi_to_utf8(humanname.c_str(), humanname.length(), humanname);
			ansi_to_utf8(contentid.c_str(), contentid.length(), contentid);
			ansi_to_utf8(sourcefolder.c_str(), sourcefolder.length(), sourcefolder);

			ansi_to_utf8(imagematting.c_str(), imagematting.length(), imagematting);
			ansi_to_utf8(keyframe.c_str(), keyframe.length(), keyframe);
			ansi_to_utf8(foreground.c_str(), foreground.length(), foreground);
			ansi_to_utf8(background.c_str(), background.length(), background);
			ansi_to_utf8(foreground2.c_str(), foreground2.length(), foreground2);
			ansi_to_utf8(background2.c_str(), background2.length(), background2);
			ansi_to_utf8(speakmodelpath.c_str(), speakmodelpath.length(), speakmodelpath);
			ansi_to_utf8(pwgmodelpath.c_str(), pwgmodelpath.length(), pwgmodelpath);
			ansi_to_utf8(mouthmodelfile.c_str(), mouthmodelfile.length(), mouthmodelfile);
			ansi_to_utf8(facemodelfile.c_str(), facemodelfile.length(), facemodelfile);
		}
	} humaninfo, *phumaninfo;
	bool addhumaninfo(humaninfo humanitem, bool update = false);
	bool gethumaninfo(std::string humanid, humaninfo& humanitem);//��ʹ�ù�����Ϣ,�����������û�
	bool isexisthuman_humanid(std::string humanid);//[���õ�ǰ���������˿���],�����������û�
	bool isavailable_humanid(std::string humanid); //[���õ�ǰ���������˿���],�����������û�
	bool gethumaninfo_label(std::string humanid, std::vector<int> vecbelongids, std::string& labelstring);//���������û�,���⸴�����������
	bool sethumaninfo_label(std::string humanid, std::vector<int> vecbelongids, std::string labelstring);//���������û�,���⸴�����������
	bool gethumaninfo_remaintime(std::string humanid, std::vector<int> vecbelongids, long long& remaintime);//���������û�,���⸴�����������
	bool sethumaninfo_remaintime(std::string humanid, std::vector<int> vecbelongids, long long remaintime);//���������û�,���⸴�����������
	bool gethumaninfo_remaintime(int id, long long& remaintime);
	bool sethumaninfo_remaintime(int id, long long remaintime);
	bool deletehuman_humanid(std::string humanid, bool deletefile, std::string& errmsg);//ɾ��ʱ���ú�ԭ������һ��ɾ��

	typedef std::vector<humaninfo> VEC_HUMANINFO;
	bool gethumanid_all(std::vector<int>& vechumanid);
	bool gethumanlistinfo(std::string humanid, std::vector<int> vecbelongids, VEC_HUMANINFO& vechumaninfo);

	//�������
	typedef struct _taskinfo
	{
		int			taskid;
		int			belongid;		//�����û�
		int			privilege;		//Ȩ��,0=����,1=˽��
		std::string groupid;		//������,ǰ�˰�����ʾ���
		std::string versionname;	//�汾��Ӧ���Զ�������
		int			version;		//�汾��

		int			tasktype;		//0=onlyaudio,1=audio+video,2=audio->video
		int			moodtype;		//0=nomal,1=serious
		double		speakspeed;
		std::string taskname;
		int			taskstate;		//0xFE=saved,0xFF=wait merge,0=merging,1=success,2...=failed
		int			taskprogress;
		std::string createtime;
		std::string scannedtime;
		std::string finishtime;
		int			priority;		//0=��,1=��,2=��
		int			islastedit;		//0=�������༭�İ汾��1=�����༭�İ汾
		std::string humanid;
		std::string humanname;
		std::string ssmltext;

		std::string audio_path;
		std::string audio_format;
		int			audio_length;
		std::string video_path;
		std::string video_keyframe;
		std::string video_format;
		int			video_length;
		int			video_width;
		int			video_height;
		double		video_fps;

		std::string foreground;
		double		front_left;
		double		front_right;
		double		front_top;
		double		front_bottom;

		std::string background;
		std::string backaudio;
		double		back_volume;
		int			back_loop;
		int			back_start;
		int			back_end;

		int			window_width;
		int			window_height;

		_taskinfo()
		{
			taskid = 0;
			belongid = 0;
			privilege = 1;
			groupid = "";
			versionname="";
			version = -1;

			tasktype = -1;
			moodtype = 0;
			speakspeed = 1.0;
			taskname = "";
			taskstate = -1;
			taskprogress = -1;
			createtime = "";
			scannedtime = "";
			finishtime = "";
			priority = 0;
			islastedit = 0;
			humanid = "";
			humanname = "";
			ssmltext = "";

			audio_path = "";
			audio_format = "";
			audio_length = 0;
			video_path = "";
			video_keyframe = "";
			video_format = "";
			video_length = 0;
			video_width = 0;
			video_height = 0;
			video_fps = 0.0;

			foreground = "";
			front_left = 0.0;
			front_right = 0.0;
			front_top = 1.0;
			front_bottom = 1.0;

			background = "";
			backaudio = "";
			back_volume = 0.00;
			back_loop = 1;
			back_start = 0;
			back_end = 0;

			window_width = 1920;
			window_height = 1080;
		}

		void to_utf8()
		{
			ansi_to_utf8(groupid.c_str(), groupid.length(), groupid);
			ansi_to_utf8(versionname.c_str(), versionname.length(), versionname);
			ansi_to_utf8(taskname.c_str(), taskname.length(), taskname);

			ansi_to_utf8(createtime.c_str(), createtime.length(), createtime);
			ansi_to_utf8(scannedtime.c_str(), scannedtime.length(), scannedtime);
			ansi_to_utf8(finishtime.c_str(), finishtime.length(), finishtime);
			ansi_to_utf8(humanid.c_str(), humanid.length(), humanid);
			ansi_to_utf8(humanname.c_str(), humanname.length(), humanname);
			ansi_to_utf8(ssmltext.c_str(), ssmltext.length(), ssmltext);

			ansi_to_utf8(audio_path.c_str(), audio_path.length(), audio_path);
			ansi_to_utf8(audio_format.c_str(), audio_format.length(), audio_format);

			ansi_to_utf8(video_path.c_str(), video_path.length(), video_path);
			ansi_to_utf8(video_keyframe.c_str(), video_keyframe.length(), video_keyframe);
			ansi_to_utf8(video_format.c_str(), video_format.length(), video_format);

			ansi_to_utf8(foreground.c_str(), foreground.length(), foreground);
			ansi_to_utf8(background.c_str(), background.length(), background);
			ansi_to_utf8(backaudio.c_str(), backaudio.length(), backaudio);
		}

	} taskinfo, * ptaskinfo;
	bool addtaskinfo(int& taskid, taskinfo taskitem, bool update = false);
	bool gettaskinfo(int taskid, taskinfo& taskitem);
	bool clearscannedtime_id(int taskid);
	bool setscannedtime_nextrun(std::string scannedtime);
	bool gettaskinfo_nextrun(std::string scannedtime, taskinfo& taskitem);
	bool setaudiopath(int taskid, std::string audiopath);
	bool setvideopath(int taskid, std::string videopath);
	bool setfinishtime(int taskid, std::string finishtime);
	bool setkeyframepath(int taskid, std::string keyframepath);
	bool setpriority(int taskid, int priority);
	bool settasklastedit(std::string groupid, int taskid);//�����ָ���汾��Ϊ���༭״̬
	bool isexisttask_taskid(int taskid);
	int  gettasknextversion(std::string groupid);

	typedef std::vector<taskinfo> VEC_TASKINFO;
	bool gettaskhistoryinfo(VEC_FILTERINFO& vecfilterinfo, std::string order_key, int order_way, int pagesize, int pagenum, int& total, VEC_TASKINFO& vectaskhistory, std::vector<int> vecbelongids);
	bool gettaskgroupinfo(std::string groupid, VEC_TASKINFO& vectaskgroup);
	bool gettasklistinfo(VEC_FILTERINFO& vecfilterinfo, int pagesize, int pagenum, int& total, VEC_TASKINFO& vectasklist);
	bool deletetask_taskid(int taskid, bool deletefile, std::string& errmsg);

	//������Դ
	enum sourceType
	{
		source_image = 0,
		source_video,
		source_audio,
	};
	typedef struct _tasksourceinfo 
	{
		int			id;
		int			belongid;		//�����û�
		int			privilege;		//Ȩ��,0=����,1=˽��

		int			sourcetype;		//0=image,1=video,2=audio
		std::string sourcepath;
		std::string sourcekeyframe;
		int			sourcewidth;	//image��video������
		int			sourceheight;	//image��video����߶�
		std::string createtime;

		_tasksourceinfo()
		{
			id = 0;
			belongid = 0;
			privilege = 1;

			sourcetype = -1;
			sourcepath = "";
			sourcekeyframe = "";
			sourcewidth = 0;
			sourceheight = 0;
			createtime = "";
		}

		void to_utf8()
		{
			ansi_to_utf8(sourcepath.c_str(), sourcepath.length(), sourcepath);
			ansi_to_utf8(sourcekeyframe.c_str(), sourcekeyframe.length(), sourcekeyframe);
			ansi_to_utf8(createtime.c_str(), createtime.length(), createtime);
		}

	} tasksourceinfo, *ptasksourceinfo;
	typedef std::vector<tasksourceinfo> VEC_TASKSOURCEINFO;
	bool addtasksource(tasksourceinfo tasksourceitem, bool update = false);
	bool gettasksource(int id, tasksourceinfo& tasksourceitem);
	bool isexisttasksource_path(std::string sourcepath);

	bool gettasksourcelist(std::vector<int> vecbelongids, VEC_TASKSOURCEINFO& vectasksource);
	bool deletetasksource_id(int id, bool deletefile, std::string& errmsg);

	//������Դ
	typedef struct _humanactioninfo
	{
		int			id;

		std::string	humanid;
		std::string actionname;
		std::string actionkeyframe;
		std::string actionvideo;
		int			actionduration;
		int			videowidth;
		int			videoheight;

		_humanactioninfo()
		{
			id = 0;

			humanid = "";
			actionname = "";
			actionkeyframe = "";
			actionvideo = "";
			actionduration = 0;
			videowidth = 0;
			videoheight = 0;
		}

		void to_utf8()
		{
			ansi_to_utf8(humanid.c_str(), humanid.length(), humanid);
			ansi_to_utf8(actionname.c_str(), actionname.length(), actionname);
			ansi_to_utf8(actionkeyframe.c_str(), actionkeyframe.length(), actionkeyframe);
			ansi_to_utf8(actionvideo.c_str(), actionvideo.length(), actionvideo);
		}

	} humanactioninfo, *phumanactioninfo;
	typedef std::vector<humanactioninfo> VEC_HUMANACTIONINFO;
	bool addhumanaction(humanactioninfo humanactionitem);
	bool getactionlist(std::string humanid, VEC_HUMANACTIONINFO& vechumanaction);

	//ԭʼ��Ƶ
	typedef struct _originalvdoinfo
	{
		std::string humanid;
		std::string videofile;
		int			moodtype;
		int			duration;
		int			filesize;
		int			fromtype;//0��ʾΪ�û��ϴ���1��ʾΪ��������ϴ�

		_originalvdoinfo()
		{
			humanid = "";
			videofile = "";
			moodtype = -1;
			duration = 0;
			filesize = 0;
			fromtype = -1;
		}

		void to_utf8()
		{
			ansi_to_utf8(humanid.c_str(), humanid.length(), humanid);
			ansi_to_utf8(videofile.c_str(), videofile.length(), videofile);
		}
	} originalvdoinfo, * poriginalvdoinfo;
	bool addoriginalvdo(originalvdoinfo originalvdoitem);

	//�������
	int  getmergeprogress(int taskid);
	bool setmergeprogress(int taskid, int nprogress);
	//-1=waitmerge,0=merging,1=mergesuccess,2=mergefailed,3=movefailed
	int  getmergestate(int taskid);
	bool setmergestate(int taskid, int nstate);
};

