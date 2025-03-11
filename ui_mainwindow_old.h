/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.7.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>
#include <utils/videoglwidget_gl.h>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    VideoGLWidget_gl *cameraContainerWidget;
    QPushButton *track;
    QPushButton *fireOn;
    QPushButton *fieOff;
    QListWidget *trackIdListWidget;
    QPushButton *mode;
    QPushButton *motion;
    QPushButton *up;
    QPushButton *down;
    QPushButton *autotrack;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(1024, 768);
        MainWindow->setStyleSheet(QString::fromUtf8("background-color: rgb(154, 153, 150);"));
        MainWindow->setIconSize(QSize(5, 5));
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        cameraContainerWidget = new VideoGLWidget_gl(centralwidget);
        cameraContainerWidget->setObjectName("cameraContainerWidget");
        cameraContainerWidget->setGeometry(QRect(0, 0, 1024, 768));
        cameraContainerWidget->setStyleSheet(QString::fromUtf8("background-color: rgb(36, 31, 49);"));
        track = new QPushButton(centralwidget);
        track->setObjectName("track");
        track->setGeometry(QRect(810, 40, 89, 25));
        fireOn = new QPushButton(centralwidget);
        fireOn->setObjectName("fireOn");
        fireOn->setGeometry(QRect(810, 110, 89, 25));
        fieOff = new QPushButton(centralwidget);
        fieOff->setObjectName("fieOff");
        fieOff->setGeometry(QRect(810, 140, 89, 25));
        trackIdListWidget = new QListWidget(centralwidget);
        trackIdListWidget->setObjectName("trackIdListWidget");
        trackIdListWidget->setGeometry(QRect(930, 50, 101, 281));
        mode = new QPushButton(centralwidget);
        mode->setObjectName("mode");
        mode->setGeometry(QRect(720, 520, 89, 25));
        motion = new QPushButton(centralwidget);
        motion->setObjectName("motion");
        motion->setGeometry(QRect(720, 550, 89, 25));
        up = new QPushButton(centralwidget);
        up->setObjectName("up");
        up->setGeometry(QRect(830, 240, 89, 25));
        down = new QPushButton(centralwidget);
        down->setObjectName("down");
        down->setGeometry(QRect(830, 270, 89, 25));
        autotrack = new QPushButton(centralwidget);
        autotrack->setObjectName("autotrack");
        autotrack->setGeometry(QRect(830, 310, 89, 25));
        MainWindow->setCentralWidget(centralwidget);
        track->raise();
        fireOn->raise();
        fieOff->raise();
        trackIdListWidget->raise();
        mode->raise();
        motion->raise();
        up->raise();
        down->raise();
        autotrack->raise();
        cameraContainerWidget->raise();

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MainWindow", nullptr));
        track->setText(QCoreApplication::translate("MainWindow", "track", nullptr));
        fireOn->setText(QCoreApplication::translate("MainWindow", "fire on", nullptr));
        fieOff->setText(QCoreApplication::translate("MainWindow", "fire off", nullptr));
        mode->setText(QCoreApplication::translate("MainWindow", "modes", nullptr));
        motion->setText(QCoreApplication::translate("MainWindow", "motion", nullptr));
        up->setText(QCoreApplication::translate("MainWindow", "up", nullptr));
        down->setText(QCoreApplication::translate("MainWindow", "down", nullptr));
        autotrack->setText(QCoreApplication::translate("MainWindow", "autotrack", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
