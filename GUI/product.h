#ifndef PRODUCT_H
#define PRODUCT_H

#include <QDialog>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QTextEdit>
#include <QSerialPort>
#include <QSerialPortInfo>



namespace Ui {
class Product;

}

class Product : public QDialog
{
    Q_OBJECT

public:
    explicit Product(QWidget *parent = nullptr);
    ~Product();

private slots:
    void on_searchBtn_clicked();

    void on_changeBtn_clicked();

    void on_clearBtn_clicked();

    void on_addBtn_clicked();

    void on_saveBtn_clicked();

    void on_cancelBtn_clicked();

    void on_deleteBtn_clicked();

    void on_allProducts_clicked();

    void on_flashBtn_clicked();

private:
    Ui::Product *ui;
    bool productFound; // bool to track existance of the product

        void searchProduct(const QString &id);
        void addProduct(const QString &id, const QString &name, const QString &price, const QString &details);
        void updateProduct(const QString &id, const QString &name, const QString &price, const QString &details);
        void resetFields();
        void sendData(const QString &data);
        QSerialPort *serial;
};

#endif // PRODUCT_H
