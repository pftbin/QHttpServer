#pragma once
#include <string>
#include <vector>
#include "json.h"

//HumanList
//===========================================================================================================================================
class DigitalMan_Item
{
public:
	DigitalMan_Item() 
	{
		HumanID = "";
		HumanName="";
		HumanLabel = "";
		HumanRemainTime = "";
		HumanAvailable = 0;
		SpeakSpeed = 1.0;
		Foreground = "";
		Background = "";
		Foreground2 = "";
		Background2 = "";
		KeyFrame_Format="";
		KeyFrame_Width=0;
		KeyFrame_Height=0;
		KeyFrame_BitCount=0;
		KeyFrame_FilePath="";
		KeyFrame_KeyData="";
	};
	virtual ~DigitalMan_Item() {};

	virtual std::string writeJson();

public:
	std::string HumanID;
	std::string HumanName;
	std::string HumanLabel;
	std::string HumanRemainTime;
	int			HumanAvailable;
	double		SpeakSpeed;
	std::string Foreground;
	std::string Background;
	std::string Foreground2;
	std::string Background2;
	std::string KeyFrame_Format;
	int			KeyFrame_Width;
	int			KeyFrame_Height;
	int			KeyFrame_BitCount;
	std::string KeyFrame_FilePath;
	std::string KeyFrame_KeyData;

	std::vector<std::string> vecDigitManTaskObj;//用于区分数字人时
};
class DigitalMan_Items
{
public:
	DigitalMan_Items() {};
	virtual ~DigitalMan_Items() {};

	virtual std::string writeJson();
public:
	std::vector<DigitalMan_Item> vecDigitManItems;
};

//HumanHistory
//===========================================================================================================================================
class DigitalMan_Task
{
public:
	DigitalMan_Task() 
	{
		TaskID = 0;
		TaskType = 1;
		TaskMoodType = 0;
		TaskName = "";
		TaskState = 0;
		TaskProgerss = 0;
		TaskSpeakSpeed = 1.0;
		TaskInputSsml = "";
		TaskCreateTime = "";
		TaskHumanID = "";
		TaskHumanName = "";

		//
		Foreground = "";
		Front_left = 0.0;
		Front_right = 1.0;
		Front_top = 0.0;
		Front_bottom = 1.0;
		Background = "";
		BackAudio = "";
		Back_volume = 0.0;
		Back_loop = 1;
		Back_start = 0;
		Back_end = 0;

		//
		KeyFrame_Format="";
		KeyFrame_Width=0;
		KeyFrame_Height=0;
		KeyFrame_BitCount=0;
		KeyFrame_FilePath="";
		KeyFrame_KeyData="";

		Audio_Format="";
		Audio_File="";

		Video_Format="";
		Video_Width=0;
		Video_Height=0;
		Video_Fps=0.0;
		Video_File="";
	};
	virtual ~DigitalMan_Task() {};

	virtual std::string writeJson();

public:
	std::string	TaskGroupID;
	int			TaskID;
	int			TaskType;
	int			TaskMoodType;
	std::string TaskName;
	std::string TaskVersionName;
	int			TaskVersion;
	int			TaskState;
	int			TaskProgerss;
	double		TaskSpeakSpeed;
	std::string TaskInputSsml;
	std::string TaskCreateTime;
	int			TaskIsLastEdit;
	std::string TaskHumanID;
	std::string TaskHumanName;

	//
	std::string Foreground;
	double		Front_left;
	double		Front_right;
	double		Front_top;
	double		Front_bottom;
	std::string Background;
	std::string BackAudio;
	double		Back_volume;
	int			Back_loop;
	int			Back_start;
	int			Back_end;
	
	int			Window_width;
	int			Window_height;

	//
	std::string KeyFrame_Format;
	int			KeyFrame_Width;
	int			KeyFrame_Height;
	int			KeyFrame_BitCount;
	std::string KeyFrame_FilePath;
	std::string KeyFrame_KeyData;

	std::string Audio_Format;
	std::string Audio_File;
	std::string Audio_Duration;

	std::string Video_Format;
	int			Video_Width;
	int			Video_Height;
	double		Video_Fps;
	std::string Video_File;
	std::string Video_Duration;
};
class DigitalMan_Tasks
{
public:
	DigitalMan_Tasks() {};
	virtual ~DigitalMan_Tasks() {};

	virtual std::string writeJson();

public:
	std::vector<DigitalMan_Task> vecDigitManTasks;
};

//TaskSourceList
//===========================================================================================================================================
class DigitalMan_TaskSource
{
public:
	DigitalMan_TaskSource()
	{

	};
	~DigitalMan_TaskSource() {};

	virtual std::string writeJson();

public:
	int				TaskSource_Id;
	int				TaskSource_Type;
	std::string		TaskSource_FilePath;
	std::string		TaskSource_KeyFrame;
	int				TaskSource_Width;
	int				TaskSource_Height;
};
class DigitalMan_TaskSources
{
public:
	DigitalMan_TaskSources(){};
	~DigitalMan_TaskSources() {};

	virtual std::string writeJson();

public:
	std::vector<DigitalMan_TaskSource> vecDigitManTaskSources;
};

//UserList
//===========================================================================================================================================
class DigitalMan_UserItem
{
public:
	DigitalMan_UserItem()
	{
		UserID = 0;
		UserName = "";
		UserPassWord = "";
		RootName = "";
		AdminFlag = 0;
		UserType = 0;
		ServiceType = 0;
		ProjectName = "";
		ReaminTime = "";
		DeadlineTime = "";
		UserEmail = "";
		UserPhone = "";
	};
	virtual ~DigitalMan_UserItem() {};

	virtual std::string writeJson();

public:
	int				UserID;
	std::string		UserName;
	std::string		UserPassWord;
	std::string		RootName;
	int				AdminFlag;
	int				UserType;
	int				ServiceType;
	std::string		ProjectName;
	std::string		ReaminTime;
	std::string		DeadlineTime;
	std::string		UserEmail;
	std::string		UserPhone;
};
class DigitalMan_UserItems
{
public:
	DigitalMan_UserItems() {};
	~DigitalMan_UserItems() {};

	virtual std::string writeJson();

public:
	std::vector<DigitalMan_UserItem> vecDigitManUserItems;
};

//IndentList
//===========================================================================================================================================
class DigitalMan_Ident
{
public:
	DigitalMan_Ident()
	{
		RootID = 0;
		RootName = "";
		IdentID = 0;
		ServiceType = 0;
		OperationWay = 0;
		IndentType = 0;
		IndentContent = "";
		CreateTime = "";
	};
	virtual ~DigitalMan_Ident() {};

	virtual std::string writeJson();

public:
	int				IdentID;
	int				RootID;
	std::string		RootName;
	int				ServiceType;
	int				OperationWay;
	int				IndentType;
	std::string		IndentContent;
	std::string		CreateTime;
};
class DigitalMan_Idents
{
public:
	DigitalMan_Idents() {};
	virtual ~DigitalMan_Idents() {};

	virtual std::string writeJson();

public:
	std::vector<DigitalMan_Ident> vecDigitManIdents;
};

//TaskList
//===========================================================================================================================================
class DigitalMan_TaskEx
{
public:
	DigitalMan_TaskEx()
	{
		TaskGroupID = "";
		TaskID = 0;
		TaskType = -1;
		TaskState = -1;
		TaskPriority = 0;
		TaskFileName = "";

		TaskName = "";
		TaskDuration = "";
		TaskCreateTime = "";
		TaskFinishTime = "";

		TaskUserID = 0;
		TaskUserName = "";
		TaskRootID = 0;
		TaskRootName = "";
		TaskHumanID = "";
		TaskHumanName = "";
	};
	virtual ~DigitalMan_TaskEx() {};

	virtual std::string writeJson();

public:
	std::string	TaskGroupID;
	int			TaskID;
	int			TaskType;
	int			TaskState;
	int			TaskPriority; //0=低,1=中,高=2
	std::string TaskFileName;

	std::string TaskName;
	std::string TaskDuration;
	std::string TaskCreateTime;
	std::string TaskFinishTime;

	int			TaskUserID;
	std::string TaskUserName;
	int			TaskRootID;
	std::string TaskRootName;
	std::string TaskHumanID;
	std::string TaskHumanName;
};
class DigitalMan_TaskExs
{
public:
	DigitalMan_TaskExs() {};
	virtual ~DigitalMan_TaskExs() {};

	virtual std::string writeJson();

public:
	std::vector<DigitalMan_TaskEx> vecDigitManTaskExs;
};

//ActionList
//===========================================================================================================================================
class DigitalMan_ActionItem
{
public:
	DigitalMan_ActionItem()
	{
		ActionID = 0;
		ActionName = "";
		ActionKeyframe = "";
		ActionVideo = "";
		ActionDuration = 0;
		VideoWidth = 0;
		VideoHeight = 0;
	};
	virtual ~DigitalMan_ActionItem() {};

	virtual std::string writeJson();

public:
	int			ActionID;
	std::string ActionName;
	std::string ActionKeyframe;
	std::string ActionVideo;
	int			ActionDuration;
	int			VideoWidth;
	int			VideoHeight;
};
class DigitalMan_ActionItems
{
public:
	DigitalMan_ActionItems() {};
	virtual ~DigitalMan_ActionItems() {};

	virtual std::string writeJson();

public:
	std::vector<DigitalMan_ActionItem> vecDigitManActionItems;
};