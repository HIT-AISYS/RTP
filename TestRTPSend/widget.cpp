#include "widget.h"
#include "ui_widget.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    initState();
}

Widget::~Widget()
{
    SDL_Quit();

    delete ui;
}

void Widget::initState()
{
    SDL_Init(SDL_INIT_VIDEO);
    avdevice_register_all();

    this->setWindowTitle("RTPSend");
    worker.videoOutput->setShowHandle((void *)ui->widget->winId());
}

void Widget::on_btnSelect_clicked()
{
    fileName = QFileDialog::getOpenFileName(this, "File", "E:/MediaFile/Video", "*.*");
    if(fileName.isEmpty())
        return;

    ui->lineEdit->setText(fileName);
}

void Widget::on_btnStart_clicked()
{
    if(fileName.isEmpty())
        return;

    worker.setFileName(fileName);
    worker.setRunState(true);

    worker.videoOutput->setShowHandle((void *)ui->widget->winId());
    ui->widget->setUpdatesEnabled(false);
}

void Widget::on_btnClose_clicked()
{
    worker.setRunState(false);
    ui->widget->setUpdatesEnabled(true);
}

void Widget::on_btnRealStart_clicked()
{
    worker.doTaskRealStart();
}

void Widget::on_btnRealStop_clicked()
{
    worker.doTaskRealStop();
}
