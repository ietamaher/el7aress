#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "controllers/gimbalcontroller.h"
#include "controllers/weaponcontroller.h"
#include "controllers/cameracontroller.h"
#include "controllers/joystickcontroller.h"
#include "devices/daycamerapipelinedevice.h"
#include "core/systemstatemachine.h"

#include "models/systemstatemodel.h"
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QCoreApplication>

MainWindow::MainWindow(GimbalController *gimbal,
                       WeaponController *weapon,
                       CameraController *camera,
                       SystemStateMachine *stateMachine,
                       JoystickController *joystick,
                       SystemStateModel *stateModel,
                       QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_gimbalCtrl(gimbal),
    m_weaponCtrl(weapon),
    m_cameraCtrl(camera),
    m_joystickCtrl(joystick),
    m_stateModel(stateModel),
    m_stateMachine(stateMachine),
    m_isDayCameraActive(true)
{
    ui->setupUi(this);

    if (m_stateModel) {
        connect(m_stateModel, &SystemStateModel::dataChanged,
                this,        &MainWindow::onSystemStateChanged);
    }

    connect(m_joystickCtrl, &JoystickController::trackSelectButtonPressed,
            this, &MainWindow::onTrackSelectButtonPressed);

            connect(m_gimbalCtrl, &GimbalController::azAlarmDetected, this, &MainWindow::onAlarmDetected);
            connect(m_gimbalCtrl, &GimbalController::azAlarmCleared, this, &MainWindow::onAlarmCleared);
            connect(m_gimbalCtrl, &GimbalController::elAlarmDetected, this, &MainWindow::onAlarmDetected);
            connect(m_gimbalCtrl, &GimbalController::elAlarmCleared, this, &MainWindow::onAlarmCleared);


    // Create a layout for whichever placeholder widget in your UI, or the central widget
    m_layout = new QVBoxLayout(ui->cameraContainerWidget);
        // e.g. cameraContainerWidget is a QWidget from .ui

    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    // Set font and font size
    QFont listFont("Arial", 13);
    ui->trackIdListWidget->setFont(listFont);

    // Set text color and background color with semi-transparency using Stylesheets
    ui->trackIdListWidget->setStyleSheet(
        "QListWidget { background-color: rgba(128, 128, 128, 170); }" // Light gray with 50% opacity
        "QListWidget::item { color: rgba(150, 0, 0, 255); }" // Dark blue text color
        );

    //ui->trackIdListWidget->hide();
    setTracklistColorStyle("Green");
    //ui->trackIdListWidget->hide();
    ui->trackIdListWidget->clear();
    // Optionally, set selection behavior and mode
    ui->trackIdListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->trackIdListWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    // Initially show the day camera
    SystemStateData newData;
    onActiveCameraChanged(newData.activeCameraIsDay);
    //m_layout->addWidget(m_cameraCtrl->getDayCameraWidget());
    //m_cameraCtrl->getDayCameraWidget()->setVisible(true);

    // If you have a button or checkbox to switch cameras:
    /*connect(ui->switchCameraButton, &QPushButton::clicked, this, [this]() {
        // Toggle
        onActiveCameraChanged(!m_isDayCameraActive);
    });*/
    updateTimer = new QTimer(this);
    updateTimer->setInterval(500); // Update every 500ms

    connect(m_cameraCtrl, &CameraController::trackedIdsUpdated, this, &MainWindow::onTrackedIdsUpdated, Qt::QueuedConnection);
    connect(m_cameraCtrl, &CameraController::selectedTrackLost, this, &MainWindow::onSelectedTrackLost);
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::processPendingUpdates);

    updateTimer->start();


}

MainWindow::~MainWindow()
{
    // Stop update timer
    if (updateTimer && updateTimer->isActive()) {
        updateTimer->stop();
    }

    delete ui;
}

void MainWindow::onSystemStateChanged(const SystemStateData &newData)
{

    if (!m_oldState.upSw && newData.upSw) {
         onUpSwChanged();
        m_oldState.upSw = newData.upSw;
    }
    if (!m_oldState.downSw && newData.downSw) {
        onDownSwChanged();
        m_oldState.upSw = newData.upSw;

    }
    if (!m_oldState.menuValSw && newData.menuValSw) {
         onMenuValSwChanged();
    }

    if (m_oldState.activeCameraIsDay != newData.activeCameraIsDay) {
         onActiveCameraChanged(newData.activeCameraIsDay);
    }

    if (m_oldState.authorized != newData.authorized) {
         closeAppAndHardware();    // !!!! New value must be equal to 1 to stop app
    }

    if (m_oldState.upTrackButton != newData.upTrackButton) {
         onUpTrackChanged(newData.upTrackButton);
    }
    if (m_oldState.downTrackButton != newData.downTrackButton) {
         onDownTrackChanged(newData.downTrackButton);
    }
      // If color changed, you might do something
    if (m_oldState.colorStyle != newData.colorStyle) {
        // e.g. update some UI
    }

    m_oldState = newData; // store for next comparison

}

void MainWindow::onActiveCameraChanged(bool isDay)
{
    // Guard against redundant operations
    if (m_isDayCameraActive == isDay)
        return;

    // Update widgets based on active camera
    if (isDay) {
        // Switch to day camera
        switchCameraWidget(
            m_cameraCtrl->getNightCameraWidget(),
            m_cameraCtrl->getDayCameraWidget()
        );
    } else {
        // Switch to night camera
        switchCameraWidget(
            m_cameraCtrl->getDayCameraWidget(),
            m_cameraCtrl->getNightCameraWidget()
        );
    }

    // Update state
    m_isDayCameraActive = isDay;
}
//m_cameraCtrl->setActiveCamera(m_isDayCameraActive);
void MainWindow::onUpSwChanged()
{

        if (m_reticleMenuActive && m_reticleMenuWidget) {
            m_reticleMenuWidget->moveSelectionUp();
        } else if (m_colorMenuActive && m_colorMenuWidget) {
            m_colorMenuWidget->moveSelectionUp();
        } else if (m_menuActive && m_menuWidget) {
            m_menuWidget->moveSelectionUp();
        } /*else if (m_stateManager->currentMode() == OperationalMode::Tracking) {
            // Move selection up
            int currentRow = ui->trackIdListWidget->currentRow();
            if (currentRow > 0) {
                ui->trackIdListWidget->setCurrentRow(currentRow - 1);
            }
        }*/
        // Other cases...

}

void MainWindow::onDownSwChanged()
{

        if (m_reticleMenuActive && m_reticleMenuWidget) {
            m_reticleMenuWidget->moveSelectionDown();
        } else if (m_colorMenuActive && m_colorMenuWidget) {
            m_colorMenuWidget->moveSelectionDown();
        } else if (m_menuActive && m_menuWidget) {
            m_menuWidget->moveSelectionDown();
        } /*else if (m_stateManager->currentMode() == OperationalMode::Tracking) {
            // Move selection down
            int currentRow = ui->trackIdListWidget->currentRow();
            if (currentRow < ui->trackIdListWidget->count() - 1) {
                ui->trackIdListWidget->setCurrentRow(currentRow + 1);
            }
        }*/
        // Other cases...

}

    void MainWindow::onMenuValSwChanged()
{
        if (m_systemStatusActive && m_systemStatusWidget) {
            m_systemStatusWidget->selectCurrentItem();
        } else
            if (m_reticleMenuActive && m_reticleMenuWidget) {
                m_reticleMenuWidget->selectCurrentItem();
            } else if (m_colorMenuActive && m_colorMenuWidget) {
                m_colorMenuWidget->selectCurrentItem();
            } else if (m_menuActive && m_menuWidget) {
                m_menuWidget->selectCurrentItem();
            }       else if (m_stateModel->data().opMode == OperationalMode::Idle) {
                showIdleMenu();
            }/*  else if (m_stateManager->currentMode() == OperationalMode::Tracking) {
                // Select the current track ID
                QListWidgetItem* item = ui->trackIdListWidget->currentItem();
                if (item) {
                    int trackId = item->data(Qt::UserRole).toInt();
                    m_cameraSystem->setSelectedTrackId(trackId);
                    QMessageBox::information(this, "Track Selected", QString("Tracking object ID %1.").arg(trackId));
                }
            }*/
        // Handle other state modes...
    // Handle other state modes below...
}

void MainWindow::showIdleMenu()
{
    if (m_menuActive) return;

    m_menuActive = true;
    QStringList menuOptions;
    menuOptions << "System Status"
                << "Personalize Reticle"
                << "Personalize Colors"
                << "Adjust Brightness"
                << "Configure Settings"
                << "View Logs"
                << "Software Updates"
                << "Diagnostics"
                << "Help/About"
                << "Return ..."; // Add Return option

    m_menuWidget = new CustomMenuWidget(menuOptions, m_stateModel, this);
    m_menuWidget->setColorStyleChanged(m_stateModel->data().colorStyle);

    m_menuWidget->resize(250, 250);

    connect(m_menuWidget, &CustomMenuWidget::optionSelected, this, &MainWindow::handleMenuOptionSelected);
    connect(m_menuWidget, &CustomMenuWidget::menuClosed, this, &MainWindow::handleMenuClosed);
    m_menuWidget->show();
}


void MainWindow::handleMenuOptionSelected(const QString &option)
{
    if (option == "Return ...") {
        // Close the menu
        if (m_menuWidget) {
            m_menuWidget->close();
        }
    } else if (option == "System Status") {
        showSystemStatus();
    } else if (option == "Personalize Reticle") {
        personalizeReticle();
    } else if (option == "Personalize Colors") {
        personalizeColor();
    } else if (option == "Adjust Brightness") {
        adjustBrightness();
    } else if (option == "Configure Settings") {
        configureSettings();
    } else if (option == "View Logs") {
        viewLogs();
    } else if (option == "Help/About") {
        showHelpAbout();
    }
}


void MainWindow::handleMenuClosed() {
    m_menuActive = false;
    m_menuWidget = nullptr;
}

void MainWindow::showSystemStatus() {
    if (m_systemStatusActive) return;

    m_systemStatusActive = true;
    QStringList systemStatus = {"Return ...", "All systems operational."};

    m_systemStatusWidget = new CustomMenuWidget(systemStatus,  m_stateModel, this);
    m_systemStatusWidget->setColorStyleChanged(m_stateModel->data().colorStyle);
    m_systemStatusWidget->resize(250, 80);

    connect(m_systemStatusWidget, &CustomMenuWidget::optionSelected, this, [this](const QString &option) {
        if (option == "Return ...") {
            m_systemStatusWidget->close();
            showIdleMenu();
        }
    });

    connect(m_systemStatusWidget, &CustomMenuWidget::menuClosed, this, [this]() {
        m_systemStatusActive = false;
        m_systemStatusWidget = nullptr;
    });
    m_systemStatusWidget->show();

}

void MainWindow::personalizeReticle()
{
    if (m_reticleMenuActive) return;

    m_reticleMenuActive = true;
    QStringList reticleOptions = {"Default", "Crosshair", "Dot", "Circle", "Return ..."};
    m_reticleMenuWidget = new CustomMenuWidget(reticleOptions, m_stateModel, this);
    m_reticleMenuWidget->setColorStyleChanged(m_stateModel->data().colorStyle);
    m_reticleMenuWidget->resize(250, 150);
    // Update OSD as user navigates
    connect(m_reticleMenuWidget, &CustomMenuWidget::currentItemChanged, this, [this](const QString &currentItem) {
        if (currentItem != "Return ...") {
            //m_dataModel->setReticleStyle(currentItem);
        }
    });

    connect(m_reticleMenuWidget, &CustomMenuWidget::optionSelected, this, [this](const QString &option) {
        if (option == "Return ...") {
            m_reticleMenuWidget->close();
            showIdleMenu();
        } else {
            m_stateModel->setReticleStyle(option);
            //m_reticleMenuWidget->close();
            showIdleMenu();

        }
    });

    connect(m_reticleMenuWidget, &CustomMenuWidget::menuClosed, this, [this]() {
        m_reticleMenuActive = false;
        m_reticleMenuWidget = nullptr;
    });

    m_reticleMenuWidget->show();
}

void MainWindow::personalizeColor()
{
    if (m_colorMenuActive) return;

    m_colorMenuActive = true;
    QStringList colorleOptions = {"Default", "Red", "Green", "White", "Return ..."};
    m_colorMenuWidget = new CustomMenuWidget(colorleOptions,  m_stateModel, this);
    m_colorMenuWidget->setColorStyleChanged(m_stateModel->data().colorStyle);

    m_colorMenuWidget->resize(250, 150);
    // Update OSD as user navigates
    connect(m_colorMenuWidget, &CustomMenuWidget::currentItemChanged, this, [this](const QString &currentItem) {
        if (currentItem != "Return ...") {
            //m_stateModel->setColorStyle(currentItem);
            //setTracklistColorStyle(currentItem);
        }
    });

    connect(m_colorMenuWidget, &CustomMenuWidget::optionSelected, this, [this](const QString &option) {
        if (option == "Return ...") {
            showIdleMenu();
        } else {
            m_stateModel->setColorStyle(option);
            showIdleMenu();
        }
    });

    connect(m_colorMenuWidget, &CustomMenuWidget::menuClosed, this, [this]() {
        m_colorMenuActive = false;
        m_colorMenuWidget = nullptr;
    });

    m_colorMenuWidget->show();
}

void MainWindow::adjustBrightness() {
    // Implement brightness adjustment
    // Use UP/DOWN to increase/decrease brightness
    // For simplicity, here's a placeholder
    QMessageBox::information(this, "Adjust Brightness", "Brightness adjustment not implemented yet.");
}

void MainWindow::configureSettings() {
    // Implement settings configuration menu
    QMessageBox::information(this, "Configure Settings", "Settings configuration not implemented yet.");
}

void MainWindow::viewLogs() {
    // Display logs
    QMessageBox::information(this, "View Logs", "No logs available.");
}

void MainWindow::softwareUpdates() {
    // Implement software updates logic
    QMessageBox::information(this, "Software Updates", "Software is up to date.");
}

void MainWindow::runDiagnostics() {
    // Implement diagnostics
    QMessageBox::information(this, "Diagnostics", "Diagnostics completed successfully.");
}

void MainWindow::showHelpAbout() {
    QMessageBox::information(this, "Help/About", "Application Version 1.0\nDeveloped by Your Company.");
}


void MainWindow::closeAppAndHardware() {
    // 1. Perform any necessary cleanup first (e.g., closing hardware, saving state, etc.)
    // cleanupHardware(); // (Your cleanup code here)

    // 2. Create a DBus interface to request a system shutdown.
    QDBusInterface iface("org.freedesktop.login1",
                         "/org/freedesktop/login1",
                         "org.freedesktop.login1.Manager",
                         QDBusConnection::systemBus());

    if (!iface.isValid()) {
        qWarning() << "DBus interface for login1 is invalid:"
                   << QDBusConnection::systemBus().lastError().message();
    } else {
        // Declare the reply variable in this scope
        //QDBusReply<void> reply = iface.call("PowerOff", true);
        //if (!reply.isValid()) {
        //    qWarning() << "DBus call to PowerOff failed:" << reply.error().message();
        //}
    }

    // 3. Quit the Qt application.
    //qApp->quit();
}


void MainWindow::onSelectedTrackLost(int trackId)
{
    QMessageBox::information(this, "Track Lost", QString("Selected object ID %1 is no longer being tracked.").arg(trackId));

    // Reset the selection in the QListWidget
    ui->trackIdListWidget->clearSelection(); // Or use setCurrentItem(nullptr)

    // Notify the camera pipeline of the deselection
    m_cameraCtrl->setSelectedTrackId(-1);
}


void MainWindow::onTrackedIdsUpdated(const QSet<int>& trackIds)
{
    pendingTrackIds = trackIds;
    updatePending = true;
}

void MainWindow::processPendingUpdates()
{
    if (updatePending)
    {
        // Get the currently selected track ID
        QListWidgetItem* currentItem = ui->trackIdListWidget->currentItem();
        int currentTrackId = currentItem ? currentItem->data(Qt::UserRole).toInt() : -1;

        // Build a set of existing track IDs for quick lookup
        QSet<int> existingTrackIds;
        for (int i = 0; i < ui->trackIdListWidget->count(); ++i)
        {
            QListWidgetItem* item = ui->trackIdListWidget->item(i);
            if (item) {
                int id = item->data(Qt::UserRole).toInt();
                existingTrackIds.insert(id);
            }
        }

        // Add new track IDs
        for (int id : pendingTrackIds)
        {
            if (!existingTrackIds.contains(id))
            {
                QListWidgetItem* newItem = new QListWidgetItem(QString::number(id));
                newItem->setData(Qt::UserRole, id); // Store the track ID
                ui->trackIdListWidget->addItem(newItem);
            }
        }

        // Remove track IDs that no longer exist
        for (int i = ui->trackIdListWidget->count() - 1; i >= 0; --i)
        {
            QListWidgetItem* item = ui->trackIdListWidget->item(i);
            if (item)
            {
                int id = item->data(Qt::UserRole).toInt();
                if (!pendingTrackIds.contains(id))
                {
                    delete ui->trackIdListWidget->takeItem(i); // Properly delete the item to avoid memory leaks
                }
            }
        }

        // Restore previous selection if still valid
        int index = findItemIndexByData(ui->trackIdListWidget, currentTrackId);
        if (index != -1)
        {
            ui->trackIdListWidget->setCurrentRow(index);
        }
        else
        {
            // If previous selection is no longer valid, clear the selection
            ui->trackIdListWidget->clearSelection();
            m_cameraCtrl->setSelectedTrackId(-1);
        }

        updatePending = false;
    }
}

void MainWindow::onTrackIdSelected(QListWidgetItem* current, QListWidgetItem* previous)
{
    Q_UNUSED(previous); // Avoid compiler warnings if 'previous' is unused

    if (current)
    {
        int trackId = current->data(Qt::UserRole).toInt();
        m_cameraCtrl->setSelectedTrackId(trackId);
    }
    else
    {
        // No selection
        m_cameraCtrl->setSelectedTrackId(-1);
    }
}

int MainWindow::findItemIndexByData(QListWidget* listWidget, int data) const
{
    for (int i = 0; i < listWidget->count(); ++i)
    {
        QListWidgetItem* item = listWidget->item(i);
        if (item && item->data(Qt::UserRole).toInt() == data)
        {
            return i;
        }
    }
    return -1; // Not found
}

void MainWindow::setTracklistColorStyle(const QString &style)
{
    // Update the reticle rendering parameters based on the new style
    QString m_currentColorStyle = style;
    if (m_currentColorStyle == "Red") {
        ui->trackIdListWidget->setStyleSheet(
            "QListWidget {"
            "  background-color: rgba(0, 0, 0, 100);"
            "  color: rgba(200, 0, 0, 255);"
            "  font: 700 14pt 'Courier New';"
            "}"
            "QListWidget::item:selected {"
            "  color: white;"
            "  background: rgba(200, 0, 0, 255);"
            "  border: 1px solid white;"
            "}"
            );
    }
    else if (m_currentColorStyle == "Green") {
        ui->trackIdListWidget->setStyleSheet(
            "QListWidget {"
            "  background-color: rgba(0, 0, 0, 100);"
            "  color: rgba(0, 212, 76, 255);"
            "  font: 700 14pt 'Courier New';"
            "}"
            "QListWidget::item:selected {"
            "  color: white;"
            "  background: rgba(0, 212, 76, 255);"
            "  border: 1px solid white;"
            "}"
            );
    }
    else if (m_currentColorStyle == "White") {
        ui->trackIdListWidget->setStyleSheet(
            "QListWidget {"
            "  background-color: rgba(0, 0, 0, 100);"
            "  color: rgba(255, 255, 255, 255);"
            "  font: 700 14pt 'Courier New';"
            "}"
            "QListWidget::item:selected {"
            "  color: white;"
            "  background: rgba(255, 255, 255, 255);"
            "  color: rgba(0, 0, 0, 255);"
            "  border: 1px solid white;"
            "}"
            );
    }
    else {
        ui->trackIdListWidget->setStyleSheet(
            "QListWidget {"
            "  background-color: rgba(0, 0, 0, 100);"
            "  color: rgba(0, 212, 76, 255);"
            "  font: 700 14pt 'Courier New';"
            "}"
            "QListWidget::item:selected {"
            "  color: white;"
            "  background: rgba(0, 212, 76, 255);"
            "  border: 1px solid white;"
            "}"
            );
    }

}

void MainWindow::onUpTrackChanged(bool state)
{
    if (state) {
        int currentRow = ui->trackIdListWidget->currentRow();
        if (currentRow > 0) { // Check if not already at the first item
            ui->trackIdListWidget->setCurrentRow(currentRow - 1);
        }
    }
}

void MainWindow::onDownTrackChanged(bool state)
{
    if (state) {
        int currentRow = ui->trackIdListWidget->currentRow();
        if (currentRow < ui->trackIdListWidget->count() - 1) { // Check if not already at the last item
            ui->trackIdListWidget->setCurrentRow(currentRow + 1);
        }
    }
}

void MainWindow::onTrackSelectButtonPressed()
{

       auto *item = ui->trackIdListWidget->currentItem();
        if (item) {
            int trackId = item->data(Qt::UserRole).toInt();
            m_cameraCtrl->setSelectedTrackId(trackId);
            qDebug() << "Track ID selected: " << trackId;
        } else {
            m_cameraCtrl->setSelectedTrackId(-1);
            qDebug() << "No track selected.";
        }

}

// **************  TEsting tools ********************

void MainWindow::on_opmode_clicked()
{
    m_stateModel->setOpMode(OperationalMode::Surveillance);
}


void MainWindow::on_fireOn_clicked()
{
    m_weaponCtrl->startFiring();
}


void MainWindow::on_fieOff_clicked()
{
    m_weaponCtrl->stopFiring();
}


void MainWindow::on_mode_clicked()
{
    /*if (m_stateModel->data().opMode == OperationalMode::Tracking) {
        m_stateModel->setOpMode(OperationalMode::Surveillance);
        m_stateModel->setMotionMode(MotionMode::Manual);
    }
    else {
        m_stateModel->setOpMode(OperationalMode::Tracking);
        m_stateModel->setMotionMode( MotionMode::AutoTrack );
    }*/

    const bool enteringTracking = 
    (m_stateMachine->currentState() != SystemStateMachine::Tracking);

    if (enteringTracking) {
        // Get valid initial mode for current camera
        const MotionMode initialMode = m_stateModel->data().activeCameraIsDay 
            ? MotionMode::AutoTrack 
            : MotionMode::ManualTrack;

        m_stateMachine->setState(SystemStateMachine::Tracking);
        m_stateModel->setMotionMode(initialMode);
    } else {
        m_stateMachine->setState(SystemStateMachine::Surveillance);
        m_stateModel->setMotionMode(MotionMode::Manual);
    }

}


void MainWindow::on_track_clicked()
{
    if (m_stateModel->data().motionMode == MotionMode::ManualTrack) {
        m_cameraCtrl->startTracking();
        /*if (!m_stateModel->data().startTracking) {
            m_stateModel->setTrackingStarted(true);
            qDebug() << "Joystick pressed: starting tracking.";
        } else {
            // Toggle restart flag to force state update.
            m_stateModel->setTrackingRestartRequested(false);
            m_stateModel->setTrackingRestartRequested(true);
            qDebug() << "Joystick pressed: tracking restart requested.";
        }*/
    }
}


void MainWindow::on_motion_clicked()
{

    OperationalMode opMode = m_stateModel->data().opMode;
    MotionMode motionMode = m_stateModel->data().motionMode;

    if (opMode == OperationalMode::Surveillance) {
        // cycle between Manual and Pattern
        if (motionMode == MotionMode::Manual) {
            m_stateModel->setMotionMode(MotionMode::Pattern);
        } else {
            m_stateModel->setMotionMode(MotionMode::Manual);
        }

    } else if (opMode == OperationalMode::Tracking) {
        MotionMode nextMode = MotionMode::ManualTrack;
        // Only do AutoTrack if day camera
        if (m_stateModel->data().activeCameraIsDay) {
            nextMode = (motionMode == MotionMode::AutoTrack)
                ? MotionMode::ManualTrack
                : MotionMode::AutoTrack;
        }
        m_stateModel->setMotionMode(nextMode);
        
    }



}




void MainWindow::on_up_clicked()
{
    if (m_stateModel->data().opMode  == OperationalMode::Idle) {
        m_stateModel->setUpSw(true);
    } else if (m_stateModel->data().opMode  == OperationalMode::Tracking) {
        m_stateModel->setUpTrack(true);
    }
}


void MainWindow::on_down_clicked()
{
    if (m_stateModel->data().opMode  == OperationalMode::Idle) {
        m_stateModel->setDownSw(!m_stateModel->data().downTrackButton);
    } else if (m_stateModel->data().opMode  == OperationalMode::Tracking) {
        m_stateModel->setDownTrack(false);
        m_stateModel->setDownTrack(true);
    }
}


void MainWindow::on_autotrack_clicked()
{
    m_cameraCtrl->startTracking();
    /*if (m_stateModel->data().motionMode == MotionMode::ManualTrack) {
        if (!m_stateModel->data().startTracking) {
            m_stateModel->setTrackingStarted(true);
            qDebug() << "Joystick pressed: starting tracking.";
        } else {
            // Toggle restart flag to force state update.
            m_stateModel->setTrackingRestartRequested(false);
            m_stateModel->setTrackingRestartRequested(true);
            qDebug() << "Joystick pressed: tracking restart requested.";
        }
    }
    else if (m_stateModel->data().motionMode == MotionMode::AutoTrack){
        onTrackSelectButtonPressed();
    }*/
}


void MainWindow::switchCameraWidget(QWidget* fromWidget, QWidget* toWidget)
{
    // Remove the old widget if it exists in the layout
    if (fromWidget && m_layout->indexOf(fromWidget) != -1) {
        m_layout->removeWidget(fromWidget);
        fromWidget->setVisible(false);
    }

    // Add the new widget if it's not already in the layout
    if (toWidget && m_layout->indexOf(toWidget) == -1) {
        m_layout->addWidget(toWidget);
        toWidget->setVisible(true);
    }
}

void MainWindow::on_day_clicked()
{
    // Let the state model handle the camera switching logic
    if (m_stateModel) {
        m_stateModel->setActiveCameraIsDay(!m_isDayCameraActive);
    }
}


void MainWindow::on_night_clicked()
{
    m_stateModel->setActiveCameraIsDay(false);

}

void MainWindow::on_quit_clicked()
{
    QCoreApplication::quit();

}

void MainWindow::onAlarmDetected(uint16_t alarmCode, const QString &description)
{
    qDebug() << "Alarm detected: " << alarmCode << description;
    // Update UI with alarm information
    // e.g. show alarm code and description in a dialog
    // QMessageBox::warning(this, "Alarm Detected", QString("Alarm %1: %2").arg(alarmCode).arg(description));
}

void MainWindow::onAlarmCleared()
{
    qDebug() << "Alarm cleared.";
}

void MainWindow::onAlarmHistoryRead(const QList<uint16_t> &alarmHistory)
{
}

void MainWindow::onAlarmHistoryCleared()
{
}

void MainWindow::on_read_clicked()
{
    m_gimbalCtrl->readAlarms();
}


void MainWindow::on_clear_clicked()
{
    m_gimbalCtrl->clearAlarms();
}

 
