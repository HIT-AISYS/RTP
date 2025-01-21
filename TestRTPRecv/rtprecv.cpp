#include "rtprecv.h"

RTPRecv::RTPRecv(QObject *parent) : QThread(parent)
{
    isRun = false;

    fifoBufferVideo = new FiFoBuffer();
    fifoBufferVideo->initFiFoBuffer(3*1024*1024);

    fifoBufferAudio = new FiFoBuffer();
    fifoBufferAudio->initFiFoBuffer(1*1024*1024);
}

RTPRecv::~RTPRecv()
{
    isRun = false;

    delete fifoBufferVideo;
    fifoBufferVideo = nullptr;

    delete fifoBufferAudio;
    fifoBufferAudio = nullptr;

    quit();
    wait();
}

void RTPRecv::setRunState(bool start)
{
    isRun = start;

    if(start)
        this->start();
}

void RTPRecv::resetRecvBuffer()
{
    fifoBufferAudio->resetFiFoBuffer();
    fifoBufferVideo->resetFiFoBuffer();
}

void RTPRecv::run()
{
    QUdpSocket udpSocket;
    udpSocket.bind(QHostAddress::AnyIPv4, 8554);

    QUdpSocket udpSocketAudio;
    udpSocketAudio.bind(QHostAddress::AnyIPv4, 8555);

    char udpData[1500] = {0};
    char udpDataAudio[1500] = {0};

    while (isRun)
    {
        if(udpSocket.hasPendingDatagrams())
        {
            memset(udpData, 0, 1500);
            qint64 len = udpSocket.readDatagram(udpData, 1500);
            fifoBufferVideo->pushData(udpData, len);
        }

        if(udpSocketAudio.hasPendingDatagrams())
        {
            memset(udpDataAudio, 0, 1500);
            qint64 len = udpSocketAudio.readDatagram(udpDataAudio, 1500);
            fifoBufferAudio->pushData(udpDataAudio, len);
        }

        QThread::msleep(1);
    }
}
