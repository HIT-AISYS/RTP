#include "audiodecoder.h"



AudioDecoder::AudioDecoder(QObject *parent) : QThread(parent)
{
    isRun = false;
    entireRtpPacket = new char[1*1024*1024];
    entireLength = 0;

    audioOutput = new AudioOutput();
}

AudioDecoder::~AudioDecoder()
{
    isRun = false;
    delete[] entireRtpPacket;

    delete audioOutput;

    quit();
    wait();
}

void AudioDecoder::setRunState(bool start)
{
    if(start && fifoBuffer == nullptr)
        return;

    isRun = start;

    if(start)
        this->start();
}

void AudioDecoder::initDecodeParm(int sampleRate, int sampleFmt, int channels)
{
    this->sampleRate = sampleRate;
    this->sampleFmt = sampleFmt;
    this->channels = channels;
    audioOutput->setAudioParm(sampleRate, sampleFmt, channels);
}

AVPacket *AudioDecoder::parseRtpData(char *data, int size)
{
    RtpPacket *packet = (RtpPacket *)data;
    RtpHeader *header = (RtpHeader *)&packet->rtpHeader;
    uint32_t timeStamp = header->timestamp;

    int entireLength = size - RTP_HEADER_SIZE - 4;
    memcpy(entireRtpPacket, packet->payload + 4, entireLength);

    //直接赋值不会引用计数 packet不会自动释放data内存
    AVPacket *pkt = av_packet_alloc();
    pkt->pts = timeStamp;
    pkt->dts = timeStamp;
    pkt->data = (uint8_t *)entireRtpPacket;
    pkt->size = entireLength;

    return pkt;
}

void AudioDecoder::run()
{
    //获取视频流解码器, 或者指定解码器
    AVCodec *audioDecoder = avcodec_find_decoder(AV_CODEC_ID_AAC);
    if (audioDecoder == NULL)
    {
        qDebug() << "audioDecoder not found";
        return;
    }

    //初始化视频解码器上下文
    AVCodecContext *audioCodecCtx = avcodec_alloc_context3(audioDecoder);
    audioCodecCtx->sample_rate = sampleRate;
    audioCodecCtx->sample_fmt = (AVSampleFormat)sampleFmt;
    audioCodecCtx->channels = channels;
    audioCodecCtx->channel_layout = av_get_default_channel_layout(channels);
    audioCodecCtx->time_base = AVRational{1, sampleRate};

    int result = avcodec_open2(audioCodecCtx, audioDecoder, NULL);
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
            qDebug() << "音频已缓存3帧";
            break;
        }

        QThread::msleep(1);
    }

    AVFrame *audioFrame = av_frame_alloc();
    int64_t startPlaytime = av_gettime();

    while (isRun)
    {
        if(fifoBuffer->getDataNodeSize() == 0)
        {
            QThread::msleep(1);
            continue;
        }

        int rtpLength = 0;
        char *rtpData = fifoBuffer->popData(&rtpLength);

        //解析RTP包转为AVPacket 此packet指向堆数据不需释放
        AVPacket *pkt = nullptr;
        pkt = parseRtpData(rtpData, rtpLength);
        fifoBuffer->popDelete();

        if(pkt)
        {
            result = avcodec_send_packet(audioCodecCtx, pkt);
            if(result < 0)
            {
                qDebug() << "audio avcodec_send_packet" << getAVError(result);
                continue;
            }

            result = avcodec_receive_frame(audioCodecCtx, audioFrame);
            if(result == 0)
            {
                calcAndDelay(startPlaytime, pkt->pts, audioCodecCtx->time_base);

                audioOutput->startPlay(audioFrame);
                av_frame_unref(audioFrame);
            }

            av_packet_free(&pkt);
        }

        QThread::msleep(1);
    }

    avcodec_free_context(&audioCodecCtx);
    av_frame_free(&audioFrame);
}
