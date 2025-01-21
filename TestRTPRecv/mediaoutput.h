#ifndef MEDIAOUTPUT_H
#define MEDIAOUTPUT_H

#include <QAudioFormat>
#include <QAudioOutput>
#include <QDebug>
#include "ffmpegheader.h"
#include "SDL.h"

//视频输出类 SDL
class VideoOutput
{
public:
    VideoOutput();
    ~VideoOutput();

    //只显示不负责释放 AVFrame为YUV420P格式
    void startDisplay(AVFrame *videoFrame);
    void stopDisplay();

    //设置渲染窗口句柄
    void setShowHandle(void *id);
    //设置视频宽、高、渲染方式
    bool setVideoParm(int width, int height, bool cpuMode = true);
    //释放
    void release();

private:
    //初始化状态
    bool hasInit;

    //用到的相关变量
    SDL_Window *sdlWindow;
    SDL_Renderer *sdlRender;
    SDL_Texture *sdlTexture;

    //显示的句柄
    void *showId;
};


//音频输出类 Qt自带
class AudioOutput
{
public:
    AudioOutput();
    ~AudioOutput();

    //只播放不负责释放 AVFrame为解码后音频帧
    void startPlay(AVFrame *audioFrame);

    //设置音频参数 采样率 采样格式 通道数
    bool setAudioParm(int sampleRate, int sampleFmt, int channels);
    //释放
    void release();

private:
    //初始化状态
    bool hasInit;

    //音频参数
    int sampleRate;
    int sampleSize;
    int channels;

    //设备输出
    QAudioOutput *audioOutput;
    QIODevice *audioDevice;

    //重采样
    SwrContext *audioSwrCtx;
    uint8_t *audioData;
};

#endif // MEDIAOUTPUT_H
