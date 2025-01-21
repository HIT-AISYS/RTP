#ifndef MEDIACAPTURER_H
#define MEDIACAPTURER_H

#include <QAudioInput>
#include <QThread>
#include <QDebug>
#include <QTime>
#include "ffmpegheader.h"
#include "rtpsend.h"
#include "mediaoutput.h"


//摄像头视频捕捉类
class VideoCapturer : public QThread
{
public:
    VideoCapturer();
    ~VideoCapturer();

    //设置运行状态
    void setRunState(bool start);

protected:
    void run();

public:
    //外部RTP发送者指针
    RtpPacketSender *rtpSender;
    VideoOutput *videoOutput;
private:
    volatile bool isRun;
};

//麦克风音频捕捉
class AudioCapturer : public QThread
{
public:
    AudioCapturer();
    ~AudioCapturer();

    //设置运行状态
    void setRunState(bool start);

protected:
    void run();

public:
    //外部RTP发送者指针
    RtpPacketSender *rtpSender;

private:
    volatile bool isRun;
};

#endif // MEDIACAPTURER_H
