#include "widget.h"
#include "ui_widget.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    init();
}

Widget::~Widget()
{
    SDL_Quit();
    delete ui;
}

void Widget::init()
{
    this->setWindowTitle("RTPRecv");
    SDL_Init(SDL_INIT_VIDEO);

    server.setParent(this);
    server.listen(QHostAddress::AnyIPv4, 8888);
    socket = nullptr;
    connect(&server, &QTcpServer::newConnection, this, &Widget::newConnection);

    vDecoder.fifoBuffer = rtpRecv.fifoBufferVideo;
    aDecoder.fifoBuffer = rtpRecv.fifoBufferAudio;

    rtpRecv.setRunState(true);
}

void Widget::newConnection()
{
    if(socket != nullptr)
        socket->abort();

    socket = server.nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &Widget::recvTcpData);
}

void Widget::recvTcpData()
{
    QByteArray array = socket->readAll();
    RTPMediaInfo *info = (RTPMediaInfo *)array.data();

    if(info->length != array.size())
    {
        qDebug() << "MediaInfo recvSize not equal";
        return;
    }

    //执行开始、结束指令
    if(info->cmd == 1)
    {
        qDebug() << "执行接收指令:开始";
        rtpRecv.resetRecvBuffer();

        if(info->hasAudio)
        {
            aDecoder.initDecodeParm(info->sampleRate, info->format, info->channel);
            aDecoder.setRunState(true);
            qDebug() << "音频参数" << info->sampleRate << info->format << info->channel;
        }
        if(info->hasVideo)
        {
            vDecoder.initDecodeParm((void *)ui->show->winId(), info->width, info->height);
            vDecoder.setRunState(true);
            qDebug() << "视频参数" << info->width << info->height;
        }
    }
    else if(info->cmd == 2)
    {
        qDebug() << "执行接收指令:结束" << endl;
        vDecoder.setRunState(false);
        aDecoder.setRunState(false);
    }
}


