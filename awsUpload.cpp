#include "awsUpload.h"
#include "public.h"
#include "dirent.h" //windows���Լ�ʵ��,linux����ϵͳ�ļ�

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#pragma comment(lib, "aws-c-auth.lib") 
#pragma comment(lib, "aws-c-cal.lib") 
#pragma comment(lib, "aws-c-common.lib") 
#pragma comment(lib, "aws-c-compression.lib") 
#pragma comment(lib, "aws-c-event-stream.lib") 
#pragma comment(lib, "aws-checksums.lib") 
#pragma comment(lib, "aws-c-http.lib") 
#pragma comment(lib, "aws-c-io.lib") 
#pragma comment(lib, "aws-c-mqtt.lib") 
#pragma comment(lib, "aws-crt-cpp.lib") 
#pragma comment(lib, "aws-c-s3.lib") 
#pragma comment(lib, "aws-c-sdkutils.lib") 
#pragma comment(lib, "aws-cpp-sdk-core.lib") 
#pragma comment(lib, "aws-cpp-sdk-s3.lib") 

using namespace Aws::S3::Model;
#undef  GetMessage				//������Windows�Ķ����ͻ 
#undef  GetObject				//������Windows�Ķ����ͻ
#undef  DeleteFile				//������Windows�Ķ����ͻ

#define DF_RETRY_TIMES	3		//���Դ���

//custom loger
static FileWriter loger_upload("awsUpload.log");

//�ж��ļ��Ƿ����
inline bool file_exists(const std::string& name)
{
	struct stat buffer;
	return (stat(name.c_str(), &buffer) == 0);
}

//����ļ���С����λ�ֽ�
inline size_t get_filesize(const char* fileName)
{
	if (fileName == nullptr)
		return 0;

	// ����һ���洢�ļ�(��)��Ϣ�Ľṹ�壬�������ļ���С�ʹ���ʱ�䡢����ʱ�䡢�޸�ʱ���
	struct stat statbuf;

	// �ṩ�ļ����ַ���������ļ����Խṹ��
	stat(fileName, &statbuf);

	// ��ȡ�ļ���С
	size_t filesize = statbuf.st_size;

	return filesize;
}

//ö�ٱ���Ŀ¼
inline void list_localfolder(const char* folderpath, std::vector<std::string>& vecfilepath)
{
	DIR* dir;
	struct dirent* entry;

	if ((dir = opendir(folderpath)) != NULL)
	{
		while ((entry = readdir(dir)) != NULL)
		{
			if (entry->d_type == DT_DIR)
			{
				if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
				{
					char new_path[1000];
					sprintf(new_path, "%s/%s", folderpath, entry->d_name);
					list_localfolder(new_path, vecfilepath);
				}
			}
			else {
				char file_path[1000];
				sprintf(file_path, "%s/%s", folderpath, entry->d_name);
				vecfilepath.push_back(file_path);
			}
		}
		closedir(dir);
	}
}

awsUpload::awsUpload(void)
{
	Aws::InitAPI(m_aws_options);
}

awsUpload:: ~awsUpload(void)
{
	Aws::ShutdownAPI(m_aws_options);
}

//��ǰ�ļ�ͳһҪ��: �����������Ϊutf8����
bool awsUpload::SetAWSConfig(std::string strWebPrefix, std::string strEndPoint, std::string aws_url, std::string aws_ak, std::string aws_sk, std::string aws_bucket, std::string aws_region)
{
	if (strWebPrefix.empty() || strEndPoint.empty() || aws_url.empty() || aws_ak.empty() || aws_sk.empty() || aws_bucket.empty())
		return false;

	m_strWebPrefix = strWebPrefix;
	m_strEndPoint = strEndPoint;
	m_strUrl = aws_url;
	m_strAK = aws_ak;
	m_strSK = aws_sk;
	m_strBucket = aws_bucket;
	m_strRegion = aws_region;

	return true;
}

bool awsUpload::UploadFile(std::string object_folder, std::string localfile_path, std::string& urlfile_path, bool multipart)
{
	if (localfile_path.empty())
		return false;

	//�ϴ�
	BOOL bResult = false;
	if (object_folder.back() != '/')
		object_folder += std::string("/");
	std::string object_name = get_path_name(localfile_path);
	std::string objectfile_path = object_folder + object_name;
	for (int i = 0; i < DF_RETRY_TIMES; i++)
	{
		if (multipart)
		{
			if (put_s3_object_multipart(objectfile_path, localfile_path, urlfile_path))
			{
				bResult = true;
				break;
			}
		}
		else
		{
			if (put_s3_object(objectfile_path, localfile_path, urlfile_path))
			{
				bResult = true;
				break;
			}
		}
	}

	return bResult;
}

bool awsUpload::DownloadFile(std::string object_folder, std::string object_name, std::string localfile_path)
{
	if (object_folder.empty() || object_name.empty())
		return false;

	//����
	BOOL bResult = false;
	if (object_folder.back() != '/')
		object_folder += std::string("/");
	std::string objectfile_path = object_folder + object_name;
	for (int i = 0; i < DF_RETRY_TIMES; i++)
	{
		if (get_s3_object_multipart(objectfile_path, localfile_path))
		{
			bResult = true;
			break;
		}
	}

	return true;
}

bool awsUpload::RemoveFile(std::string objectfile_path)
{
	if (objectfile_path.empty())
		return false;

	//ɾ��
	BOOL bResult = false;
	for (int i = 0; i < DF_RETRY_TIMES; i++)
	{
		if (del_s3_object(objectfile_path))
		{
			bResult = true;
			break;
		}
	}

	return bResult;
}

bool awsUpload::UploadFolder(std::string object_folder, std::string local_folder)
{
	if (object_folder.empty() || local_folder.empty())
		return false;
	
	std::string local_folder_ansi = "";
	utf8_to_ansi(local_folder.c_str(), local_folder.length(), local_folder_ansi);
	std::vector<std::string> veclocalfiles;
	list_localfolder(local_folder_ansi.c_str(), veclocalfiles);//�˺�����������,����·���޷�ö�ٳ��ļ�
	if (veclocalfiles.size() <= 0) return false;

	for (size_t i = 0; i < veclocalfiles.size(); i++)
	{
		std::string urlfile_path;
		std::string localfile_path = veclocalfiles[i];											//�ļ���·������ D:/AAA/BBB/123.mp4
		std::string localfile_folder = get_path_folder(localfile_path);							//�ļ���·������ D:/AAA/BBB

		std::string object_fullfolder = object_folder; 
		utf8_to_ansi(object_fullfolder.c_str(), object_fullfolder.length(), object_fullfolder);
		std::string object_subfolder = str_replace(localfile_folder, local_folder_ansi, "");	//���ļ���·����ɾ����Ŀ¼���� D:/AAA
		if (object_subfolder.length() > 1)														//��󳤶ȴ���1������������ļ��У��� /BBB
			object_fullfolder += object_subfolder;

		ansi_to_utf8(object_fullfolder.c_str(), object_fullfolder.length(), object_fullfolder);
		ansi_to_utf8(localfile_path.c_str(), localfile_path.length(), localfile_path);
		if (UploadFile(object_fullfolder, localfile_path, urlfile_path))
		{
			_debug_to(loger_upload, 0, "[UploadFolder] Upload Success, localfile:%s\n", localfile_path.c_str());
		}
		else
		{
			_debug_to(loger_upload, 1, "[UploadFolder] Upload Failed, localfile:%s\n", localfile_path.c_str());
			return false;
		}
	}

	return true;
}

bool awsUpload::DownloadFolder(std::string object_folder, std::string local_folder)
{
	if (object_folder.empty() || local_folder.empty())
		return false;

	std::string local_folder_ansi = "";
	utf8_to_ansi(local_folder.c_str(), local_folder.length(), local_folder_ansi);
	if (!create_directories(local_folder_ansi.c_str()))
		return false;

	std::vector<std::string> vecobject_path;
	if (list_s3_folder(object_folder, vecobject_path))
	{
		for (size_t i = 0; i < vecobject_path.size(); i++)
		{
			std::string ossfilepath = vecobject_path[i];

			//Ϊ�˲���ָ�������ļ������ٴ���һ�㣬���Ա���ָ���ļ����滻��ossfilepath�׸��ļ���
			std::string root_ossfolder = object_folder;
			if (root_ossfolder.back() != '/') root_ossfolder += std::string("/");

			std::string root_localfolder = local_folder;
			if (root_localfolder.back() != '/') root_localfolder += std::string("/");

			std::string ossfilepath_ansi = "", root_ossfolder_ansi = "", root_localfolder_ansi = "";
			utf8_to_ansi(ossfilepath.c_str(), ossfilepath.length(), ossfilepath_ansi);
			utf8_to_ansi(root_ossfolder.c_str(), root_ossfolder.length(), root_ossfolder_ansi);
			utf8_to_ansi(root_localfolder.c_str(), root_localfolder.length(), root_localfolder_ansi);
			std::string localfile_path = str_replace(ossfilepath_ansi, root_ossfolder_ansi, root_localfolder_ansi);
			std::string localfile_folder = get_path_folder(localfile_path);
			if (!create_directories(localfile_folder.c_str()))
			{
				_debug_to(loger_upload, 1, "[DownloadFolder] CreateFolder Failed, folder:%s\n", localfile_folder.c_str());
				return false;
			}
			ansi_to_utf8(localfile_path.c_str(), localfile_path.length(), localfile_path);
			if (get_s3_object_multipart(vecobject_path[i], localfile_path))
			{
				_debug_to(loger_upload, 0, "[DownloadFolder] Download Success, localfile:%s\n", localfile_path.c_str());
			}
			else
			{
				_debug_to(loger_upload, 1, "[DownloadFolder] Download Failed, localfile:%s\n", localfile_path.c_str());
				return false;
			}
		}
	}
	
	return true;
}

bool awsUpload::RemoveFolder(std::string object_folder)
{
	if (object_folder.empty())
		return false;

	std::vector<std::string> vecobject_path;
	if (list_s3_folder(object_folder, vecobject_path))
	{
		for (size_t i = 0; i < vecobject_path.size(); i++)
		{
			if (del_s3_object(vecobject_path[i]))
			{
				_debug_to(loger_upload, 0, "[DeleteFolder] Delete Success, object path:%s\n", vecobject_path[i].c_str());
			}
			else
			{
				_debug_to(loger_upload, 1, "[DeleteFolder] Delete Failed, object path:%s\n", vecobject_path[i].c_str());
				return false;
			}
		}	
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////
//�ϴ��ļ�
bool awsUpload::put_s3_object(std::string objectfile_path, std::string localfile_path, std::string& urlfile_path)
{
	// �ж��ļ��Ƿ����[��ansi���룬ʹ����ʱ����]
	std::string localfile_path_ansi = "";
	utf8_to_ansi(localfile_path.c_str(), localfile_path.length(), localfile_path_ansi);
	if (!file_exists(localfile_path_ansi))
	{
		_debug_to(loger_upload, 1, ("[put_s3_object]: file is not exist...[ %s ]\n"), localfile_path.c_str());
		return false;
	}

	//��������
	Aws::String aws_Endpoint(m_strEndPoint.c_str(), m_strEndPoint.size());
	Aws::String aws_AK(m_strAK.c_str(), m_strAK.size());
	Aws::String aws_SK(m_strSK.c_str(), m_strSK.size());
	Aws::String aws_bucket(m_strBucket.c_str(), m_strBucket.size());
	Aws::String aws_object_path(objectfile_path.c_str(), objectfile_path.size());
	Aws::String aws_region(m_strRegion.c_str(), m_strRegion.size());
	_debug_to(loger_upload, 0, ("[put_s3_object] Parameter: Endpoint:%s, AK:%s, SK:%s, Bucket:%s, ObjPath:%s, Region:%s\n"), m_strEndPoint.c_str(), m_strAK.c_str(), m_strSK.c_str(), m_strBucket.c_str(), objectfile_path.c_str(), m_strRegion.c_str());

	//���ָ���˵���
	Aws::Client::ClientConfiguration cfg;
	if (!aws_region.empty())
		cfg.region = aws_region;

	//S3����
	cfg.endpointOverride = aws_Endpoint;  // S3��������ַ�Ͷ˿�
	cfg.scheme = Aws::Http::Scheme::HTTP;
	cfg.verifySSL = false;

	Aws::Auth::AWSCredentials cred(aws_AK, aws_SK);  // ak,sk
	Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy signPayloads = Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never;
	Aws::S3::S3Client s3_client(cred, cfg, signPayloads, false);

#if 1 // �ϴ��ļ�
	Aws::S3::Model::PutObjectRequest put_object_request;
	const std::shared_ptr<Aws::IOStream> input_data = Aws::MakeShared<Aws::FStream>("SampleAllocationTag", localfile_path_ansi.c_str(), std::ios_base::in | std::ios_base::binary);
	put_object_request.SetBucket(aws_bucket);
	put_object_request.SetKey(aws_object_path);
	put_object_request.SetBody(input_data);

	auto put_object_outcome = s3_client.PutObject(put_object_request);
	if (put_object_outcome.IsSuccess())
	{
		//�洢�ļ��Թ�URL
		urlfile_path = m_strWebPrefix + m_strUrl + std::string("/") + objectfile_path;
		urlfile_path = url_encode(urlfile_path); //����
		_debug_to(loger_upload, 0, "[put_s3_object] Upload Success, url:%s\n", urlfile_path.c_str());
	}
	else
	{
		Aws::S3::S3Error error = put_object_outcome.GetError();
		Aws::String aws_ExceptionName = error.GetExceptionName();
		Aws::String aws_Message = error.GetMessage();
		std::string sExceptionName(aws_ExceptionName.c_str(), aws_ExceptionName.size());
		std::string sMessage(aws_Message.c_str(), aws_Message.size());
		_debug_to(loger_upload, 1, "[put_s3_object] Upload Failed, Exception:%s, Message:%s\n", sExceptionName.c_str(), sMessage.c_str());
		return false;
	}
#endif

	return true;
}
//�ֶ��ϴ�
bool awsUpload::put_s3_object_multipart(std::string objectfile_path, std::string localfile_path, std::string& urlfile_path)
{
	// �ж��ļ��Ƿ����[��ansi���룬ʹ����ʱ����]
	std::string localfile_path_ansi = "";
	utf8_to_ansi(localfile_path.c_str(), localfile_path.length(), localfile_path_ansi);
	if (!file_exists(localfile_path_ansi))
	{
		_debug_to(loger_upload, 1, ("[put_s3_object_multipart]: file is not exist...[ %s ]\n"), localfile_path.c_str());
		return false;
	}

	//��������
	Aws::String aws_Endpoint(m_strEndPoint.c_str(), m_strEndPoint.size());
	Aws::String aws_AK(m_strAK.c_str(), m_strAK.size());
	Aws::String aws_SK(m_strSK.c_str(), m_strSK.size());
	Aws::String aws_bucket(m_strBucket.c_str(), m_strBucket.size());
	Aws::String aws_object_path(objectfile_path.c_str(), objectfile_path.size());
	Aws::String aws_region(m_strRegion.c_str(), m_strRegion.size());
	_debug_to(loger_upload, 0, ("[put_s3_object_multipart] Parameter: Endpoint:%s, AK:%s, SK:%s, Bucket:%s, ObjPath:%s, Region:%s\n"), m_strEndPoint.c_str(), m_strAK.c_str(), m_strSK.c_str(), m_strBucket.c_str(), objectfile_path.c_str(), m_strRegion.c_str());

	//���ָ���˵���
	Aws::Client::ClientConfiguration cfg;
	if (!aws_region.empty())
		cfg.region = aws_region;

	//S3����
	cfg.endpointOverride = aws_Endpoint;  // S3��������ַ�Ͷ˿�
	cfg.scheme = Aws::Http::Scheme::HTTP;
	cfg.verifySSL = false;

	Aws::Auth::AWSCredentials cred(aws_AK, aws_SK);  // ak,sk
	Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy signPayloads = Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never;
	Aws::S3::S3Client s3_client(cred, cfg, signPayloads, false);

#if 1 // ��Ƭ�ϴ��ļ�

	//1. init
	Aws::String aws_upload_id;
	Aws::S3::Model::CreateMultipartUploadRequest create_multipart_request;
	create_multipart_request.WithBucket(aws_bucket).WithKey(aws_object_path);
	auto createMultiPartResult = s3_client.CreateMultipartUpload(create_multipart_request);
	if (createMultiPartResult.IsSuccess())
	{
		aws_upload_id = createMultiPartResult.GetResult().GetUploadId();
		_debug_to(loger_upload, 0, ("[put_s3_object_multipart]: Create MultipartUpload Success, UploadID: %s\n"), aws_upload_id.c_str());
	}
	else
	{
		Aws::S3::S3Error error = createMultiPartResult.GetError();
		Aws::String aws_ExceptionName = error.GetExceptionName();
		Aws::String aws_Message = error.GetMessage();
		std::string sExceptionName(aws_ExceptionName.c_str(), aws_ExceptionName.size());
		std::string sMessage(aws_Message.c_str(), aws_Message.size());
		_debug_to(loger_upload, 1, ("[put_s3_object_multipart]: Create MultipartUpload Failed, Exception:%s, Message:%s\n"), sExceptionName.c_str(), sMessage.c_str());
		return false;
	}

	//2. uploadPart
	long partSize = 5 * 1024 * 1024;//5M
	std::fstream file(localfile_path_ansi.c_str(), std::ios::in | std::ios::binary);//��ȡ�ļ�[��ansi���룬ʹ����ʱ����]
	file.seekg(0, std::ios::end);
	long fileSize = file.tellg();
	//std::cout << file.tellg() << std::endl;
	file.seekg(0, std::ios::beg);

	long filePosition = 0;
	Aws::Vector<Aws::S3::Model::CompletedPart> completeParts;
	char* buffer = new char[partSize]; std::memset(buffer, 0, partSize);

	int partNumber = 1;
	while (filePosition < fileSize)
	{
		partSize = min(partSize, (fileSize - filePosition));
		file.read(buffer, partSize);
		//std::cout << "readSize : " << partSize << std::endl;
		Aws::S3::Model::UploadPartRequest upload_part_request;
		upload_part_request.WithBucket(aws_bucket).WithKey(aws_object_path).WithUploadId(aws_upload_id).WithPartNumber(partNumber).WithContentLength(partSize);

		Aws::String str(buffer, partSize);
		auto input_data = Aws::MakeShared<Aws::StringStream>("UploadPartStream", str);
		upload_part_request.SetBody(input_data);
		filePosition += partSize;

		auto uploadPartResult = s3_client.UploadPart(upload_part_request);
		//std::cout << uploadPartResult.GetResult().GetETag() << std::endl;
		completeParts.push_back(Aws::S3::Model::CompletedPart().WithETag(uploadPartResult.GetResult().GetETag()).WithPartNumber(partNumber));
		std::memset(buffer, 0, partSize);
		++partNumber;
	}

	if (fileSize > 0 && filePosition > 0)
	{
		_debug_to(loger_upload, 0, ("[put_s3_object_multipart]: CompletedPart Success, FileSize: %dM, PartCount:%d\n"), fileSize / (1024 * 1024), partNumber);
	}
	else
	{
		_debug_to(loger_upload, 1, ("[put_s3_object_multipart]: CompletedPart Falied, FileSize: %dM, PartCount:%d\n"), fileSize / (1024 * 1024), partNumber);
	}

	//3. complete multipart upload
	Aws::S3::Model::CompleteMultipartUploadRequest complete_multipart_request;
	complete_multipart_request.WithBucket(aws_bucket).WithKey(aws_object_path).WithUploadId(aws_upload_id).WithMultipartUpload(Aws::S3::Model::CompletedMultipartUpload().WithParts(completeParts));
	auto completeMultipartUploadResult = s3_client.CompleteMultipartUpload(complete_multipart_request);
	if (completeMultipartUploadResult.IsSuccess())
	{
		//�洢�ļ��Թ�URL
		urlfile_path = m_strWebPrefix + m_strUrl + std::string("/") + objectfile_path;
		urlfile_path = url_encode(urlfile_path); //����
		_debug_to(loger_upload, 0, "[put_s3_object_multipart] Upload Success, url:%s\n", urlfile_path.c_str());
	}
	else
	{
		//�ж��ϴ�
		Aws::S3::Model::AbortMultipartUploadRequest abort_multipart_request;
		abort_multipart_request.WithBucket(aws_bucket).WithKey(aws_object_path).WithUploadId(aws_upload_id);
		s3_client.AbortMultipartUpload(abort_multipart_request);

		//������Ϣ
		Aws::S3::S3Error error = completeMultipartUploadResult.GetError();
		Aws::String aws_ExceptionName = error.GetExceptionName();
		Aws::String aws_Message = error.GetMessage();
		std::string sExceptionName(aws_ExceptionName.c_str(), aws_ExceptionName.size());
		std::string sMessage(aws_Message.c_str(), aws_Message.size());

		_debug_to(loger_upload, 1, "[put_s3_object] Upload Failed, Exception:%s, Message:%s\n", sExceptionName.c_str(), sMessage.c_str());
		return false;
	}

#endif

	return true;
}
//�����ļ�
bool awsUpload::get_s3_object(std::string objectfile_path, std::string localfile_path)
{
	//��������
	Aws::String aws_Endpoint(m_strEndPoint.c_str(), m_strEndPoint.size());
	Aws::String aws_AK(m_strAK.c_str(), m_strAK.size());
	Aws::String aws_SK(m_strSK.c_str(), m_strSK.size());
	Aws::String aws_bucket(m_strBucket.c_str(), m_strBucket.size());
	Aws::String aws_object_path(objectfile_path.c_str(), objectfile_path.size());
	Aws::String aws_region(m_strRegion.c_str(), m_strRegion.size());
	_debug_to(loger_upload, 0, ("[get_s3_object] Parameter: Endpoint:%s, AK:%s, SK:%s, Bucket:%s, ObjPath:%s, Region:%s\n"), m_strEndPoint.c_str(), m_strAK.c_str(), m_strSK.c_str(), m_strBucket.c_str(), objectfile_path.c_str(), m_strRegion.c_str());

	//���ָ���˵���
	Aws::Client::ClientConfiguration cfg;
	if (!aws_region.empty())
		cfg.region = aws_region;

	//S3����
	cfg.endpointOverride = aws_Endpoint;  // S3��������ַ�Ͷ˿�
	cfg.scheme = Aws::Http::Scheme::HTTP;
	cfg.verifySSL = false;

	Aws::Auth::AWSCredentials cred(aws_AK, aws_SK);  // ak,sk
	Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy signPayloads = Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never;
	Aws::S3::S3Client s3_client(cred, cfg, signPayloads, false);

#if 1 //�����ļ�

	//��������
	Aws::S3::Model::GetObjectRequest getObjectRequest;
	getObjectRequest.SetBucket(aws_bucket);
	getObjectRequest.SetKey(aws_object_path);

	//����Object
	Aws::S3::Model::GetObjectOutcome getObjectOutcome = s3_client.GetObject(getObjectRequest);
	if (getObjectOutcome.IsSuccess()) 
	{
		//���浽����
		std::string localfile_path_ansi = "";
		utf8_to_ansi(localfile_path.c_str(), localfile_path.length(), localfile_path_ansi);
		std::ofstream outputFileStream(localfile_path_ansi, std::ios::binary);
		if (!outputFileStream.is_open())
		{
			_debug_to(loger_upload, 0, "[get_s3_object] OpenFile Failed, local:%s\n", localfile_path.c_str());
			return false;
		}
		outputFileStream << getObjectOutcome.GetResult().GetBody().rdbuf();
		outputFileStream.close();
		_debug_to(loger_upload, 0, "[get_s3_object] Download Success, local:%s\n", localfile_path.c_str());
	}
	else
	{
		//������Ϣ
		Aws::S3::S3Error error = getObjectOutcome.GetError();
		Aws::String aws_ExceptionName = error.GetExceptionName();
		Aws::String aws_Message = error.GetMessage();
		std::string sExceptionName(aws_ExceptionName.c_str(), aws_ExceptionName.size());
		std::string sMessage(aws_Message.c_str(), aws_Message.size());

		_debug_to(loger_upload, 1, "[get_s3_object] Download Failed, Exception:%s, Message:%s\n", sExceptionName.c_str(), sMessage.c_str());
		return false;
	}

#endif

	return true;
}
//�ֶ�����
bool awsUpload::get_s3_object_multipart(const std::string objectfile_path, const std::string localfile_path)
{
	//��������
	Aws::String aws_Endpoint(m_strEndPoint.c_str(), m_strEndPoint.size());
	Aws::String aws_AK(m_strAK.c_str(), m_strAK.size());
	Aws::String aws_SK(m_strSK.c_str(), m_strSK.size());
	Aws::String aws_bucket(m_strBucket.c_str(), m_strBucket.size());
	Aws::String aws_object_path(objectfile_path.c_str(), objectfile_path.size());
	Aws::String aws_region(m_strRegion.c_str(), m_strRegion.size());
	_debug_to(loger_upload, 0, ("[get_s3_object_multipart] Parameter: Endpoint:%s, AK:%s, SK:%s, Bucket:%s, ObjPath:%s, Region:%s\n"), m_strEndPoint.c_str(), m_strAK.c_str(), m_strSK.c_str(), m_strBucket.c_str(), objectfile_path.c_str(), m_strRegion.c_str());

	//���ָ���˵���
	Aws::Client::ClientConfiguration cfg;
	if (!aws_region.empty())
		cfg.region = aws_region;

	//S3����
	cfg.endpointOverride = aws_Endpoint;  // S3��������ַ�Ͷ˿�
	cfg.scheme = Aws::Http::Scheme::HTTP;
	cfg.verifySSL = false;

	Aws::Auth::AWSCredentials cred(aws_AK, aws_SK);  // ak,sk
	Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy signPayloads = Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never;
	Aws::S3::S3Client s3_client(cred, cfg, signPayloads, false);

#if 1 //�����ļ�

	//��������
	Aws::S3::Model::GetObjectRequest getObjectRequest;
	getObjectRequest.SetBucket(aws_bucket);
	getObjectRequest.SetKey(aws_object_path);

	//����Object
	Aws::S3::Model::GetObjectOutcome getObjectOutcome = s3_client.GetObject(getObjectRequest);
	if (getObjectOutcome.IsSuccess())
	{
		// ��ȡ����Ĵ�С
		std::string localfile_path_ansi = "";
		utf8_to_ansi(localfile_path.c_str(), localfile_path.length(), localfile_path_ansi);
		long long object_size = getObjectOutcome.GetResult().GetContentLength();
		std::ofstream outputFileStream(localfile_path_ansi, std::ios::binary);
		if (!outputFileStream.is_open())
		{
			_debug_to(loger_upload, 0, "[get_s3_object_multipart] OpenFile Failed, local:%s\n", localfile_path.c_str());
			return false;
		}

		// �ֶ����ز�д�뱾���ļ�
		long long part_size = 1024 * 1024;									// ÿ���ֶεĴ�С����������Ϊ 1MB
		long long num_parts = (object_size + part_size - 1) / part_size;	// �ֶε�����
		for (long long i = 0; i < num_parts; i++)
		{
			long long start_byte = i * part_size;
			long long end_byte = (i + 1) * part_size - 1;
			if (end_byte >= object_size)
				end_byte = object_size - 1;

			// ����GetObjectRequest��ָ�����صķ�Χ
			Aws::S3::Model::GetObjectRequest partGetObjectRequest;
			partGetObjectRequest.SetBucket(aws_bucket);
			partGetObjectRequest.SetKey(aws_object_path);
			partGetObjectRequest.SetRange("bytes=" + std::to_string(start_byte) + "-" + std::to_string(end_byte));

			// ���طֶ�
			auto partGetObjectOutcome = s3_client.GetObject(partGetObjectRequest);
			if (!partGetObjectOutcome.IsSuccess())
			{
				//������Ϣ
				Aws::S3::S3Error error = partGetObjectOutcome.GetError();
				Aws::String aws_ExceptionName = error.GetExceptionName();
				Aws::String aws_Message = error.GetMessage();
				std::string sExceptionName(aws_ExceptionName.c_str(), aws_ExceptionName.size());
				std::string sMessage(aws_Message.c_str(), aws_Message.size());

				_debug_to(loger_upload, 1, "[get_s3_object_multipart] Part Download Failed, Exception:%s, Message:%s\n", sExceptionName.c_str(), sMessage.c_str());
				return false;
			}

			// ���ֶ�����д�뱾���ļ�
			const Aws::IOStream& part_data = partGetObjectOutcome.GetResult().GetBody();
			outputFileStream << part_data.rdbuf();
		}
		outputFileStream.close();
		_debug_to(loger_upload, 0, "[get_s3_object_multipart] Download Success, local:%s\n", localfile_path.c_str());
	}
	else
	{
		//������Ϣ
		Aws::S3::S3Error error = getObjectOutcome.GetError();
		Aws::String aws_ExceptionName = error.GetExceptionName();
		Aws::String aws_Message = error.GetMessage();
		std::string sExceptionName(aws_ExceptionName.c_str(), aws_ExceptionName.size());
		std::string sMessage(aws_Message.c_str(), aws_Message.size());

		_debug_to(loger_upload, 1, "[get_s3_object_multipart] Download Failed, Exception:%s, Message:%s\n", sExceptionName.c_str(), sMessage.c_str());
		return false;
	}

#endif

	return true;
}
//ɾ���ļ�
bool awsUpload::del_s3_object(std::string objectfile_path)
{
	//��������
	Aws::String aws_Endpoint(m_strEndPoint.c_str(), m_strEndPoint.size());
	Aws::String aws_AK(m_strAK.c_str(), m_strAK.size());
	Aws::String aws_SK(m_strSK.c_str(), m_strSK.size());
	Aws::String aws_bucket(m_strBucket.c_str(), m_strBucket.size());
	Aws::String aws_object_path(objectfile_path.c_str(), objectfile_path.size());
	Aws::String aws_region(m_strRegion.c_str(), m_strRegion.size());
	_debug_to(loger_upload, 0, ("[del_s3_object] Parameter: Endpoint:%s, AK:%s, SK:%s, Bucket:%s, ObjPath:%s, Region:%s\n"), m_strEndPoint.c_str(), m_strAK.c_str(), m_strSK.c_str(), m_strBucket.c_str(), objectfile_path.c_str(), m_strRegion.c_str());

	//���ָ���˵���
	Aws::Client::ClientConfiguration cfg;
	if (!aws_region.empty())
		cfg.region = aws_region;

	//S3����
	cfg.endpointOverride = aws_Endpoint;  // S3��������ַ�Ͷ˿�
	cfg.scheme = Aws::Http::Scheme::HTTP;
	cfg.verifySSL = false;

	Aws::Auth::AWSCredentials cred(aws_AK, aws_SK);  // ak,sk
	Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy signPayloads = Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never;
	Aws::S3::S3Client s3_client(cred, cfg, signPayloads, false);

#if 1 //ɾ���ļ�

	// ����ɾ����������
	Aws::S3::Model::DeleteObjectRequest deleteObjectRequest;
	deleteObjectRequest.SetBucket(aws_bucket);
	deleteObjectRequest.SetKey(aws_object_path);

	// ����ɾ����������
	auto deleteObjectOutcome = s3_client.DeleteObject(deleteObjectRequest);

	// ���ɾ�����
	if (deleteObjectOutcome.IsSuccess())
	{
		_debug_to(loger_upload, 0, "[del_s3_object] Delete Success, object path:%s\n", aws_object_path.c_str());
	}
	else
	{
		//������Ϣ
		Aws::S3::S3Error error = deleteObjectOutcome.GetError();
		Aws::String aws_ExceptionName = error.GetExceptionName();
		Aws::String aws_Message = error.GetMessage();
		std::string sExceptionName(aws_ExceptionName.c_str(), aws_ExceptionName.size());
		std::string sMessage(aws_Message.c_str(), aws_Message.size());

		_debug_to(loger_upload, 1, "[del_s3_object] Delete Failed, Exception:%s, Message:%s\n", sExceptionName.c_str(), sMessage.c_str());
		return false;
	}

#endif

	return true;
}

//ö��OSSĿ¼
bool awsUpload::list_s3_folder(std::string object_folder, std::vector<std::string>& vecobject_name)
{
	//��������
	Aws::String aws_Endpoint(m_strEndPoint.c_str(), m_strEndPoint.size());
	Aws::String aws_AK(m_strAK.c_str(), m_strAK.size());
	Aws::String aws_SK(m_strSK.c_str(), m_strSK.size());
	Aws::String aws_bucket(m_strBucket.c_str(), m_strBucket.size());
	Aws::String aws_region(m_strRegion.c_str(), m_strRegion.size());
	_debug_to(loger_upload, 0, ("[list_s3_folder] Parameter: Endpoint:%s, AK:%s, SK:%s, Bucket:%s, Region:%s\n"), m_strEndPoint.c_str(), m_strAK.c_str(), m_strSK.c_str(), m_strBucket.c_str(), m_strRegion.c_str());

	//���ָ���˵���
	Aws::Client::ClientConfiguration cfg;
	if (!aws_region.empty())
		cfg.region = aws_region;

	//S3����
	cfg.endpointOverride = aws_Endpoint;  // S3��������ַ�Ͷ˿�
	cfg.scheme = Aws::Http::Scheme::HTTP;
	cfg.verifySSL = false;

	Aws::Auth::AWSCredentials cred(aws_AK, aws_SK);  // ak,sk
	Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy signPayloads = Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never;
	Aws::S3::S3Client s3_client(cred, cfg, signPayloads, false);

#if 1 //ö��Ŀ¼

	//��������
	Aws::S3::Model::ListObjectsRequest objectsRequest;
	objectsRequest.SetBucket(aws_bucket);

	std::string s_prefix = object_folder;
	if (s_prefix.back() != '/')
		s_prefix += std::string("/"); //��ֻ֤������ǰ�ļ���,���������ļ���
	Aws::String aws_prefix(s_prefix.c_str(), s_prefix.size());
	objectsRequest.SetPrefix(aws_prefix);

	//����Object
	Aws::S3::Model::ListObjectsOutcome list_objects_outcome = s3_client.ListObjects(objectsRequest);
	if (list_objects_outcome.IsSuccess())
	{
		Aws::Vector<Object> objects = list_objects_outcome.GetResult().GetContents();
		for (const auto& object : objects)
		{
			if (object.GetKey().back() == '/') // If it's a directory
			{
				continue;
			}
			else // If it's a file
			{
				std::string object_name = object.GetKey();
				vecobject_name.push_back(object_name);
			}
		}
	}
	else
	{
		//������Ϣ
		Aws::S3::S3Error error = list_objects_outcome.GetError();
		Aws::String aws_ExceptionName = error.GetExceptionName();
		Aws::String aws_Message = error.GetMessage();
		std::string sExceptionName(aws_ExceptionName.c_str(), aws_ExceptionName.size());
		std::string sMessage(aws_Message.c_str(), aws_Message.size());

		_debug_to(loger_upload, 1, "[list_s3_folder] Failed to list objects, bucket: %s, prefix: %s, Exception:%s, Message:%s\n", aws_bucket.c_str(), aws_prefix.c_str(), sExceptionName.c_str(), sMessage.c_str());
		return false;
	}

#endif

	return true;
}


