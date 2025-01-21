#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QFileDialog>
#include "worker.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE


class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

    void initState();

private slots:
    void on_btnSelect_clicked();
    void on_btnStart_clicked();
    void on_btnClose_clicked();

    void on_btnRealStart_clicked();
    void on_btnRealStop_clicked();

private:
    Ui::Widget *ui;

    QString fileName;

    Worker worker;
};
#endif // WIDGET_H
