#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QByteArray>
#include <QtSql>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDebug>
#include <product.h>


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_comChoice_activated(const QString &arg1);
    void on_baudrateChoice_activated(const QString &arg1);
    void on_parityChoice_activated(const QString &arg1);
    void on_dataChoice_activated(const QString &arg1);
    void on_stopbitChoice_activated(const QString &arg1);
    void on_connectBtn_clicked();
    void onReadyRead();

    void on_login_clicked();

    void on_flash_clicked();

private:
    Ui::MainWindow *ui;
    QSerialPort *serialPort;
    QString selectedPort;
    int selectedBaudRate;
    QSerialPort::Parity selectedParity;
    QString selectedDataBits;
    QSerialPort::StopBits selectedStopBits;

    void populateSerialSettings();
    bool openPort(const QString &portName, qint32 baudRate);
    void closePort();
    void sendData(const QByteArray &data);
    QByteArray readData();
    QSqlDatabase sqlitedb;
    QSerialPort *serial;  // Declare the serial port object

};

#endif // MAINWINDOW_H
