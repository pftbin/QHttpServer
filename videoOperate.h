#pragma once
#include "public.h"  //经测试，必须放在h文件中，否则编译不过

#include <string>
#include <opencv2/opencv.hpp>
#include <iostream>

using namespace cv;
using namespace std;

bool getimage_fromvideo(std::string videofilepath, std::string& imagefilepath, unsigned int& videowidth, unsigned int& videoheight);