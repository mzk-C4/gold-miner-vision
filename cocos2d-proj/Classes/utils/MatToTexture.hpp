#pragma once

#include "cocos2d.h"
#include <opencv2/core.hpp>

class MatToTexture {
public:
    /// Convert OpenCV BGR/RGB Mat to Cocos2d-x Texture2D
    static cocos2d::Texture2D* convert(const cv::Mat& mat);
};
