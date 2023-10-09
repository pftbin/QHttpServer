#include "digitalEntityJson.h"

#define BUFF_SZ					1024*16		//system max stack size
#define DF_IMAGE_USEKEYDATA		0

static std::string text_replace(std::string str, std::string old, std::string now)
{
	if (str.empty())
		return "";

	int oldPos = 0;
	while (str.find(old, oldPos) != -1)
	{
		int start = str.find(old, oldPos);
		str.replace(start, old.size(), now);
		oldPos = start + now.size();
	}
	return str;
}

static std::string jsontext_replace(std::string jsonString)
{
	std::string result = jsonString;
	result = text_replace(result, "\"", "");
	result = text_replace(result, "'",  "");
	result = text_replace(result, "\n", "");
	result = text_replace(result, "\r", "");
	result = text_replace(result, "\t", "");
	result = text_replace(result, "\\", "");

	return result;
}

//===========================================================================================================================================
std::string DigitalMan_Item::writeJson()
{
	std::string sResultJson = "";
	char buff[BUFF_SZ] = { 0 };
	std::string skeyvalue = "";

	//
	snprintf(buff, BUFF_SZ, "\"HumanID\":\"%s\" ,", HumanID.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"HumanName\":\"%s\" ,", HumanName.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"HumanLabel\":\"%s\" ,", HumanLabel.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"HumanRemainTime\":\"%s\" ,", HumanRemainTime.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"HumanAvailable\":%d,", HumanAvailable); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"SpeakSpeed\":%.6f,", SpeakSpeed); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"Foreground\":\"%s\" ,", Foreground.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"Background\":\"%s\" ,", Background.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"Foreground2\":\"%s\" ,", Foreground2.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"Background2\":\"%s\" ,", Background2.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	//
	snprintf(buff, BUFF_SZ, "\"KeyFrame\":{\"Format\":\"%s\",\"Width\": %d,\"Height\": %d,\"BitCount\": %d, ", KeyFrame_Format.c_str(), KeyFrame_Width, KeyFrame_Height, KeyFrame_BitCount); skeyvalue = buff;
	sResultJson += skeyvalue;

#if DF_IMAGE_USEKEYDATA
	std::string keydata = "\"KeyData\":\"";
	keydata += KeyFrame_KeyData;		//KeyData too long,use string
	keydata += "\" },";	
	sResultJson += keydata;
#else
	std::string filepath = "\"FilePath\":\"" + KeyFrame_FilePath + "\"},";
	sResultJson += filepath;
#endif

	//5,select database and add to this class
	sResultJson += "\"HumanData\": [";
	for (size_t i = 0; i < vecDigitManTaskObj.size(); i++)
	{
		sResultJson += vecDigitManTaskObj[i];

		if (i != vecDigitManTaskObj.size()-1)
			sResultJson += ",";	//last no ","
	}
	sResultJson += "]";

	return sResultJson;
}

std::string DigitalMan_Items::writeJson()
{
	std::string sResultJson = "";
	char buff[BUFF_SZ] = { 0 };
	std::string skeyvalue = "";

	//0
	for (size_t i = 0; i < vecDigitManItems.size(); ++i)
	{
		std::string vecObjTemp = vecDigitManItems[i].writeJson();
		std::string vecObjJson = "{" + vecObjTemp + "}";

		sResultJson += vecObjJson;
		if (i != vecDigitManItems.size() - 1)//last no ","
			sResultJson += ",";
	}

	return sResultJson;
}

//===========================================================================================================================================
std::string DigitalMan_Task::writeJson()
{
	std::string sResultJson = "";
	char buff[BUFF_SZ] = { 0 };
	std::string skeyvalue = "";

	//文本内容替换,避免json格式错误
	TaskName = jsontext_replace(TaskName);
	TaskVersionName = jsontext_replace(TaskVersionName);
	TaskInputSsml = jsontext_replace(TaskInputSsml);

	snprintf(buff, BUFF_SZ, "\"TaskGroupID\":\"%s\",", TaskGroupID.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"TaskID\":%d,", TaskID); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"TaskType\":%d,", TaskType); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"TaskMoodType\":%d,", TaskMoodType); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"TaskName\":\"%s\",", TaskName.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"TaskVersionName\":\"%s\",", TaskVersionName.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"TaskVersion\":%d,", TaskVersion); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"State\":%d,", TaskState); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"Progress\":%d,", TaskProgerss); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"SpeakSpeed\":%.6f,", TaskSpeakSpeed); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"InputSsml\":\"%s\",", TaskInputSsml.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"Createtime\":\"%s\",", TaskCreateTime.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"IsLastEdit\":%d,", TaskIsLastEdit); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"HumanID\":\"%s\",", TaskHumanID.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"HumanName\":\"%s\",", TaskHumanName.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;

	//
	snprintf(buff, BUFF_SZ, "\"Foreground\":\"%s\",", Foreground.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"left\":%.6f,", Front_left); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"right\":%.6f,", Front_right); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"top\":%.6f,", Front_top); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"bottom\":%.6f,", Front_bottom); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"Background\":\"%s\",", Background.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"BackAudio\":\"%s\",", BackAudio.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"volume\":%.6f,", Back_volume); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"loop\":%d,", Back_loop); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"start\":%d,", Back_start); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"end\":%d,", Back_end); skeyvalue = buff;
	sResultJson += skeyvalue;

	snprintf(buff, BUFF_SZ, "\"WndWidth\":%d,", Window_width); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"WndHeight\":%d,", Window_height); skeyvalue = buff;
	sResultJson += skeyvalue;

	//
	snprintf(buff, BUFF_SZ, "\"KeyFrame\":{\"Format\":\"%s\",\"Width\": %d,\"Height\": %d,\"BitCount\": %d, ", KeyFrame_Format.c_str(), KeyFrame_Width, KeyFrame_Height, KeyFrame_BitCount); skeyvalue = buff;
	sResultJson += skeyvalue;

#if DF_IMAGE_USEKEYDATA
	std::string keydata = "\"KeyData\":\"";
	keydata += KeyFrame_KeyData;		//KeyData too long,use string
	keydata += "\" },";
	sResultJson += keydata;
#else
	std::string filepath = "\"FilePath\":\"" + KeyFrame_FilePath + "\"},";
	sResultJson += filepath;
#endif

	snprintf(buff, BUFF_SZ, "\"Audio\":{\"AudioFormat\":\"%s\",\"AudioFile\":\"%s\",\"Duration\":\"%s\"},", Audio_Format.c_str(), Audio_File.c_str(), Audio_Duration.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;

	snprintf(buff, BUFF_SZ, "\"Vedio\":{\"VideoFormat\":\"%s\",\"Width\":%d,\"Height\":%d,\"Fps\":%.2f,\"VedioFile\":\"%s\",\"Duration\":\"%s\"}", Video_Format.c_str(), Video_Width, Video_Height, Video_Fps, Video_File.c_str(), Video_Duration.c_str()); skeyvalue = buff;//last no ","
	sResultJson += skeyvalue;

	return sResultJson;
}

std::string DigitalMan_Tasks::writeJson()
{
	std::string sResultJson = "";
	char buff[BUFF_SZ] = { 0 };
	std::string skeyvalue = "";

	//1
	for (size_t i = 0; i < vecDigitManTasks.size(); ++i)
	{
		std::string vecObjTemp = vecDigitManTasks[i].writeJson();
		std::string vecObjJson = "{" + vecObjTemp + "}";

		sResultJson += vecObjJson;
		if (i != vecDigitManTasks.size() - 1)//last no ","
			sResultJson += ",";
	}

	return sResultJson;
}

//===========================================================================================================================================
std::string DigitalMan_TaskSource::writeJson()
{
	std::string sResultJson = "";
	char buff[BUFF_SZ] = { 0 };
	std::string skeyvalue = "";

	snprintf(buff, BUFF_SZ, "\"SourceID\":%d,", TaskSource_Id); skeyvalue = buff;
	sResultJson += skeyvalue;

	if (TaskSource_Type == 0)//image
	{
		snprintf(buff, BUFF_SZ, "\"ImageFile\":\"%s\",", TaskSource_FilePath.c_str()); skeyvalue = buff;
		sResultJson += skeyvalue;
		snprintf(buff, BUFF_SZ, "\"SourceWidth\":%d,", TaskSource_Width); skeyvalue = buff;
		sResultJson += skeyvalue;
		snprintf(buff, BUFF_SZ, "\"SourceHeight\":%d", TaskSource_Height); skeyvalue = buff;//last no ","
		sResultJson += skeyvalue;
	}
	if (TaskSource_Type == 1)//video
	{
		snprintf(buff, BUFF_SZ, "\"VideoFile\":\"%s\",", TaskSource_FilePath.c_str()); skeyvalue = buff;
		sResultJson += skeyvalue;
		snprintf(buff, BUFF_SZ, "\"KeyFrameFile\":\"%s\",", TaskSource_KeyFrame.c_str()); skeyvalue = buff;
		sResultJson += skeyvalue;
		snprintf(buff, BUFF_SZ, "\"SourceWidth\":%d,", TaskSource_Width); skeyvalue = buff;
		sResultJson += skeyvalue;
		snprintf(buff, BUFF_SZ, "\"SourceHeight\":%d", TaskSource_Height); skeyvalue = buff;//last no ","
		sResultJson += skeyvalue;
	}
	if (TaskSource_Type == 2)//audio
	{
		snprintf(buff, BUFF_SZ, "\"AudioFile\":\"%s\"", TaskSource_FilePath.c_str()); skeyvalue = buff;//last no ","
		sResultJson += skeyvalue;
	}

	return sResultJson;
}

std::string DigitalMan_TaskSources::writeJson()
{
	std::string sResultJson = "";
	char buff[BUFF_SZ] = { 0 };
	std::string skeyvalue = "";

	//1
	for (size_t i = 0; i < vecDigitManTaskSources.size(); ++i)
	{
		std::string vecObjTemp = vecDigitManTaskSources[i].writeJson();
		std::string vecObjJson = "{" + vecObjTemp + "}";

		sResultJson += vecObjJson;
		if (i != vecDigitManTaskSources.size() - 1)//last no ","
			sResultJson += ",";
	}

	return sResultJson;
}

//===========================================================================================================================================
std::string DigitalMan_UserItem::writeJson()
{
	std::string sResultJson = "";
	char buff[BUFF_SZ] = { 0 };
	std::string skeyvalue = "";

	snprintf(buff, BUFF_SZ, "\"UserID\":%d,", UserID); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"UserName\":\"%s\",", UserName.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	//snprintf(buff, BUFF_SZ, "\"UserPassWord\":\"%s\",", UserPassWord.c_str()); skeyvalue = buff;
	//sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"RootName\":\"%s\",", RootName.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"AdminFlag\":%d,", AdminFlag); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"UserType\":%d,", UserType); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"ServiceType\":%d,", ServiceType); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"ProjectName\":\"%s\",", ProjectName.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"ReaminTime\":\"%s\",", ReaminTime.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"DeadlineTime\":\"%s\",", DeadlineTime.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"UserEmail\":\"%s\",", UserEmail.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"UserPhone\":\"%s\"", UserPhone.c_str()); skeyvalue = buff;//last no ","
	sResultJson += skeyvalue;

	return sResultJson;
}

std::string DigitalMan_UserItems::writeJson()
{
	std::string sResultJson = "";
	char buff[BUFF_SZ] = { 0 };
	std::string skeyvalue = "";

	//1
	for (size_t i = 0; i < vecDigitManUserItems.size(); ++i)
	{
		std::string vecObjTemp = vecDigitManUserItems[i].writeJson();
		std::string vecObjJson = "{" + vecObjTemp + "}";

		sResultJson += vecObjJson;
		if (i != vecDigitManUserItems.size() - 1)//last no ","
			sResultJson += ",";
	}

	return sResultJson;
}

//===========================================================================================================================================
std::string DigitalMan_Ident::writeJson()
{
	std::string sResultJson = "";
	char buff[BUFF_SZ] = { 0 };
	std::string skeyvalue = "";

	snprintf(buff, BUFF_SZ, "\"IdentID\":%d,", IdentID); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"RootID\":%d,", RootID); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"RootName\":\"%s\",", RootName.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"ServiceType\":%d,", ServiceType); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"OperationWay\":%d,", OperationWay); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"IndentType\":%d,", IndentType); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"IndentContent\":\"%s\",", IndentContent.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"CreateTime\":\"%s\"", CreateTime.c_str()); skeyvalue = buff;//last no ","
	sResultJson += skeyvalue;

	return sResultJson;
}

std::string DigitalMan_Idents::writeJson()
{
	std::string sResultJson = "";
	char buff[BUFF_SZ] = { 0 };
	std::string skeyvalue = "";

	//1
	for (size_t i = 0; i < vecDigitManIdents.size(); ++i)
	{
		std::string vecObjTemp = vecDigitManIdents[i].writeJson();
		std::string vecObjJson = "{" + vecObjTemp + "}";

		sResultJson += vecObjJson;
		if (i != vecDigitManIdents.size() - 1)//last no ","
			sResultJson += ",";
	}

	return sResultJson;
}

//===========================================================================================================================================
std::string DigitalMan_TaskEx::writeJson()
{
	std::string sResultJson = "";
	char buff[BUFF_SZ] = { 0 };
	std::string skeyvalue = "";

	//文本内容替换,避免json格式错误
	TaskName = jsontext_replace(TaskName);

	snprintf(buff, BUFF_SZ, "\"TaskGroupID\":\"%s\",", TaskGroupID.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"TaskID\":%d,", TaskID); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"TaskType\":%d,", TaskType); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"TaskState\":%d,", TaskState); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"TaskPriority\":%d,", TaskPriority); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"TaskFileName\":\"%s\",", TaskFileName.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;

	snprintf(buff, BUFF_SZ, "\"TaskName\":\"%s\",", TaskName.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"TaskDuration\":\"%s\",", TaskDuration.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"TaskCreateTime\":\"%s\",", TaskCreateTime.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"TaskFinishTime\":\"%s\",", TaskFinishTime.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;

	snprintf(buff, BUFF_SZ, "\"TaskUserID\":%d,", TaskUserID); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"TaskUserName\":\"%s\",", TaskUserName.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"TaskRootID\":%d,", TaskRootID); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"TaskRootName\":\"%s\",", TaskRootName.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"TaskHumanID\":\"%s\",", TaskHumanID.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"TaskHumanName\":\"%s\"", TaskHumanName.c_str()); skeyvalue = buff;//last no ","
	sResultJson += skeyvalue;

	return sResultJson;
}

std::string DigitalMan_TaskExs::writeJson()
{
	std::string sResultJson = "";
	char buff[BUFF_SZ] = { 0 };
	std::string skeyvalue = "";

	//1
	for (size_t i = 0; i < vecDigitManTaskExs.size(); ++i)
	{
		std::string vecObjTemp = vecDigitManTaskExs[i].writeJson();
		std::string vecObjJson = "{" + vecObjTemp + "}";

		sResultJson += vecObjJson;
		if (i != vecDigitManTaskExs.size() - 1)//last no ","
			sResultJson += ",";
	}

	return sResultJson;
}

//===========================================================================================================================================
std::string DigitalMan_ActionItem::writeJson()
{
	std::string sResultJson = "";
	char buff[BUFF_SZ] = { 0 };
	std::string skeyvalue = "";

	snprintf(buff, BUFF_SZ, "\"ActionID\":%d,", ActionID); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"ActionName\":\"%s\",", ActionName.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"ActionKeyframe\":\"%s\",", ActionKeyframe.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"ActionVideo\":\"%s\",", ActionVideo.c_str()); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"ActionDuration\":%d,", ActionDuration); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"VideoWidth\":%d,", VideoWidth); skeyvalue = buff;
	sResultJson += skeyvalue;
	snprintf(buff, BUFF_SZ, "\"VideoHeight\":%d", VideoHeight); skeyvalue = buff;
	sResultJson += skeyvalue;


	return sResultJson;
}

std::string DigitalMan_ActionItems::writeJson()
{
	std::string sResultJson = "";
	char buff[BUFF_SZ] = { 0 };
	std::string skeyvalue = "";

	//1
	for (size_t i = 0; i < vecDigitManActionItems.size(); ++i)
	{
		std::string vecObjTemp = vecDigitManActionItems[i].writeJson();
		std::string vecObjJson = "{" + vecObjTemp + "}";

		sResultJson += vecObjJson;
		if (i != vecDigitManActionItems.size() - 1)//last no ","
			sResultJson += ",";
	}

	return sResultJson;
}

