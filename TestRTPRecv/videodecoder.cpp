#include "videodecoder.h"

VideoDecoder::VideoDecoder(QObject *parent) : QThread(parent)
{
    fifoBuffer = nullptr;
    isRun = false;

    entireRtpPacket = new char[1*1024*1024];
    entireLength = 0;

    videoOutput = new VideoOutput();
}

VideoDecoder::~VideoDecoder()
{
    isRun = false;
    delete[] entireRtpPacket;

    delete videoOutput;

    quit();
    wait();
}

void VideoDecoder::setRunState(bool start)
{
    if(start && fifoBuffer == nullptr)
        return;

    isRun = start;

    if(start)
        this->start();
}

void VideoDecoder::initDecodeParm(void *showId, int width, int height)
{
    this->width = width;
    this->height = height;
    videoOutput->setShowHandle(showId);
    videoOutput->setVideoParm(width, height, true);
}

AVPacket *VideoDecoder::parseRtpData(char *data, int size)
{
    RtpPacket *packet = (RtpPacket *)data;
    RtpHeader *header = (RtpHeader *)&packet->rtpHeader;
    uint32_t timeStamp = header->timestamp;

    AVPacket *pkt = nullptr;

    bool single = (packet->payload[0] & 0x1f) != 28 ? true : false;
    if (single)
    {
        //单一NALU单元
        char *payload = (char *)packet->payload;
        int payloadLen = size - sizeof(RtpHeader);

        entireLength = 4;
        memset(entireRtpPacket, 0, 1*1024*1024);
        entireRtpPacket[3] = 1;
        memcpy(entireRtpPacket + entireLength, payload, payloadLen);
        entireLength += payloadLen;

        qDebug() << "单独包" << timeStamp << header->seq << entireLength << (packet->payload[0] & 0x1f);
        pkt = getAvPacketFromRtp(entireRtpPacket, entireLength, timeStamp);
    }
    else
    {
        //分包执行合并
        char fuIndicator = packet->payload[0];
        char fuHeader = packet->payload[1];
        char nalu = (fuIndicator & 0x60) | (fuHeader & 0x1f);
        bool start = fuHeader & 0x80;
        bool end = fuHeader & 0x40;
        bool middle = !(fuHeader & 0xc0);

        if(start)
        {
            entireLength = 0;
            memset(entireRtpPacket, 0, 1*1024*1024);

            char *payload = (char *)packet->payload + 2;
            int payloadLen = size - sizeof(RtpHeader) - 2;
            entireRtpPacket[3] = 1;
            entireRtpPacket[4] = nalu;
            entireLength += 5;
            memcpy(entireRtpPacket + entireLength, payload, payloadLen);
            entireLength += payloadLen;
        }

        if(middle)
        {
            char *payload = (char *)packet->payload + 2;
            int payloadLen = size - sizeof(RtpHeader) - 2;
            memcpy(entireRtpPacket + entireLength, payload, payloadLen);
            entireLength += payloadLen;
        }

        if(end)
        {
            char *payload = (char *)packet->payload + 2;
            int payloadLen = size - sizeof(RtpHeader) - 2;
            memcpy(entireRtpPacket + entireLength, payload, payloadLen);
            entireLength += payloadLen;

            qDebug() << "合并包" << timeStamp << header->seq << entireLength << (nalu & 0x1f);
            pkt = getAvPacketFromRtp(entireRtpPacket, entireLength, timeStamp);
        }
    }

    return pkt;
}

AVPacket *VideoDecoder::getAvPacketFromRtp(char *data, int size, int timeStamp)
{
    //直接填充AvPacket数据形式为 startcode+data 如00 00 00 01 67... 直接赋值不会引用计数 packet不会自动释放data内存
    AVPacket *packet = av_packet_alloc();
    packet->pts = timeStamp;
    packet->dts = timeStamp;
    packet->data = (uint8_t *)data;
    packet->size = size;

    return packet;
}

void VideoDecoder::run()
{
    //获取视频流解码器, 或者指定解码器
    AVCodec *videoDecoder = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (videoDecoder == NULL)
    {
        qDebug() << "video decoder not found";
        return;
    }

    //初始化视频解码器上下文
    AVCodecContext *videoCodecCtx = avcodec_alloc_context3(videoDecoder);
    videoCodecCtx->width = width;
    videoCodecCtx->height = height;
    videoCodecCtx->time_base.den = 90000;
    videoCodecCtx->time_base.num = 1;
    videoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    videoCodecCtx->sample_aspect_ratio.num = 1;
    videoCodecCtx->sample_aspect_ratio.den = 1;
    videoCodecCtx->thread_count = 2;
    videoCodecCtx->slice_count = 2;

    int result = avcodec_open2(videoCodecCtx, videoDecoder, NULL);
    if (result < 0)
    {
        qDebug() << "open video codec error";
        return;
    }

    //缓存帧数据 帧数默认3 增加流畅度但会增大延时
    while (isRun)
    {
        if(fifoBuffer->getDataNodeSize() >= 3)
        {
            qDebug() << "视频已缓存3帧";
            break;
        }

        QThread::msleep(1);
    }

    AVFrame *frame = av_frame_alloc();
    int64_t startPlaytime = av_gettime();

    while (isRun)
    {
        if(fifoBuffer->getDataNodeSize() == 0)
        {
            QThread::msleep(1);
            continue;
        }

        //解析RTP包转为AVPacket 此packet指向堆数据不需释放
        int rtpLength = 0;
        char *rtpData = fifoBuffer->popData(&rtpLength);

        AVPacket *pkt = nullptr;
        pkt = parseRtpData(rtpData, rtpLength);
        fifoBuffer->popDelete();

        if(pkt)
        {
            result = avcodec_send_packet(videoCodecCtx, pkt);
            if(result < 0)
            {
                qDebug() << "video avcodec_send_packet" << getAVError(result);
                continue;
            }

            result = avcodec_receive_frame(videoCodecCtx, frame);
            if(result == 0)
            {
                calcAndDelay(startPlaytime, pkt->pts, videoCodecCtx->time_base);

                videoOutput->startDisplay(frame);
                av_frame_unref(frame);
            }

            av_packet_free(&pkt);
        }

        QThread::msleep(1);
    }

    videoOutput->stopDisplay();
    avcodec_free_context(&videoCodecCtx);
    av_frame_free(&frame);
}
