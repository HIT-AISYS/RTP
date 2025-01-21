#ifndef FFMPEGHEADER_H
#define FFMPEGHEADER_H

/**
 * 封装常用的ffmpeg方法以及类 只需要引入文件即可直接使用 避免重复轮子
 * By ZXT
 */

extern "C"
{
#include "./libavcodec/avcodec.h"
#include "./libavformat/avformat.h"
#include "./libavformat/avio.h"
#include "./libavutil/opt.h"
#include "./libavutil/time.h"
#include "./libavutil/imgutils.h"
#include "./libswscale/swscale.h"
#include "./libswresample/swresample.h"
#include "./libavutil/avutil.h"
#include "./libavutil/ffversion.h"
#include "./libavutil/frame.h"
#include "./libavutil/pixdesc.h"
#include "./libavutil/imgutils.h"
#include "./libavfilter/avfilter.h"
#include "./libavfilter/buffersink.h"
#include "./libavfilter/buffersrc.h"
#include "./libavdevice/avdevice.h"
}

#include <QDebug>

/************************************* 常用函数封装 ************************************/
//获取ffmpeg报错信息
char *getAVError(int errNum);

//根据pts计算实际时间us
int64_t getRealTimeByPTS(int64_t pts, AVRational timebase);
//pts转换为us差时进行延时
void calcAndDelay(int64_t startTime, int64_t pts, AVRational timebase);


/************************************* 常用类封装 *************************************/
//视频图像转换类
class VideoSwser
{
public:
    VideoSwser();
    ~VideoSwser();

    //初始化转换器
    bool initSwsCtx(int srcWidth, int srcHeight, AVPixelFormat srcFmt, int dstWidth, int dstHeight, AVPixelFormat dstFmt);
    void release();

    //返回转换格式后的AVFrame
    AVFrame *getSwsFrame(AVFrame *srcFrame);

private:
    bool hasInit;
    bool needSws;

    int dstWidth;
    int dstHeight;
    AVPixelFormat dstFmt;

    //格式转换
    SwsContext *videoSwsCtx;
};

//视频编码器类
class VideoEncoder
{
public:
    VideoEncoder();
    ~VideoEncoder();

    //初始化编码器
    bool initEncoder(int width, int height, AVPixelFormat fmt, int fps);
    void release();

    //返回编码后AVpacket
    AVPacket *getEncodePacket(AVFrame *srcFrame);

    //计算时间戳
    void calcTimeStamp();

private:
    bool hasInit;
    int64_t startTime;

    //时间戳
    int duration;
    int64_t timeStamp;

    //编码器
    AVCodecContext *videoEnCodecCtx;
};


//音频重采样类
class AudioSwrer
{
public:
    AudioSwrer();
    ~AudioSwrer();

    //初始化转换器
    bool initSwrCtx(int inChannels, int inSampleRate, AVSampleFormat inFmt, int outChannels, int outSampleRate, AVSampleFormat outFmt);
    void release();

    //返回转换格式后的AVFrame
    AVFrame *getSwrFrame(AVFrame *srcFrame);
    //返回转换格式后的AVFrame srcdata为一帧源格式的数据
    AVFrame *getSwrFrame(uint8_t *srcData);

private:
    bool hasInit;
    bool needSwr;

    int outChannels;
    int outSampleRate;
    AVSampleFormat outFmt;

    //格式转换
    SwrContext *audioSwrCtx;
};

//音频编码器类
class AudioEncoder
{
public:
    AudioEncoder();
    ~AudioEncoder();

    //初始化编码器
    bool initEncoder(int channels, int sampleRate, AVSampleFormat sampleFmt);
    void release();

    //返回编码后AVpacket
    AVPacket *getEncodePacket(AVFrame *srcFrame);

    //计算时间戳
    void calcTimeStamp();

private:
    bool hasInit;
    int64_t startTime;

    //时间戳
    int duration;
    int64_t timeStamp;

    //编码器
    AVCodecContext *audioEnCodecCtx;
};



#endif // FFMPEGHEADER_H
