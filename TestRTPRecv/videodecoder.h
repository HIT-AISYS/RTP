#ifndef VIDEODECODER_H
#define VIDEODECODER_H

#include <QObject>
#include <QWidget>
#include <QDebug>
#include <QTime>
#include <QThread>
#include "rtp.h"
#include "fifobuffer.h"
#include "ffmpegheader.h"
#include "SDL.h"
#include "mediaoutput.h"

class VideoDecoder : public QThread
{
    Q_OBJECT
public:
    explicit VideoDecoder(QObject *parent = nullptr);
    ~VideoDecoder();

    void setRunState(bool start);

    //设置解码参数
    void initDecodeParm(void *showId, int width, int height);

    //累积解析RTP数据 得到AVPacket
    AVPacket *parseRtpData(char *data, int size);
    AVPacket *getAvPacketFromRtp(char *data, int size, int timeStamp);

protected:
    void run();

public:
    bool isRun;

    //完整RTP包
    char *entireRtpPacket;
    int entireLength;

    //视频宽高
    int width;
    int height;

    //视频输出
    VideoOutput *videoOutput;

    //引用外部队列指针
    FiFoBuffer *fifoBuffer;
};

#endif // VIDEODECODER_H
