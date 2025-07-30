/********************************************************************************
** Form generated from reading UI file 'product_info.ui'
**
** Created by: Qt User Interface Compiler version 5.12.12
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_PRODUCT_INFO_H
#define UI_PRODUCT_INFO_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_product_info
{
public:
    QWidget *centralwidget;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *product_info)
    {
        if (product_info->objectName().isEmpty())
            product_info->setObjectName(QString::fromUtf8("product_info"));
        product_info->resize(800, 600);
        centralwidget = new QWidget(product_info);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        product_info->setCentralWidget(centralwidget);
        menubar = new QMenuBar(product_info);
        menubar->setObjectName(QString::fromUtf8("menubar"));
        product_info->setMenuBar(menubar);
        statusbar = new QStatusBar(product_info);
        statusbar->setObjectName(QString::fromUtf8("statusbar"));
        product_info->setStatusBar(statusbar);

        retranslateUi(product_info);

        QMetaObject::connectSlotsByName(product_info);
    } // setupUi

    void retranslateUi(QMainWindow *product_info)
    {
        product_info->setWindowTitle(QApplication::translate("product_info", "product_info", nullptr));
    } // retranslateUi

};

namespace Ui {
    class product_info: public Ui_product_info {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_PRODUCT_INFO_H
