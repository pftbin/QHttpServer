#pragma once
#include "public.h"  //�����ԣ��������h�ļ��У�������벻��

#include <string>
#include <opencv2/opencv.hpp>
#include <iostream>

using namespace cv;
using namespace std;

bool getimage_fromvideo(std::string videofilepath, std::string& imagefilepath, unsigned int& videowidth, unsigned int& videoheight);