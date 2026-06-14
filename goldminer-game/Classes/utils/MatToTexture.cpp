#include "MatToTexture.hpp"
#include <opencv2/imgproc.hpp>

USING_NS_CC;

Texture2D* MatToTexture::convert(const cv::Mat& mat) {
    if (mat.empty()) return nullptr;

    cv::Mat rgb;
    switch (mat.channels()) {
        case 3:
            cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
            break;
        case 4:
            cv::cvtColor(mat, rgb, cv::COLOR_BGRA2RGBA);
            break;
        case 1:
            cv::cvtColor(mat, rgb, cv::COLOR_GRAY2RGB);
            break;
        default:
            return nullptr;
    }

    auto image = new Image();
    image->initWithRawData(rgb.data, rgb.total() * rgb.elemSize(),
                           rgb.cols, rgb.rows, rgb.channels() == 4 ? 32 : 24);
    auto texture = Director::getInstance()->getTextureCache()->addImage(image, "");
    image->release();
    return texture;
}
