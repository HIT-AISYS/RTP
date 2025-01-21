#include "mediacapturer.h"

VideoCapturer::VideoCapturer()
{
    isRun = false;
    rtpSender = nullptr;
}

VideoCapturer::~VideoCapturer()
{
    quit();
    wait();
}

void VideoCapturer::setRunState(bool start)
{
    isRun = start;

    if(start)
        this->start();
}

void VideoCapturer::run()
{
    if(rtpSender == nullptr)
    {
        qDebug() << "外部RTP发送者指针无效";
        return;
    }

    AVFormatContext *avFormatContext = avformat_alloc_context();
    AVInputFormat *iFmt = NULL;
    int ret = 0;

#if 0
    //本地文件
    ret = avformat_open_input(&avFormatContext, "E:/MediaFile/Video/music264.mp4", iFmt, NULL);
#elif 0
    //桌面
    iFmt = av_find_input_format("gdigrab");
    ret = avformat_open_input(&avFormatContext, "desktop", iFmt, NULL);
#elif 1
    //设备
    iFmt = av_find_input_format("dshow");
    ret = avformat_open_input(&avFormatContext, "video=HD Webcam", iFmt, NULL);
#endif

    if (ret < 0)
    {
        qDebug() << "open input error" << getAVError(ret);
        return;
    }

    //usb摄像头很多固定帧率30 下方options设置可能无效
    AVDictionary *options = NULL;
    av_dict_set(&options, "framerate", "15", 0);

    //获取流信息
    ret = avformat_find_stream_info(avFormatContext, &options);
    if (ret < 0)
    {
        qDebug() << "find stream info error";
        av_dict_free(&options);
        return;
    }

    av_dict_free(&options);

    //获取视频流
    int videoIndex = av_find_best_stream(avFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (videoIndex < 0)
    {
        qDebug() << "find video stream index error";
        return;
    }
    AVStream *videoStream = avFormatContext->streams[videoIndex];

    //初始化编码器上下文
    AVCodec *videoDecoder = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (videoDecoder == NULL)
    {
        qDebug() << "video decoder not found";
        return;
    }

    AVCodecContext *videoDeCodecCtx = avcodec_alloc_context3(videoDecoder);
    if (!videoDeCodecCtx)
    {
        qDebug() << "avcodec_alloc_context3 error";
        return;
    }

    //复制参数
    ret = avcodec_parameters_to_context(videoDeCodecCtx, videoStream->codecpar);
    if (ret < 0)
    {
        qDebug() << "avcodec_parameters_to_context error" << getAVError(ret);
        return;
    }

    //打开解码器
    ret = avcodec_open2(videoDeCodecCtx, videoDecoder, NULL);
    if (ret < 0)
    {
        qDebug() << "open video codec error" << getAVError(ret);
        return;
    }

    //获取视频信息 注：USB设备或录屏当系统负载较多时很难达到真实帧率 可能下降较多
    int videoWidth = videoStream->codecpar->width;
    int videoHeight = videoStream->codecpar->height;
    int videoFPS = av_q2d(videoStream->avg_frame_rate);

    //实时传输不需要太高帧数
    uint frameCount = 0;
    bool needHalfFrame = false;
    if(videoFPS >= 20)
    {
        needHalfFrame = true;
        videoFPS = videoFPS * 0.5;
    }

    videoOutput->setVideoParm(videoWidth, videoHeight, true);
    rtpSender->info.width = videoWidth;
    rtpSender->info.height = videoHeight;
    rtpSender->info.fps = videoFPS;
    rtpSender->info.hasVideo = 1;

    rtpSender->setVideo = true;
    rtpSender->setTransType(true);

    //定义像素格式
    AVPixelFormat srcFormat = videoDeCodecCtx->pix_fmt;
    AVPixelFormat dstFormat = AV_PIX_FMT_YUV420P;

    //设置编码器
    VideoSwser videoSwser;
    videoSwser.initSwsCtx(videoWidth, videoHeight, srcFormat, videoWidth, videoHeight, dstFormat);

    VideoEncoder videoEncoder;
    videoEncoder.initEncoder(videoWidth, videoHeight, dstFormat, videoFPS);

    AVPacket *videoPacket = av_packet_alloc();
    AVFrame *videoFrame = av_frame_alloc();
    qDebug() << "init VideoCapturer Sucess" << videoWidth << videoHeight << videoFPS;

    while (isRun)
    {
        //摄像头不断读取视频packet
        int ret = av_read_frame(avFormatContext, videoPacket);
        if (ret == AVERROR_EOF)
            break;

        frameCount++;
        if(needHalfFrame && (frameCount % 2 == 0))
        {
            av_packet_unref(videoPacket);
            continue;
        }

        //摄像头一般采集的直接是AV_CODEC_ID_RAWVIDEO yuv422格式
        if (videoPacket->stream_index == videoIndex)
        {
            //编码数据进行解码
            ret = avcodec_send_packet(videoDeCodecCtx, videoPacket);
            if (ret < 0)
            {
                qDebug() << "avcodec_send_packet video error" << getAVError(ret);
                av_packet_unref(videoPacket);
                continue;
            }

            //获得解码数据AVFrame
            ret = avcodec_receive_frame(videoDeCodecCtx, videoFrame);
            if (ret < 0)
            {
                qDebug() << "avcodec_receive_frame video error" << getAVError(ret);
                av_packet_unref(videoPacket);
                continue;
            }

            //编码器处理数据
            AVFrame *swsFrame = videoSwser.getSwsFrame(videoFrame);
            videoOutput->startDisplay(swsFrame);

            AVPacket *packet = videoEncoder.getEncodePacket(swsFrame);
            if(packet)
            {
                ReadyPacket *readyPacket = new ReadyPacket();
                readyPacket->type = PacketVideoStart;
                readyPacket->packet = packet;
                rtpSender->writePacket(readyPacket);
            }

            //释放解码帧
            av_frame_free(&swsFrame);
            av_frame_unref(videoFrame);
        }

        //释放原始编码帧
        av_packet_unref(videoPacket);
    }

    videoOutput->stopDisplay();

    av_packet_free(&videoPacket);
    videoPacket = NULL;
    av_frame_free(&videoFrame);
    videoFrame = NULL;
    avcodec_free_context(&videoDeCodecCtx);
    videoDeCodecCtx = NULL;
    avformat_close_input(&avFormatContext);
    avFormatContext = NULL;
}


AudioCapturer::AudioCapturer()
{
    isRun = false;
    rtpSender = nullptr;
}

AudioCapturer::~AudioCapturer()
{
    quit();
    wait();
}

void AudioCapturer::setRunState(bool start)
{
    isRun = start;

    if(start)
        this->start();
}

void AudioCapturer::run()
{
    if(rtpSender == nullptr)
    {
        qDebug() << "外部RTP发送者指针无效";
        return;
    }

    //设置Qt音频录制参数
    int sampleRate = 44100;
    int channels = 2;
    int sampleByte = 2;
    AVSampleFormat inSampleFmt = AV_SAMPLE_FMT_S16;

    QAudioFormat recordFmt;
    recordFmt.setSampleRate(sampleRate);
    recordFmt.setChannelCount(channels);
    recordFmt.setSampleSize(sampleByte * 8);
    recordFmt.setCodec("audio/pcm");
    recordFmt.setByteOrder(QAudioFormat::LittleEndian);
    recordFmt.setSampleType(QAudioFormat::UnSignedInt);
    QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
    if (!info.isFormatSupported(recordFmt))
    {
        qDebug() << "Audio format not support!";
        recordFmt = info.nearestFormat(recordFmt);
    }
    QAudioInput *audioInput = new QAudioInput(recordFmt);

    //ffmpeg -h encoder=aac 查询自带编码器仅支持AV_SAMPLE_FMT_FLTP 大多数AAC编码器都采用平面布局数据格式 可以提高数据访问效率和缓存命中率 加快编码效率
    AVSampleFormat outSampleFmt = AV_SAMPLE_FMT_FLTP;
    rtpSender->info.sampleRate = sampleRate;
    rtpSender->info.format = outSampleFmt;
    rtpSender->info.channel = channels;
    rtpSender->info.hasAudio = 1;
    rtpSender->setAudio = true;
    rtpSender->setTransType(false);

    //设置重采样
    AudioSwrer audioSwrer;
    audioSwrer.initSwrCtx(channels, sampleRate, inSampleFmt, channels, sampleRate, outSampleFmt);

    //初始化音频编码器相关
    AudioEncoder audioEncoder;
    audioEncoder.initEncoder(channels, sampleRate, outSampleFmt);

    //一帧pcm原始数据的字节数
    int pcmSize = av_get_bytes_per_sample((AVSampleFormat)inSampleFmt) * channels * 1024;
    char *pcmBuf = new char[pcmSize];

    //音频数据开始捕获
    QIODevice *audioIO = audioInput->start();
    qDebug() << "init AudioCapturer Sucess" << sampleRate << channels << outSampleFmt;

    while(isRun)
    {
        if (audioInput->bytesReady() >= pcmSize)
        {
            //捕获一帧pcm原始数据
            int size = 0;
            while (size != pcmSize)
            {
                int len = audioIO->read(pcmBuf + size, pcmSize - size);
                if (len < 0)
                    break;
                size += len;
            }

            //重采样后进行编码处理
            AVFrame *swrFrame = audioSwrer.getSwrFrame((uint8_t *)pcmBuf);

            //添加时间戳 实时时间戳方式
            AVPacket *packet = audioEncoder.getEncodePacket(swrFrame);
            if(packet)
            {
                ReadyPacket *readyPacket = new ReadyPacket();
                readyPacket->type = PacketAudio;
                readyPacket->packet = packet;
                rtpSender->writePacket(readyPacket);
            }

            av_frame_free(&swrFrame);
        }

        QThread::msleep(1);
    }

    //释放资源
    audioInput->stop();
    audioIO->close();
    delete audioInput;

    delete[] pcmBuf;
    pcmBuf = nullptr;
}


