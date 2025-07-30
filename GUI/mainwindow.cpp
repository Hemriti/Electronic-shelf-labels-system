#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSerialPortInfo>
#include <QDebug>
#include <product.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QSqlDatabase sqlitedb = QSqlDatabase::addDatabase("QSQLITE");
    serialPort = new QSerialPort(this);

    populateSerialSettings();
    connect(serialPort, &QSerialPort::readyRead, this, &MainWindow::onReadyRead);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::populateSerialSettings()
{
    foreach (const QSerialPortInfo &portInfo, QSerialPortInfo::availablePorts()) {
        ui->comChoice->addItem(portInfo.portName());
    }

    ui->bauderateChoice->addItems({"9600", "19200", "38400", "57600", "115200"});
    // Set fixed values for stop bit, data bits, and parity
    ui->parityChoice->setEnabled(false);  // Disable the parity selection as it's fixed
    ui->stopbitChoice->setEnabled(false); // Disable the stop bit selection as it's fixed
    ui->dataChoice->setEnabled(false);    // Disable the data bits selection as it's fixed
}

void MainWindow::on_comChoice_activated(const QString &arg1)
{
    selectedPort = arg1;
}

void MainWindow::on_baudrateChoice_activated(const QString &arg1)
{
    selectedBaudRate = arg1.toInt();
}

void MainWindow::on_connectBtn_clicked()
{
    if (serialPort->isOpen()) {
        closePort();
    } else {
        if (openPort(selectedPort, selectedBaudRate)) {
            qDebug() << "Port opened successfully!";
        } else {
            qDebug() << "Failed to open port!";
        }
    }
}

bool MainWindow::openPort(const QString &portName, qint32 baudRate)
{
    serialPort->setPortName(portName);
    serialPort->setBaudRate(baudRate);
    serialPort->setParity(QSerialPort::NoParity);  // Fixed NoParity
    serialPort->setDataBits(QSerialPort::Data8);   // Fixed Data8
    serialPort->setStopBits(QSerialPort::OneStop); // Fixed OneStop

    if (serialPort->open(QIODevice::ReadWrite)) {
        return true;
    } else {
        qDebug() << "Error opening port:" << serialPort->errorString();
        return false;
    }
}

void MainWindow::closePort()
{
    if (serialPort->isOpen()) {
        serialPort->close();
        qDebug() << "Port closed!";
    }
}

void MainWindow::sendData(const QByteArray &data)
{
    if (serialPort->isOpen()) {
        serialPort->write(data);
    } else {
        qDebug() << "Port is not open!";
    }
}

QByteArray MainWindow::readData()
{
    return serialPort->readAll();
}

void MainWindow::onReadyRead()
{
    QByteArray data = serialPort->readAll();
    qDebug() << "Received Data: " << data;
}










#include <QDebug>
#include <QSqlError>

void MainWindow::on_login_clicked()
{
    QSqlDatabase sqlitedb = QSqlDatabase::addDatabase("QSQLITE");
    sqlitedb.setDatabaseName("C:/Users/moham/OneDrive/Documents/database/database.db");

    if (!sqlitedb.open()) {
        ui->log->setText("Connection failed: " + sqlitedb.lastError().text());
        return;
    }

    QString username = ui->username->text().trimmed();
    QString password = ui->password->text().trimmed();

    QSqlQuery query(sqlitedb);
    query.prepare("SELECT * FROM database WHERE username = :username AND password = :password");

    query.bindValue(":username", username);
    query.bindValue(":password", password);

    if (query.exec()) {
        if (query.next()) {
            ui->log->setText("Login successful!");
            Product *product_win = new Product();
            product_win->show();

                       this->hide();
        } else {
            ui->log->setText("Invalid username or password.");
        }
    } else {

        ui->log->setText("connexion error ");
    }

    sqlitedb.close();
}


void MainWindow::on_flash_clicked()
{
    if (serialPort->isOpen()) {
        serialPort->close();
    }

    // Set up UART (COM10, 115200 baud)
    serialPort->setPortName("COM3");
    serialPort->setBaudRate(115200);
    serialPort->setDataBits(QSerialPort::Data8);
    serialPort->setParity(QSerialPort::NoParity);
    serialPort->setStopBits(QSerialPort::OneStop);
    serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (!serialPort->open(QIODevice::ReadWrite)) {
        qDebug() << "Failed to open COM3";
        return;
    }

    qDebug() << "COM4 opened successfully!";

    // Example product data (Replace these with actual data from UI)
    QString macAddress = "BC:9A:78:56:34:13";

    QString productID = "1234";
    QString productName = "PC";
    QString productPrice = "1900";
    QString productDetail = "Lenovo";

    // Send product details one by one
    QList<QString> productData = {macAddress,productID, productName, productPrice, productDetail};

    for (const QString &data : productData) {
        QByteArray message = data.toUtf8() + "\n";
        serialPort->write(message);
        serialPort->flush();
        serialPort->waitForBytesWritten(100);
        qDebug() << "Sent: " << message;
        QThread::msleep(100); // Small delay to ensure proper transmission
    }

    // Read response from device
    if (serialPort->waitForReadyRead(500)) {
        QByteArray response = serialPort->readAll();
        qDebug() << "Received: " << response;
    } else {
        qDebug() << "No response received!";
    }
}
