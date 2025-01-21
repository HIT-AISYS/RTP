#ifndef RTPSEND_H
#define RTPSEND_H

#include <QThread>
#include <QUdpSocket>
#include <QTcpSocket>
#include <QMutexLocker>
#include <list>
#include "rtp.h"
#include "ffmpegheader.h"

//rtp目的地址
const QString destIP = "192.168.1.110";
const uint16_t destUDPVideoPort = 8554;
const uint16_t destUDPAudioPort = 8555;
const uint16_t destTCPPort = 8888;

//十六进制字节数组转十进制长度值
int32_t hexArrayToDec(char *array, int len);

struct RTPMediaInfo
{
    int length;  //信息体长度
    int width;
    int height;
    int fps;
    int sampleRate;
    int channel;
    int format;
    short hasVideo;
    short hasAudio;
    short cmd;  //1开始 2结束
};

enum AVPacketType
{
    //区分音视频以及是否带有起始字符
    PacketAudio = 0,
    PacketVideo,
    PacketAudioStart,
    PacketVideoStart
};

struct ReadyPacket
{
    AVPacketType type;
    AVPacket *packet;
};

//Rtp发送线程
class RtpPacketSender : public QThread
{
    Q_OBJECT
public:
    explicit RtpPacketSender();
    ~RtpPacketSender();

    void setRunState(bool start);

    //设置传输类型
    void setTransType(bool real);

    //rtp音视频时间戳
    void getRTPVideoTimeStamp(int64_t pts);
    void getRTPAudioTimeStamp(int64_t pts);

    //设置sps pps信息
    void setSPS_PPS(char *spsFrame, int spsLen, char *ppsFrame, int ppsLen);

    //释放队列
    void release();
    //音、视频包的写入
    void writePacket(ReadyPacket *packet);

    //解析H264组包RTP发送
    void parseH264PacketData(RtpPacket *rtpPacket, char *packetData, int packetSize, int64_t pts = 0);
    void parseH264PacketDataWithStartCode(RtpPacket *rtpPacket, char *packetData, int packetSize, int64_t pts = 0);
    void sendRtpH264Frame(RtpPacket *rtpPacket, char *frame, int frameSize, int64_t pts = 0);
    int  sendH264OverUDP(RtpPacket *rtpPacket, int packetSize);

    //解析AAC组包RTP发送
    void parseAACPacketData(RtpPacket *rtpPacket, char *packetData, int packetSize, int64_t pts = 0);
    void sendRtpAACFrame(RtpPacket *rtpPacket, char *frame, int frameSize, int64_t pts = 0);
    int  sendAACOverUDP(RtpPacket *rtpPacket, int packetSize);

protected:
    void run();

public:
    //媒体信息
    RTPMediaInfo info;
    //是否已设置
    bool setVideo;
    bool setAudio;

private:
    volatile bool isRun;

    //如果I帧前有sps pps下面则可以不设置
    bool spsppsSet;
    char *spsFrame;
    int spsLen;
    char *ppsFrame;
    int ppsLen;

    //传输类型 实时或者文件
    bool isReal;
    uint32_t videoTimeStamp;
    uint32_t audioTimeStamp;

    //队列锁
    QMutex mutex;

    //网络发送
    QUdpSocket *videoSocket;
    QUdpSocket *audioSocket;

    //音视频包队列
    QList<ReadyPacket *> packetList;
};


#endif // RTPSEND_H
