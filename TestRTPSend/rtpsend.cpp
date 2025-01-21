#include "rtpsend.h"

int32_t hexArrayToDec(char *array, int len)
{
    if(array == nullptr)
        return -1;

    int32_t result = 0;
    for(int i=0; i<len; i++)
        result = result * 256 + (unsigned char)array[i];

    return result;
}

int assertStartCode3(char *buf)
{
    if (buf[0] == 0 && buf[1] == 0 && buf[2] == 1)
        return 1;
    else
        return 0;
}

int assertStartCode4(char *buf)
{
    if (buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1)
        return 1;
    else
        return 0;
}

int splitOneH264Frame(char *inFrame, int inSize, int *outSize)
{
    //实际数据位置
    int pos = 0;
    //前缀变量
    int isStartCode3 = 0;
    int isStartCode4 = 0;
    int curStartCodeLen = 0;

    //判断前缀长度
    isStartCode3 = assertStartCode3(inFrame);
    if (isStartCode3 != 1)
    {
        //如果不是，再读一个字节
        isStartCode4 = assertStartCode4(inFrame);
        if (isStartCode4 != 1)
        {
            return -1;
        }
        else
        {
            pos = 4;
            curStartCodeLen = 4;
        }
    }
    else
    {
        pos = 3;
        curStartCodeLen = 3;
    }

    //查找到下一个开始前缀的标志位
    int nextStartCodeFound = 0;
    int nextStartCodeLen = 0;
    while (!nextStartCodeFound)
    {
        if (pos == (inSize - 1)) //判断是否到了数据尾，则返回非0值，否则返回0
        {
            *outSize = pos + 1;
            return curStartCodeLen;
        }

        pos += 1;

        isStartCode4 = assertStartCode4(&inFrame[pos - 4]);//判断是否为0x00000001
        if (isStartCode4 != 1)
            isStartCode3 = assertStartCode3(&inFrame[pos - 3]);//判断是否为0x000001

        nextStartCodeFound = (isStartCode4 == 1 || isStartCode3 == 1);
    }

    //把pos指向前一个NALU的末尾，在pos位置上偏移下一个前缀长度的值
    nextStartCodeLen = (isStartCode4 == 1) ? 4 : 3;
    pos -= nextStartCodeLen;

    //outSize含有前缀的NALU的长度 curStartCodeLen此数据帧的前缀长度
    *outSize = pos;
    return curStartCodeLen;
}

RtpPacketSender::RtpPacketSender()
{
    isRun = false;
    videoSocket = nullptr;
    audioSocket = nullptr;
    spsFrame = nullptr;
    ppsFrame = nullptr;
    spsLen = 0;
    ppsLen = 0;
    spsppsSet = false;

    info.length = sizeof(info);
    setVideo = false;
    setAudio = false;
    isReal = false;
}

RtpPacketSender::~RtpPacketSender()
{
    isRun = false;
    release();

    quit();
    wait();
}

void RtpPacketSender::setRunState(bool start)
{
    isRun = start;

    if(start)
    {
        release();
        this->start();
    }
}

void RtpPacketSender::setTransType(bool real)
{
    videoTimeStamp = 0;
    audioTimeStamp = 0;

    isReal = real;
}

void RtpPacketSender::getRTPVideoTimeStamp(int64_t pts)
{
    if(isReal)
    {
        //us1000000时间基 转 90000时间基
        videoTimeStamp = pts * 0.9;
    }
    else
    {
        //文件按照固定间隔叠加
        int duration = 90000 / info.fps;
        videoTimeStamp += duration;
    }

    qDebug() << "video pts" << videoTimeStamp << pts;
}

void RtpPacketSender::getRTPAudioTimeStamp(int64_t pts)
{
    if(isReal)
    {
        //us1000000时间基 转 90000时间基
        audioTimeStamp = pts * 0.9;
    }
    else
    {
        //文件按照固定间隔叠加
        int duration = info.sampleRate / (info.sampleRate / 1024);
        audioTimeStamp += duration;
    }

//    qDebug() << "audio pts" << audioTimeStamp << pts;
}

void RtpPacketSender::setSPS_PPS(char *spsFrame, int spsLen, char *ppsFrame, int ppsLen)
{
    this->spsFrame = spsFrame;
    this->spsLen = spsLen;
    this->ppsFrame = ppsFrame;
    this->ppsLen = ppsLen;
    this->spsppsSet = true;
}

void RtpPacketSender::release()
{
    QMutexLocker locker(&mutex);
    for(int i=packetList.size() - 1; i >= 0; i--)
    {
        ReadyPacket *ready = packetList.at(i);
        AVPacket *packet = ready->packet;
        av_packet_free(&packet);

        delete ready;
        ready = nullptr;
    }

    packetList.clear();
}

void RtpPacketSender::writePacket(ReadyPacket *packet)
{
    QMutexLocker locker(&mutex);
    packetList.append(packet);
}

void RtpPacketSender::parseH264PacketData(RtpPacket *rtpPacket, char *packetData, int packetSize, int64_t pts)
{
    if(rtpPacket == nullptr || packetData == nullptr || packetSize < 4)
        return;

    //已解析长度
    int hasParseLen = 0;
    bool hasSPSPPS = false;

    //解封装后的AVpacket中存在多帧组合的情况如SPS、PPS、IDR 组包RTP需分开每一帧；部分也有缺少SPS、PPS，需填充
    while(1)
    {
        //AVpacket数据格式：四字节大小+实际数据
        char *curPacketData = packetData + hasParseLen;
        int curLen = hexArrayToDec(curPacketData, 4);
        hasParseLen += curLen + 4;
        char *curFrame = curPacketData + 4;

        //I帧之前没有sps pps则填充
        uint8_t typeValue = curFrame[0] & 0x1F;
        if(typeValue == 7 || typeValue == 8)
            hasSPSPPS = true;

        if(this->spsppsSet && typeValue == 5 && hasSPSPPS == false)
        {
            sendRtpH264Frame(rtpPacket, spsFrame, spsLen, pts);
            sendRtpH264Frame(rtpPacket, ppsFrame, ppsLen, pts);
        }

        //得到单独一帧组包发送
        if(typeValue == 5 || typeValue == 1 || typeValue == 7 || typeValue == 8)
            sendRtpH264Frame(rtpPacket, curFrame, curLen, pts);

        if(hasParseLen == packetSize)
            break;
    }
}

void RtpPacketSender::parseH264PacketDataWithStartCode(RtpPacket *rtpPacket, char *packetData, int packetSize, int64_t pts)
{
    if(rtpPacket == nullptr || packetData == nullptr || packetSize < 4)
        return;

    //已解析长度
    int hasParseLen = 0;
    bool hasSPSPPS = false;

    //编码后的AVpacket中存在多帧组合的情况如SPS、PPS、IDR 组包RTP需分开每一帧；部分也有缺少SPS、PPS，需填充
    while(1)
    {
        //编码的AVpacket数据格式：startcode+实际数据
        char *curPacketData = packetData + hasParseLen;
        int curLen = 0;
        int startCodeLen = splitOneH264Frame(curPacketData, packetSize-hasParseLen, &curLen);
        hasParseLen += curLen;
        char *curFrame = curPacketData + startCodeLen;

        //I帧之前没有sps pps则填充
        uint8_t typeValue = curFrame[0] & 0x1F;
        if(typeValue == 7 || typeValue == 8)
            hasSPSPPS = true;

        if(this->spsppsSet && typeValue == 5 && hasSPSPPS == false)
        {
            sendRtpH264Frame(rtpPacket, spsFrame, spsLen, pts);
            sendRtpH264Frame(rtpPacket, ppsFrame, ppsLen, pts);
        }

        //得到单独一帧组包发送
        if(typeValue == 5 || typeValue == 1 || typeValue == 7 || typeValue == 8)
            sendRtpH264Frame(rtpPacket, curFrame, curLen - startCodeLen, pts);

        if(hasParseLen == packetSize)
            break;
    }
}

void RtpPacketSender::sendRtpH264Frame(RtpPacket *rtpPacket, char *frame, int frameSize, int64_t pts)
{
    if(frame == nullptr || frameSize <= 0)
        return;

    // nalu第一个字节
    uint8_t naluType = frame[0];
    uint8_t typeValue = naluType & 0x1F;

    //判断分包方式
    if (frameSize <= RTP_MAX_PKT_SIZE)
    {
        qDebug() << "H264RTP包 单包发送 type:" << typeValue << "size" << frameSize << rtpPacket->rtpHeader.seq << rtpPacket->rtpHeader.timestamp;

        // nalu长度小于最大包长：单一NALU单元模式
        //*   0 1 2 3 4 5 6 7 8 9
        //*  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //*  |F|NRI|  Type   | a single NAL unit ... |
        //*  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

        memcpy(rtpPacket->payload, frame, frameSize);
        int packetSize = frameSize + RTP_HEADER_SIZE;
        sendH264OverUDP(rtpPacket, packetSize);

        rtpPacket->rtpHeader.seq++;

        // 如果是SPS、PPS、SEI就不需要加时间戳
        if (typeValue == 7 || typeValue == 8 || typeValue == 6)
            return;

    }
    else
    {
        // nalu长度小于最大包场：分片模式
        //*  0                   1                   2
        //*  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
        //* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //* | FU indicator  |   FU header   |   FU payload   ...  |
        //* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //*     FU Indicator
        //*    0 1 2 3 4 5 6 7
        //*   +-+-+-+-+-+-+-+-+
        //*   |F|NRI|  Type   |
        //*   +---------------+
        //*      FU Header
        //*    0 1 2 3 4 5 6 7
        //*   +-+-+-+-+-+-+-+-+
        //*   |S|E|R|  Type   |
        //*   +---------------+

        qDebug() << "H264RTP包 拆包发送 type:" << typeValue << "size" << frameSize << rtpPacket->rtpHeader.seq << rtpPacket->rtpHeader.timestamp;

        int pktNum = frameSize / RTP_MAX_PKT_SIZE;        // 有几个完整的包
        int remainPktSize = frameSize % RTP_MAX_PKT_SIZE; // 剩余不完整包的大小
        int pos = 1;                                      // 第一个字节为类型 需跳过
        int limit = 0; //发送大小限制

        // 发送完整的包
        for (int i = 0; i < pktNum; i++)
        {
            rtpPacket->payload[0] = (naluType & 0x60) | 28;
            rtpPacket->payload[1] = naluType & 0x1F;

            if (i == 0) //第一包数据
                rtpPacket->payload[1] |= 0x80; // start
            else if (remainPktSize == 0 && i == pktNum - 1) //最后一包数据
                rtpPacket->payload[1] |= 0x40; // end

            memcpy(rtpPacket->payload + 2, frame + pos, RTP_MAX_PKT_SIZE);
            int packetSize = RTP_MAX_PKT_SIZE + RTP_HEADER_SIZE + 2;
            sendH264OverUDP(rtpPacket, packetSize);

            rtpPacket->rtpHeader.seq++;
            pos += RTP_MAX_PKT_SIZE;

            //短时发送大数据量数据可能出现丢包，如果是大数据每发送一定数据后进行短暂延时
            limit += packetSize;
            if(limit >= 1*packetSize)
            {
                limit = 0;
                QThread::msleep(1);
            }
        }

        // 发送剩余的数据
        if (remainPktSize > 0)
        {
            rtpPacket->payload[0] = (naluType & 0x60) | 28;
            rtpPacket->payload[1] = naluType & 0x1F;
            rtpPacket->payload[1] |= 0x40; //end

            memcpy(rtpPacket->payload + 2, frame + pos, remainPktSize - 1);
            int packetSize = remainPktSize + RTP_HEADER_SIZE + 1;
            sendH264OverUDP(rtpPacket, packetSize);

            rtpPacket->rtpHeader.seq++;
        }

    }

    //增加时间戳
    getRTPVideoTimeStamp(pts);
    rtpPacket->rtpHeader.timestamp = videoTimeStamp;
}

int RtpPacketSender::sendH264OverUDP(RtpPacket *rtpPacket, int packetSize)
{
    //均为小端，结构体传输，暂不考虑字节序差异
    int sendSize = videoSocket->writeDatagram((char *)rtpPacket, packetSize, QHostAddress(destIP), destUDPVideoPort);
    return sendSize;
}

void RtpPacketSender::parseAACPacketData(RtpPacket *rtpPacket, char *packetData, int packetSize, int64_t pts)
{
    if(rtpPacket == nullptr || packetData == nullptr || packetSize <= 0)
        return;

    sendRtpAACFrame(rtpPacket, packetData, packetSize, pts);
}

void RtpPacketSender::sendRtpAACFrame(RtpPacket *rtpPacket, char *frame, int frameSize, int64_t pts)
{
    //    qDebug() << "AAC RTP包 size:" << frameSize << "timeStamp" << rtpPacket->rtpHeader.timestamp;

    rtpPacket->payload[0] = 0x00;
    rtpPacket->payload[1] = 0x10;
    rtpPacket->payload[2] = (frameSize & 0x1FE0) >> 5; //高8位
    rtpPacket->payload[3] = (frameSize & 0x1F) << 3;  //低5位

    memcpy(rtpPacket->payload + 4, frame, frameSize);
    sendAACOverUDP(rtpPacket, frameSize + RTP_HEADER_SIZE + 4);

    rtpPacket->rtpHeader.seq++;

    /*
     * 如果采样频率是44100
     * 一般AAC每个1024个采样为一帧 一秒就有 44100 / 1024 = 43帧
     * 时间增量就是 44100 / 43 = 1025 一帧的时间为 1 / 43 = 23ms
     */
    getRTPAudioTimeStamp(pts);
    rtpPacket->rtpHeader.timestamp = audioTimeStamp;
    return;
}

int RtpPacketSender::sendAACOverUDP(RtpPacket *rtpPacket, int packetSize)
{
    //均为小端，结构体传输，暂不考虑字节序差异
    int sendSize = audioSocket->writeDatagram((char *)rtpPacket, packetSize, QHostAddress(destIP), destUDPAudioPort);
    return sendSize;
}

void RtpPacketSender::run()
{
    while (!setVideo || !setAudio)
    {
        QThread::msleep(1);
    }

    //发送媒体信息
    QTcpSocket tcpSocket;
    tcpSocket.connectToHost(QHostAddress(destIP), destTCPPort);
    if(tcpSocket.waitForConnected())
    {
        info.cmd = 1;
        tcpSocket.write((char *)&info, info.length);
        tcpSocket.flush();
        qDebug() << "开始RTP发送";
    }
    else
    {
        qDebug() << "发送媒体信息失败";
        isRun = false;
        return;
    }

    videoSocket = new QUdpSocket();
    audioSocket = new QUdpSocket();

    //设置RTP输出
    struct RtpPacket *rtpVideoPacket = (struct RtpPacket*)malloc(500000);
    rtpHeaderInit(rtpVideoPacket, 0, 0, 0, RTP_VESION, RTP_PAYLOAD_TYPE_H264, 0, 0, 0, 0);
    struct RtpPacket *rtpAudioPacket = (struct RtpPacket*)malloc(200000);
    rtpHeaderInit(rtpAudioPacket, 0, 0, 0, RTP_VESION, RTP_PAYLOAD_TYPE_AAC, 0, 0, 0, 0);

    while (isRun)
    {
        //读取队列当前所有包 只记录包指针 减少锁粒度
        int size = packetList.size();
        QList<ReadyPacket *> list;
        mutex.lock();
        for(int i=0; i<size; i++)
        {
            ReadyPacket *ready = packetList.first();
            list.append(ready);
            packetList.pop_front();
        }
        mutex.unlock();

        for(int i=0; i<list.size(); i++)
        {
            ReadyPacket *ready = list.at(i);
            AVPacket *packet = ready->packet;
            AVPacketType type = ready->type;

            if(type == PacketVideo)
                parseH264PacketData(rtpVideoPacket, (char *)packet->data, packet->size, packet->pts);
            else if(type == PacketVideoStart)
                parseH264PacketDataWithStartCode(rtpVideoPacket, (char *)packet->data, packet->size, packet->pts);
            else if(type == PacketAudio)
                parseAACPacketData(rtpAudioPacket, (char *)packet->data, packet->size, packet->pts);

            av_packet_free(&packet);
            delete ready;
            ready = nullptr;
        }

        QThread::msleep(1);
    }

    //结束指令
    info.cmd = 2;
    tcpSocket.write((char *)&info, info.length);
    tcpSocket.flush();
    tcpSocket.close();
    qDebug() << "结束RTP发送";

    release();
    audioSocket->deleteLater();
    videoSocket->deleteLater();

    setVideo = false;
    setAudio = false;
    spsppsSet = false;

    free(rtpVideoPacket);
    rtpVideoPacket = nullptr;
    free(rtpAudioPacket);
    rtpAudioPacket = nullptr;
}

