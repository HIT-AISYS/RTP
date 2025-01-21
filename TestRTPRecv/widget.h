#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QPainter>
#include <QTcpServer>
#include <QTcpSocket>
#include "videodecoder.h"
#include "audiodecoder.h"
#include "rtprecv.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

struct RTPMediaInfo
{
    int length;  //信息体长度
    int width;
    int height;
    int fps;
    int sampleRate;
    int channel;
    int format;
    short hasVideo;
    short hasAudio;
    short cmd;  //1开始 2结束
};

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

    void init();

public slots:
    void newConnection();
    void recvTcpData();

private:
    Ui::Widget *ui;

    QTcpServer server;
    QTcpSocket *socket;

    RTPRecv rtpRecv;
    VideoDecoder vDecoder;
    AudioDecoder aDecoder;
};
#endif // WIDGET_H
