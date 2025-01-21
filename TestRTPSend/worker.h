#ifndef WORKER_H
#define WORKER_H

#include <QThread>
#include <QEventLoop>
#include <QTimer>
#include "mediaoutput.h"
#include "rtpsend.h"
#include "mediacapturer.h"

class Worker : public QThread
{
    Q_OBJECT
public:
    explicit Worker();
    ~Worker();

    void msSleep(int ms);
    void setFileName(const QString &name);
    void setRunState(bool start);

    //解析得到sps pps
    void parseSPS_PPS(AVCodecContext *videoCtx);

    //avPacket传递至RTP发送线程 type为音视频类型
    void sendPacketToRtpSender(AVPacket *packet, AVPacketType type);

    //文件音视频发送
    void doTaskFile();
    //实时音视频发送
    void doTaskRealStart();
    void doTaskRealStop();

protected:
    void run();

public:
    VideoOutput *videoOutput;
    AudioOutput *audioOutput;
    RtpPacketSender *rtpSender;
    VideoCapturer *videoCapturer;
    AudioCapturer *audioCapturer;

private:
    bool isRun;
    QString fileName;

    //sps pps
    char *spsFrame;
    int spsLen;
    char *ppsFrame;
    int ppsLen;
};

#endif // WORKER_H
