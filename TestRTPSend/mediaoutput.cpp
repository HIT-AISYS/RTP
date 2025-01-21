#include "mediaoutput.h"

VideoOutput::VideoOutput()
{
    sdlWindow = nullptr;
    sdlRender = nullptr;
    sdlTexture = nullptr;
    showId = nullptr;

    hasInit = false;
}

VideoOutput::~VideoOutput()
{
    release();
}

void VideoOutput::startDisplay(AVFrame *videoFrame)
{
    if(videoFrame == nullptr)
    {
        qDebug() << "视频帧无效";
        return;
    }

    if(!hasInit)
    {
        qDebug() << "参数未设置";
        return;
    }

    //句柄尺寸变化 那么SDL渲染的窗口也会跟着变化
    SDL_Surface *sdlSurface = SDL_GetWindowSurface(sdlWindow);
    if (!sdlSurface)
    {
        qDebug() << "SDL_GetWindowSurface fail" << SDL_GetError();
        return;
    }

    //渲染
    int ret = SDL_UpdateYUVTexture(sdlTexture, NULL, videoFrame->data[0], videoFrame->linesize[0], videoFrame->data[1], videoFrame->linesize[1], videoFrame->data[2], videoFrame->linesize[2]);
    if (ret != 0)
        qDebug() << "SDL_UpdateYUVTexture fail" << SDL_GetError();

    ret = SDL_RenderClear(sdlRender);
    if (ret != 0)
        qDebug() << "SDL_RenderClear fail" << SDL_GetError();

    ret = SDL_RenderCopy(sdlRender, sdlTexture, NULL, NULL);
    if (ret != 0)
        qDebug() << "SDL_RenderCopy fail" << SDL_GetError();

    SDL_RenderPresent(sdlRender);
}

void VideoOutput::stopDisplay()
{
    if(!hasInit)
        return;

    SDL_SetRenderDrawColor(sdlRender, 240, 240, 240, 100);
    SDL_RenderClear(sdlRender);
    SDL_RenderPresent(sdlRender);
}

void VideoOutput::setShowHandle(void *id)
{
    this->showId = id;
}

bool VideoOutput::setVideoParm(int width, int height, bool cpuMode)
{
    if(this->showId == nullptr)
    {
        qDebug() << "video showId is nullptr";
        return false;
    }

    //SDL存在界面伸缩有时画面卡住 查阅资料应该是与SDL内部逻辑处理冲突 多次测试采用重置参数方法更为直接有效
    release();

    //默认cpu软件渲染模式 相比gpu硬件渲染兼容性更高
    SDL_RendererFlags flag;
    if(cpuMode)
        flag = SDL_RENDERER_SOFTWARE;
    else
        flag = SDL_RENDERER_ACCELERATED;

    sdlWindow = SDL_CreateWindowFrom(showId);
    if(!sdlWindow)
    {
        qDebug() << "SDL_CreateWindowFrom error" << SDL_GetError();
        return false;
    }

    sdlRender = SDL_CreateRenderer(sdlWindow, -1, flag);
    if (!sdlRender)
    {
        qDebug() << "SDL_CreateRenderer error" << SDL_GetError();
        return false;
    }

    sdlTexture = SDL_CreateTexture(sdlRender, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!sdlTexture)
    {
        qDebug() << "SDL_CreateRenderer error" << SDL_GetError();
        return false;
    }

    SDL_ShowWindow(sdlWindow);
    hasInit = true;
    return true;
}

void VideoOutput::release()
{
    if(sdlTexture)
    {
        SDL_DestroyTexture(sdlTexture);
        sdlTexture = nullptr;
    }

    if(sdlRender)
    {
        SDL_DestroyRenderer(sdlRender);
        sdlRender = nullptr;
    }

    if(sdlWindow)
    {
        SDL_DestroyWindow(sdlWindow);
        sdlWindow = nullptr;
    }

    hasInit = false;
}


AudioOutput::AudioOutput()
{
    audioOutput = nullptr;
    audioDevice = nullptr;
    audioSwrCtx = nullptr;
    audioData = nullptr;
    sampleRate = 0;
    sampleSize = 0;
    channels = 0;
    hasInit = false;
}

AudioOutput::~AudioOutput()
{
    release();
}

void AudioOutput::startPlay(AVFrame *audioFrame)
{
    if(audioFrame == nullptr || !hasInit)
    {
        qDebug() << "音频帧无效或参数未设置";
        return;
    }

    int outChannel = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    int outSize = av_samples_get_buffer_size(NULL, outChannel, audioFrame->nb_samples, AV_SAMPLE_FMT_S16, 1);

    int result = swr_convert(audioSwrCtx, &audioData, outSize, (const uint8_t **)audioFrame->data, audioFrame->nb_samples);
    if(result >= 0)
        audioDevice->write((char *)audioData, outSize);

}

bool AudioOutput::setAudioParm(int sampleRate, int sampleFmt, int channels)
{
    //释放上次重置即可
    release();

    this->sampleRate = sampleRate;
    this->sampleSize = av_get_bytes_per_sample((AVSampleFormat)sampleFmt) / 2;;
    this->channels = channels;

    QAudioFormat format;
    format.setCodec("audio/pcm");
    format.setSampleRate(sampleRate);
    format.setSampleSize(sampleSize * 8);
    format.setChannelCount(channels);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setByteOrder(QAudioFormat::LittleEndian);

    QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());

    bool res = info.isFormatSupported(format);
    if(!res)
    {
        qDebug() << "AudioFormat not Support";
        return false;
    }

    audioOutput = new QAudioOutput(QAudioDeviceInfo::defaultOutputDevice(), format);
    //设置缓存避免播放音频卡顿  太大可能会分配内存失败导致崩溃
    audioOutput->setBufferSize(40960);
    audioOutput->setVolume(1.0);
    audioDevice = audioOutput->start();

    //设置音频转换
    audioSwrCtx = swr_alloc();
    int64_t channelOut = AV_CH_LAYOUT_STEREO;
    int64_t channelIn = av_get_default_channel_layout(channels);
    audioSwrCtx = swr_alloc_set_opts(audioSwrCtx, channelOut, AV_SAMPLE_FMT_S16, sampleRate, channelIn, (AVSampleFormat)sampleFmt, sampleRate, 0, 0);
    if(0 > swr_init(audioSwrCtx))
    {
        qDebug() << "audio swr_init error";
        release();
        return false;
    }

    //分配音频数据内存19200 可调节
    audioData = (uint8_t *)av_malloc(19200 * sizeof(uint8_t));

    hasInit = true;
    return true;
}

void AudioOutput::release()
{
    if(audioDevice)
        audioDevice->close();
    if(audioSwrCtx)
        swr_free(&audioSwrCtx);
    if(audioData)
        av_free(audioData);
    if(audioOutput)
        audioOutput->deleteLater();

    hasInit = false;
}
