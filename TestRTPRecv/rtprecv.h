#ifndef RTPRECV_H
#define RTPRECV_H

#include <QThread>
#include <QUdpSocket>
#include "fifobuffer.h"

class RTPRecv : public QThread
{
    Q_OBJECT
public:
    explicit RTPRecv(QObject *parent = nullptr);
    ~RTPRecv();

    void setRunState(bool start);

    void resetRecvBuffer();

protected:
    void run();

public:
    bool isRun;

    //RTP包队列
    FiFoBuffer *fifoBufferVideo;
    FiFoBuffer *fifoBufferAudio;
};

#endif // RTPRECV_H
