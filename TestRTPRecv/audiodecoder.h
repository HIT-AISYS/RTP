#ifndef AUDIODECODER_H
#define AUDIODECODER_H

#include <QThread>
#include <QDebug>
#include <QTime>
#include "rtp.h"
#include "fifobuffer.h"
#include "ffmpegheader.h"
#include "mediaoutput.h"

class AudioDecoder : public QThread
{
    Q_OBJECT
public:
    explicit AudioDecoder(QObject *parent = nullptr);
    ~AudioDecoder();

    void setRunState(bool start);

    //设置音频参数 ffmpeg类型
    void initDecodeParm(int sampleRate, int sampleFmt, int channels);

    //解析RTP数据返回AVpacket
    AVPacket *parseRtpData(char *data, int size);

protected:
    void run();

public:
    bool isRun;

    //完整RTP包
    char *entireRtpPacket;
    int entireLength;

    //音频参数
    int sampleRate;
    int sampleFmt;
    int channels;

    //音频输出
    AudioOutput *audioOutput;

    //引用外部队列指针
    FiFoBuffer *fifoBuffer;
};

#endif // AUDIODECODER_H
