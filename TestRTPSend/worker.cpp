#include "worker.h"

Worker::Worker()
{
    isRun = false;

    videoOutput = new VideoOutput();
    audioOutput = new AudioOutput();
    rtpSender = new RtpPacketSender();
    videoCapturer = new VideoCapturer();
    audioCapturer = new AudioCapturer();
    videoCapturer->videoOutput = videoOutput;
    spsFrame = nullptr;
    ppsFrame = nullptr;
    spsLen = 0;
    ppsLen = 0;
}

Worker::~Worker()
{
    delete videoOutput;
    videoOutput = nullptr;
    delete audioOutput;
    audioOutput = nullptr;
    delete rtpSender;
    rtpSender = nullptr;
    delete videoCapturer;
    videoCapturer = nullptr;
    delete audioCapturer;
    audioCapturer = nullptr;
}

void Worker::msSleep(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

void Worker::setFileName(const QString &name)
{
    fileName = name;
}

void Worker::setRunState(bool start)
{
    isRun = start;

    if(start)
        this->start();
}

void Worker::parseSPS_PPS(AVCodecContext *videoCtx)
{
    //可参考 https://blog.csdn.net/heng615975867/article/details/120026185

    if(!videoCtx)
        return;

    //sps pps保存在解码器上下文的extradata字段 一般都是各1个 这里按照一个解析
    char *extraData = (char *)videoCtx->extradata;
    int curLen = 6;

    spsLen = hexArrayToDec(extraData + curLen, 2);
    curLen += 2;
    spsFrame = new char[spsLen];
    memcpy(spsFrame, extraData + curLen, spsLen);

    curLen += spsLen + 1;
    ppsLen = hexArrayToDec(extraData + curLen, 2);
    curLen += 2;
    ppsFrame = new char[ppsLen];
    memcpy(ppsFrame, extraData + curLen, ppsLen);
}

void Worker::sendPacketToRtpSender(AVPacket *packet, AVPacketType type)
{
    if(packet == nullptr)
        return;

    int size = packet->size;
    char *data = (char *)av_malloc(size);
    memcpy(data, packet->data, size);
    AVPacket *newPacket = av_packet_alloc();
    av_packet_from_data(newPacket, (uint8_t *)data, size);

    ReadyPacket *readyPacket = new ReadyPacket();
    readyPacket->packet = newPacket;
    readyPacket->type = type;

    rtpSender->writePacket(readyPacket);
}

void Worker::doTaskFile()
{
    std::string path = fileName.toStdString();
    int result = 0;
    bool hasVideo = false;
    bool hasAudio = false;

    //打开输入
    AVFormatContext *iFmtCtx = avformat_alloc_context();
    result = avformat_open_input(&iFmtCtx, path.c_str(), NULL, NULL);
    if (result < 0)
    {
        qDebug() << "open input error" << fileName;
        return;
    }

    //获取流信息
    result = avformat_find_stream_info(iFmtCtx, NULL);
    if (result < 0)
    {
        qDebug() << "find stream info error";
        return;
    }

    /*************** 视频流操作 *****************/
    //1 获取视频流
    int videoIndex = av_find_best_stream(iFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

    AVStream *videoStream = nullptr;
    AVCodecContext *videoCodecCtx = nullptr;
    if (videoIndex >= 0)
    {
        videoStream = iFmtCtx->streams[videoIndex];

        //2 获取视频流解码器, 或者指定解码器
        AVCodec *videoDecoder = avcodec_find_decoder(videoStream->codecpar->codec_id);
        if (videoDecoder == NULL)
        {
            qDebug() << "video decoder not found";
            return;
        }

        //3 初始化视频解码器上下文 解码就绪
        videoCodecCtx = avcodec_alloc_context3(videoDecoder);
        avcodec_parameters_to_context(videoCodecCtx, videoStream->codecpar);
        videoCodecCtx->lowres = videoDecoder->max_lowres;
        videoCodecCtx->flags2 |= AV_CODEC_FLAG2_FAST; //设置加速解码
        result = avcodec_open2(videoCodecCtx, videoDecoder, NULL);
        if (result < 0)
        {
            qDebug() << "open video codec error";
            return;
        }

        hasVideo = true;
    }
    else
    {
        qDebug() << "find video stream index error";
    }

    /*************** 音频流操作 *****************/
    //1 获取音频流
    int audioIndex = av_find_best_stream(iFmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);

    AVStream *audioStream = nullptr;
    AVCodecContext *audioCodecCtx = nullptr;
    if (audioIndex >= 0)
    {
        audioStream = iFmtCtx->streams[audioIndex];

        //2 获取音频流解码器, 或者指定解码器
        AVCodec *audioDecoder = avcodec_find_decoder(audioStream->codecpar->codec_id);
        if (audioDecoder == NULL)
        {
            qDebug() << "audio decoder not found";
            return;
        }

        //3 初始化音频解码器上下文 解码就绪
        audioCodecCtx = avcodec_alloc_context3(audioDecoder);
        avcodec_parameters_to_context(audioCodecCtx, audioStream->codecpar);
        audioCodecCtx->lowres = audioDecoder->max_lowres;
        audioCodecCtx->flags2 |= AV_CODEC_FLAG2_FAST; //设置加速解码
        result = avcodec_open2(audioCodecCtx, audioDecoder, NULL);
        if (result < 0)
        {
            qDebug() << "open audio codec error";
            return;
        }

        hasAudio = true;
    }
    else
    {
        qDebug() << "find audio stream index error";
    }

    if(!hasAudio && !hasVideo)
        return;

    //封装信息
    if(hasAudio)
    {
        int channels = audioCodecCtx->channels;
        int sampleRate = audioCodecCtx->sample_rate;
        int sampleFmt = audioCodecCtx->sample_fmt;
        qDebug() << "Audio:" << sampleRate << sampleFmt << channels;
        audioOutput->setAudioParm(sampleRate, sampleFmt, channels);

        rtpSender->info.sampleRate = sampleRate;
        rtpSender->info.format = sampleFmt;
        rtpSender->info.channel = channels;
    }

    if(hasVideo)
    {
        int videoWidth = videoCodecCtx->width;
        int videoHeight = videoCodecCtx->height;
        int videoFps = av_q2d(videoStream->avg_frame_rate);
        qDebug() << "Video:" << videoWidth << videoHeight << videoFps;
        videoOutput->setVideoParm(videoWidth, videoHeight, true);

        parseSPS_PPS(videoCodecCtx);
        rtpSender->setSPS_PPS(spsFrame, spsLen, ppsFrame, ppsLen);

        rtpSender->info.width = videoWidth;
        rtpSender->info.height = videoHeight;
        rtpSender->info.fps = videoFps;
    }

    rtpSender->info.hasAudio = hasAudio;
    rtpSender->info.hasVideo = hasVideo;

    //启动rtp发送线程
    rtpSender->setTransType(false);
    rtpSender->setVideo = true;
    rtpSender->setAudio = true;
    rtpSender->setRunState(true);

    qDebug() << "开始读包";
    AVPacket *packet = av_packet_alloc();
    AVFrame *videoFrame = av_frame_alloc();
    AVFrame *audioFrame = av_frame_alloc();
    int64_t startPrecessTime = av_gettime();

    while (isRun)
    {
        //不断读取视频packet
        int ret = av_read_frame(iFmtCtx, packet);
        if (ret == AVERROR_EOF)
        {
            qDebug() << "read finish";
            break;
        }

        if(packet->stream_index == videoIndex)
        {
            sendPacketToRtpSender(packet, PacketVideo);

            //解码本地渲染视频
            avcodec_send_packet(videoCodecCtx, packet);
            avcodec_receive_frame(videoCodecCtx, videoFrame);

            videoOutput->startDisplay(videoFrame);
            av_frame_unref(videoFrame);

            if(!hasAudio)
                calcAndDelay(startPrecessTime, packet->pts, videoStream->time_base);;
        }
        else if(packet->stream_index == audioIndex)
        {
            //开始播放时刻到此刻的时间差值 
            calcAndDelay(startPrecessTime, packet->pts, audioStream->time_base);

            sendPacketToRtpSender(packet, PacketAudio);

            //解码本地播放音频
            avcodec_send_packet(audioCodecCtx, packet);
            while( avcodec_receive_frame(audioCodecCtx, audioFrame) == 0)
            {
//                audioOutput->startPlay(audioFrame);
                av_frame_unref(audioFrame);
            }
        }

        av_packet_unref(packet);
    }

    qDebug() << "结束取包";
    videoOutput->stopDisplay();
    rtpSender->setRunState(false);

    av_frame_free(&videoFrame);
    av_frame_free(&audioFrame);
    av_packet_free(&packet);
    avcodec_free_context(&videoCodecCtx);
    avcodec_free_context(&audioCodecCtx);
    avformat_close_input(&iFmtCtx);
}

void Worker::doTaskRealStart()
{
    videoCapturer->rtpSender = rtpSender;
    audioCapturer->rtpSender = rtpSender;
    videoCapturer->setRunState(true);
    audioCapturer->setRunState(true);
    rtpSender->setRunState(true);
}

void Worker::doTaskRealStop()
{
    videoCapturer->setRunState(false);
    audioCapturer->setRunState(false);
    rtpSender->setRunState(false);
}

void Worker::run()
{
    doTaskFile();
}
