// mainwindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <QTimer>
#include <QListWidget>


class GimbalController;
class WeaponController;
class CameraController;
class JoystickController;
class SystemStateMachine;
class SystemStateModel;
class DayCameraPipelineDevice;

#include "models/systemstatemodel.h"
#include "custommenudialog.h"
#include "devices/videodisplaywidget.h"
#include "utils/cameracontainerwidget.h"
#include <QHBoxLayout>
 #include <QWidget>
 #include <QStackedWidget>
namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(GimbalController *gimbal,
                        WeaponController *weapon,
                        CameraController *camera,
                        SystemStateMachine *stateMachine,
                        JoystickController *joystick,
                        SystemStateModel *stateModel,
                        QWidget *parent = nullptr);
    ~MainWindow();

signals:

     void trackSelectButtonPressed();
private slots:

    // etc. other UI handlers
    void onActiveCameraChanged(bool isDay);
    void on_opmode_clicked();

    void onSystemStateChanged(const SystemStateData &newData);
    void onTrackSelectButtonPressed();
    /*void handleUpSwitchPress();
    void handleDownSwitchPress();
    void handleMenuValSwitchPress();

    void showMenu(); // example method to create a CustomMenuWidget*/

    void onUpSwChanged();
    void onDownSwChanged();
    void onMenuValSwChanged();

    void showIdleMenu();
    void handleMenuClosed() ;
    void showSystemStatus();
    void personalizeReticle();
    void personalizeColor();
    void adjustBrightness() ;
    void configureSettings() ;
    void viewLogs() ;
    void softwareUpdates();
    void runDiagnostics() ;
    void showHelpAbout();
    void handleMenuOptionSelected(const QString &option);

    void on_fireOn_clicked();

    void on_fieOff_clicked();

    void on_mode_clicked();

    void on_track_clicked();

    void on_motion_clicked();

    void onUpTrackChanged(bool state);
    void onDownTrackChanged(bool state);
    void on_up_clicked();

    void on_down_clicked();

    void on_autotrack_clicked();

    void on_day_clicked();

    void on_night_clicked();

    void on_quit_clicked();
    void onAlarmDetected(uint16_t alarmCode, const QString &description);
    void onAlarmCleared();
    void onAlarmHistoryRead(const QList<uint16_t> &alarmHistory);
    void onAlarmHistoryCleared();

    void on_read_clicked();

    void on_clear_clicked();
    void onCameraStateChanged();
    void setupCameraDisplays();
private:
    Ui::MainWindow *ui;
    VideoDisplayWidget *m_currentDisplayWidget;
    GimbalController *m_gimbalCtrl;
    WeaponController *m_weaponCtrl;    
    CameraController *m_cameraCtrl;
    JoystickController *m_joystickCtrl;
    SystemStateMachine *m_stateMachine;

    QVBoxLayout *m_layout = nullptr;

    DayCameraPipelineDevice *m_dayCamera = nullptr;
    //NightCameraPipelineDevice *m_nightCamera = nullptr;

    SystemStateModel *m_stateModel = nullptr;
    SystemStateData m_oldState; // store old m_stateModel state

    // Example: track a menu widget pointer
    CustomMenuWidget *m_menuWidget = nullptr;
    bool m_menuActive = false;


    CustomMenuWidget *m_reticleMenuWidget = nullptr;
    bool m_reticleMenuActive = false;

    CustomMenuWidget *m_colorMenuWidget = nullptr;
    bool m_colorMenuActive = false;

    CustomMenuWidget *m_systemStatusWidget = nullptr;
    bool m_systemStatusActive = false;

    //CustomMenuWidget *m_colorWidget = nullptr;
    //bool m_colorMenuActive = false;

    CustomMenuWidget *m_settingsMenuWidget = nullptr;
    bool m_settingsMenuActive = false;

    CustomMenuWidget *m_aboutWidget = nullptr;
    bool m_aboutActive = false;
    bool m_isDayCameraActive = true;

    void closeAppAndHardware();
    void onSelectedTrackLost(int trackId);
    void onTrackedIdsUpdated(const QSet<int>& trackIds);
    void processPendingUpdates();
    int findItemIndexByData(QListWidget* listWidget, int data) const;




    QTimer *updateTimer;
    QTimer *statusTimer;
    QSet<int> pendingTrackIds;
    bool updatePending = false;

    void setTracklistColorStyle(const QString &style);
    void onTrackIdSelected(QListWidgetItem *current, QListWidgetItem *previous);
    void switchCameraWidget(QWidget *fromWidget, QWidget *toWidget);
    CameraContainerWidget* m_cameraContainer;
    QWidget *m_dayWidgetPlaceholder;    
    QWidget *m_nightWidgetPlaceholder;
    void testBothDisplays();
    QStackedWidget* m_displayStack = nullptr;
    //VideoDisplayWidget* m_currentDisplayWidget = nullptr;
    //bool m_isDayCameraActive = true;
    
    // Remove switchCameraWidget if you're using the stack widget approach
    // Or keep it for backward compatibility but modify its implementation
    //void switchCameraWidget(QWidget* fromWidget, QWidget* toWidget);
    void switchCameraWidget();
};

#endif // MAINWINDOW_H

