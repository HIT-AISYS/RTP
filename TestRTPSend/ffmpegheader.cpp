#include "ffmpegheader.h"

char *getAVError(int errNum)
{
    static char msg[32] = {0};
    av_strerror(errNum, msg, 32);
    return msg;
}

int64_t getRealTimeByPTS(int64_t pts, AVRational timebase)
{
    //pts转换为对应us值
    AVRational timebase_q = {1, AV_TIME_BASE};
    int64_t ptsTime = av_rescale_q(pts, timebase, timebase_q);
    return ptsTime;
}

void calcAndDelay(int64_t startTime, int64_t pts, AVRational timebase)
{
    int64_t ptsTime = getRealTimeByPTS(pts, timebase);

    //计算startTime到此刻的时间差值
    int64_t nowTime = av_gettime() - startTime;
    int64_t offset = ptsTime - nowTime;

    //大于2秒一般时间戳存在问题 延时无法挽救
    if(offset > 1000 && offset < 2*1000*1000)
        av_usleep(offset);
}


VideoSwser::VideoSwser()
{
    videoSwsCtx = nullptr;
    hasInit = false;
    needSws = false;
}

VideoSwser::~VideoSwser()
{
    release();
}

bool VideoSwser::initSwsCtx(int srcWidth, int srcHeight, AVPixelFormat srcFmt, int dstWidth, int dstHeight, AVPixelFormat dstFmt)
{
    release();

    if(srcWidth == dstWidth && srcHeight == dstHeight && srcFmt == dstFmt)
    {
        needSws = false;
    }
    else
    {
        //设置转换上下文 srcFmt 到 dstFmt(一般为AV_PIX_FMT_YUV420P)的转换
        videoSwsCtx = sws_getContext(srcWidth, srcHeight, srcFmt, dstWidth, dstHeight, dstFmt, SWS_BILINEAR, NULL, NULL, NULL);
        if (videoSwsCtx == NULL)
        {
            qDebug() << "sws_getContext error";
            return false;
        }

        this->dstFmt = dstFmt;
        this->dstWidth = dstWidth;
        this->dstHeight = dstHeight;

        needSws = true;
    }

    hasInit = true;
    return true;
}

void VideoSwser::release()
{
    if(videoSwsCtx)
    {
        sws_freeContext(videoSwsCtx);
        videoSwsCtx = nullptr;
    }

    hasInit = false;
    needSws = false;
}

AVFrame *VideoSwser::getSwsFrame(AVFrame *srcFrame)
{
    if(!hasInit)
    {
        qDebug() << "Swser未初始化";
        return nullptr;
    }

    if(!srcFrame)
        return nullptr;

    if(!needSws)
        return srcFrame;

    AVFrame *frame = av_frame_alloc();
    frame->format = dstFmt;
    frame->width = dstWidth;
    frame->height = dstHeight;

    int ret = av_frame_get_buffer(frame, 0);
    if (ret != 0)
    {
        qDebug() << "av_frame_get_buffer swsFrame error";
        return nullptr;
    }

     ret = av_frame_make_writable(frame);
    if (ret != 0)
    {
        qDebug() << "av_frame_make_writable swsFrame error";
        return nullptr;
    }

    sws_scale(videoSwsCtx, (const uint8_t *const *)srcFrame->data, srcFrame->linesize, 0, dstHeight, frame->data, frame->linesize);
    return frame;
}

VideoEncoder::VideoEncoder()
{
    videoEnCodecCtx = nullptr;
    hasInit = false;
    duration = 0;
    timeStamp = 0;
}

VideoEncoder::~VideoEncoder()
{
    release();
}

bool VideoEncoder::initEncoder(int width, int height, AVPixelFormat fmt, int fps)
{
    //重置编码信息
    release();

    //设置编码器参数 默认AV_CODEC_ID_H264
    AVCodec *videoEnCoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    if(!videoEnCoder)
    {
        qDebug() << "avcodec_find_encoder AV_CODEC_ID_H264 error";
        return false;
    }

    videoEnCodecCtx = avcodec_alloc_context3(videoEnCoder);
    if(!videoEnCodecCtx)
    {
        qDebug() << "avcodec_alloc_context3 AV_CODEC_ID_H264 error";
        return false;
    }

    //编码参数设置 重要！
    videoEnCodecCtx->bit_rate = 1*1024*1024; //1080P:4Mbps 720P:2Mbps 480P:1Mbps 默认中等码率可适当增大
    videoEnCodecCtx->width = width;
    videoEnCodecCtx->height = height;
    videoEnCodecCtx->framerate = {fps, 1};
    videoEnCodecCtx->time_base = {1, AV_TIME_BASE};
    videoEnCodecCtx->gop_size = fps;
    videoEnCodecCtx->max_b_frames = 0;
    videoEnCodecCtx->pix_fmt = fmt;
    videoEnCodecCtx->thread_count = 4;
    videoEnCodecCtx->thread_type = FF_THREAD_FRAME;

    //设置AV_CODEC_FLAG_GLOBAL_HEADER sps、pps将设置在extradata中 否则放置于每个I帧前
//    videoEnCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    //预设参数 编码速度与压缩率的平衡 如编码快选择的算法就偏简单 压缩率低
    //由慢到快veryslow slower slow medium fast faster veryfast superfast ultrafast 默认medium
    int ret = av_opt_set(videoEnCodecCtx->priv_data, "preset", "ultrafast", 0);
    if(ret != 0)
        qDebug() << "av_opt_set preset error";

    //偏好设置 进行视觉优化
    //film电影 animation动画片 grain颗粒物 stillimage静止图片 psnr ssim图像评价指标 fastdecode快速解码 zerolatency零延迟
    ret = av_opt_set(videoEnCodecCtx->priv_data, "tune", "zerolatency", 0);
    if(ret != 0)
        qDebug() << "av_opt_set preset error";

    //画质设置 可能自动改变 如编码很快很难保证高画质会自动降级
    //baseline实时通信 extended使用较少  main流媒体  high广电、存储
    ret = av_opt_set(videoEnCodecCtx->priv_data, "profile", "main", 0);
    if(ret != 0)
        qDebug() << "av_opt_set preset error";

    ret = avcodec_open2(videoEnCodecCtx, videoEnCoder, NULL);
    if(ret != 0)
    {
        qDebug() << "avcodec_open2 video error";
        return false;
    }

    hasInit = true;

    //us为单位
    duration = 1000000 / fps;
    timeStamp = 0;
    startTime = av_gettime();
    return true;
}

void VideoEncoder::release()
{
    if(videoEnCodecCtx)
    {
        avcodec_free_context(&videoEnCodecCtx);
        videoEnCodecCtx = nullptr;
    }

    hasInit = false;
}

AVPacket *VideoEncoder::getEncodePacket(AVFrame *srcFrame)
{
    if(!hasInit)
    {
        qDebug() << "VideoEncoder no init";
        return nullptr;
    }

    if(!srcFrame)
        return nullptr;

    if(srcFrame->width != videoEnCodecCtx->width
       || srcFrame->height != videoEnCodecCtx->height
       || srcFrame->format != videoEnCodecCtx->pix_fmt)
    {
        qDebug() << "srcFrame不符合视频编码器设置格式";
        return nullptr;
    }

    //添加时间戳
    calcTimeStamp();
    int64_t videoPts = av_rescale_q(timeStamp, AVRational{1, AV_TIME_BASE}, videoEnCodecCtx->time_base);
    srcFrame->pts = videoPts;

    int ret = avcodec_send_frame(videoEnCodecCtx, srcFrame);
    if (ret != 0)
        return nullptr;

    //接收者负责释放packet
    AVPacket *packet = av_packet_alloc();
    ret = avcodec_receive_packet(videoEnCodecCtx, packet);
    if (ret != 0)
    {
        av_packet_free(&packet);
        return nullptr;
    }

    return packet;
}

void VideoEncoder::calcTimeStamp()
{
    //1.累加帧间隔    优点：时间戳稳定均匀       缺点：实际采集帧率可能不稳定造成时钟错误；帧间隔若忽略小数会累加误差造成不同步
    //2.实时时间戳    优点：时间戳保持实时及正确  缺点：存在帧间隔不均匀，极端情况不能正常播放
    //3.累加+实时判断 优点：综合以上二者优点      缺点：纠正时间戳的某一时刻可能画面或声音卡顿

    //第三种方法 时间单位为us
    timeStamp += duration;

    int64_t elapsed = av_gettime() - startTime;
    uint32_t offset = qAbs(elapsed - timeStamp);
    if(offset >= (duration * 0.5))
        timeStamp = elapsed;

}


AudioSwrer::AudioSwrer()
{
    audioSwrCtx = nullptr;
    hasInit = false;
    needSwr = false;
}

AudioSwrer::~AudioSwrer()
{
    release();
}

bool AudioSwrer::initSwrCtx(int inChannels, int inSampleRate, AVSampleFormat inFmt, int outChannels, int outSampleRate, AVSampleFormat outFmt)
{
    release();

    if(inChannels == outChannels && inSampleRate == outSampleRate && inFmt == outFmt)
    {
        needSwr = false;
    }
    else
    {
        audioSwrCtx = swr_alloc_set_opts(NULL, av_get_default_channel_layout(outChannels), outFmt, outSampleRate,
                                         av_get_default_channel_layout(inChannels), inFmt, inSampleRate, 0, NULL);
        if (!audioSwrCtx)
        {
            qDebug() << "swr_alloc_set_opts failed!";
            return false;
        }

        int ret = swr_init(audioSwrCtx);
        if (ret != 0)
        {
            qDebug() << "swr_init error";
            swr_free(&audioSwrCtx);
            return false;
        }

        this->outFmt = outFmt;
        this->outChannels = outChannels;
        this->outSampleRate = outSampleRate;

        needSwr = true;
    }

    hasInit = true;
    return true;
}

void AudioSwrer::release()
{
    if(audioSwrCtx)
    {
        swr_free(&audioSwrCtx);
        audioSwrCtx = nullptr;
    }

    hasInit = false;
    needSwr = false;
}

AVFrame *AudioSwrer::getSwrFrame(AVFrame *srcFrame)
{
    if(!hasInit)
    {
        qDebug() << "Swrer未初始化";
        return nullptr;
    }

    if(!srcFrame)
        return nullptr;

    if(!needSwr)
        return srcFrame;

    AVFrame *frame = av_frame_alloc();
    frame->format = outFmt;
    frame->channels = outChannels;
    frame->channel_layout = av_get_default_channel_layout(outChannels);
    frame->nb_samples = 1024; //默认aac

    int ret = av_frame_get_buffer(frame, 0);
    if (ret != 0)
    {
        qDebug() << "av_frame_get_buffer audio error";
        return nullptr;
    }

    ret = av_frame_make_writable(frame);
    if (ret != 0)
    {
        qDebug() << "av_frame_make_writable swrFrame error";
        return nullptr;
    }

    const uint8_t **inData = (const uint8_t **)srcFrame->data;
    swr_convert(audioSwrCtx, frame->data, frame->nb_samples, inData, frame->nb_samples);
    return frame;
}

AVFrame *AudioSwrer::getSwrFrame(uint8_t *srcData)
{
    if(!hasInit)
    {
        qDebug() << "Swrer未初始化";
        return nullptr;
    }

    if(!srcData)
        return nullptr;

    if(!needSwr)
        return nullptr;

    AVFrame *frame = av_frame_alloc();
    frame->format = outFmt;
    frame->channels = outChannels;
    frame->sample_rate = outSampleRate;
    frame->channel_layout = av_get_default_channel_layout(outChannels);
    frame->nb_samples = 1024; //默认aac

    int ret = av_frame_get_buffer(frame, 0);
    if (ret != 0)
    {
        qDebug() << "av_frame_get_buffer audio error";
        return nullptr;
    }

    ret = av_frame_make_writable(frame);
    if (ret != 0)
    {
        qDebug() << "av_frame_make_writable swrFrame error";
        return nullptr;
    }

    const uint8_t *indata[AV_NUM_DATA_POINTERS] = {0};
    indata[0] = srcData;
    swr_convert(audioSwrCtx, frame->data, frame->nb_samples, indata, frame->nb_samples);
    return frame;
}

AudioEncoder::AudioEncoder()
{
    audioEnCodecCtx = nullptr;
    hasInit = false;
    duration = 0;
    timeStamp = 0;
}

AudioEncoder::~AudioEncoder()
{
    release();
}

bool AudioEncoder::initEncoder(int channels, int sampleRate, AVSampleFormat sampleFmt)
{
    release();

    //初始化音频编码器相关 默认AAC
    AVCodec *audioEnCoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!audioEnCoder)
    {
        qDebug() << "avcodec_find_encoder AV_CODEC_ID_AAC failed!";
        return false;
    }

    audioEnCodecCtx = avcodec_alloc_context3(audioEnCoder);
    if (!audioEnCodecCtx)
    {
        qDebug() << "avcodec_alloc_context3 AV_CODEC_ID_AAC failed!";
        return false;
    }

    //音频数据量偏小 设置较为简单
    audioEnCodecCtx->bit_rate = 64*1024;
    audioEnCodecCtx->time_base = AVRational{1, sampleRate};
    audioEnCodecCtx->sample_rate = sampleRate;
    audioEnCodecCtx->sample_fmt = sampleFmt;
    audioEnCodecCtx->channels = channels;
    audioEnCodecCtx->channel_layout = av_get_default_channel_layout(channels);
    audioEnCodecCtx->frame_size = 1024;

    //打开音频编码器
    int ret = avcodec_open2(audioEnCodecCtx, audioEnCoder, NULL);
    if (ret != 0)
    {
        qDebug() << "avcodec_open2 audio error" << getAVError(ret);
        return false;
    }

    hasInit = true;

    duration = sampleRate / (sampleRate / 1024);
    timeStamp = 0;
    startTime = av_gettime();
    return true;
}

void AudioEncoder::release()
{
    if(audioEnCodecCtx)
    {
        avcodec_free_context(&audioEnCodecCtx);
        audioEnCodecCtx = nullptr;
    }

    hasInit = false;
}

AVPacket *AudioEncoder::getEncodePacket(AVFrame *srcFrame)
{
    if(!hasInit)
    {
        qDebug() << "AudioEncoder no init";
        return nullptr;
    }

    if(!srcFrame)
        return nullptr;

    if(srcFrame->channels != audioEnCodecCtx->channels
       || srcFrame->sample_rate != audioEnCodecCtx->sample_rate
       || srcFrame->format != audioEnCodecCtx->sample_fmt)
    {
        qDebug() << "srcFrame不符合音频编码器设置格式";
        return nullptr;
    }

    //添加时间戳
    calcTimeStamp();
    int64_t audioPts = av_rescale_q(timeStamp, AVRational{1, AV_TIME_BASE}, audioEnCodecCtx->time_base);
    srcFrame->pts = audioPts;

    //进行音频编码得到编码数据AVPacket
    int ret = avcodec_send_frame(audioEnCodecCtx, srcFrame);
    if (ret != 0)
        return nullptr;

    //接收者负责释放packet
    AVPacket *packet = av_packet_alloc();
    ret = avcodec_receive_packet(audioEnCodecCtx, packet);
    if (ret != 0)
    {
        av_packet_free(&packet);
        return nullptr;
    }

    return packet;
}

void AudioEncoder::calcTimeStamp()
{
    //1.累加帧间隔    优点：时间戳稳定均匀       缺点：实际采集帧率可能不稳定造成时钟错误；帧间隔若忽略小数会累加误差造成不同步
    //2.实时时间戳    优点：时间戳保持实时及正确  缺点：存在帧间隔不均匀，极端情况不能正常播放
    //3.累加+实时判断 优点：综合以上二者优点      缺点：纠正时间戳的某一时刻可能画面或声音卡顿

    //第三种方法 时间单位为us
    timeStamp += duration;

    int64_t elapsed = av_gettime() - startTime;
    uint32_t offset = qAbs(elapsed - timeStamp);
    if(offset >= (duration * 0.5))
        timeStamp = elapsed;

}
