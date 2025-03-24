QT       += core gui serialbus serialport openglwidgets dbus

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

#CONFIG += opengles2

#INCLUDEPATH += "/usr/include/vpi3"
INCLUDEPATH += "/opt/nvidia/vpi3/include"
LIBS += -L/opt/nvidia/vpi3/lib/aarch64-linux-gnu -lnvvpi
LIBS += -lSDL2


# Jetson-specific configurations
INCLUDEPATH +="/usr/local/cuda-12.6/targets/aarch64-linux/include"
INCLUDEPATH +="/opt/nvidia/deepstream/deepstream/sources/includes"
LIBS += -L/usr/local/cuda-12.6/lib64 -lcudart
LIBS += -L/opt/nvidia/deepstream/deepstream/lib -lnvdsgst_meta -lnvds_meta
LIBS += -L/usr/lib/aarch64-linux-gnu/tegra -lnvbufsurface -lnvbufsurftransform
LIBS+=-L"/usr/lib/aarch64-linux-gnu/gstreamer-1.0" -lgstxvimagesink -L"/usr/lib/aarch64-linux-gnu" -lgstbase-1.0 -lgstreamer-1.0 -lglib-2.0 -lgobject-2.0


# Common configurations
#INCLUDEPATH += "/usr/include/opencv4"
INCLUDEPATH += "/usr/local/include/opencv4"
INCLUDEPATH += "/usr/include/eigen3"
INCLUDEPATH += "/usr/include/glib-2.0"
INCLUDEPATH += "/usr/include/gstreamer-1.0"

CONFIG += link_pkgconfig
PKGCONFIG += gstreamer-1.0
PKGCONFIG += gstreamer-video-1.0

#INCLUDEPATH += /usr/include/freetype2
#LIBS += -lfreetype
#LIBS += -L/usr/local/lib -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio
LIBS += -lgstreamer-1.0 -lgstapp-1.0 -lgstbase-1.0 -lgobject-2.0 -lglib-2.0
LIBS += -L/usr/local/lib -lopencv_core -lopencv_imgcodecs -lopencv_highgui -lopencv_imgproc

PKGCONFIG += gstreamer-gl-1.0


SOURCES += \
    controllers/cameracontroller.cpp \
    controllers/gimbalcontroller.cpp \
    controllers/joystickcontroller.cpp \
    controllers/motion_modes/manualmotionmode.cpp \
    controllers/motion_modes/trackingmotionmode.cpp \
    controllers/weaponcontroller.cpp \
    core/systemcontroller.cpp \
    core/systemstatemachine.cpp \
    devices/basecamerapipelinedevice.cpp \
    devices/daycamerapipelinedevice.cpp \
    devices/nightcamerapipelinedevice.cpp \
    devices/videodisplaywidget.cpp \
    main.cpp \
    ui/mainwindow.cpp \
    ui/custommenudialog.cpp \
    devices/servoactuatordevice.cpp \
    devices/plc42device.cpp \
    devices/plc21device.cpp \
    devices/nightcameracontroldevice.cpp \
    devices/daycameracontroldevice.cpp \
    devices/lrfdevice.cpp \
    devices/joystickdevice.cpp \
    devices/lensdevice.cpp \
    devices/servodriverdevice.cpp \
    devices/gyrodevice.cpp \
    models/joystickdatamodel.cpp \
    models/systemstatemodel.cpp \
    utils/cameracontainerwidget.cpp \
    utils/dcftrackervpi.cpp \
    utils/videoglwidget_gl.cpp

HEADERS += \
    controllers/cameracontroller.h \
    controllers/gimbalcontroller.h \
    controllers/joystickcontroller.h \
    controllers/motion_modes/gimbalmotionmodebase.h \
    controllers/motion_modes/manualmotionmode.h \
    controllers/motion_modes/trackingmotionmode.h \
    controllers/weaponcontroller.h \
    core/systemcontroller.h \
    core/systemstatemachine.h \
    devices/basecamerapipelinedevice.h \
    devices/daycamerapipelinedevice.h \
    devices/nightcamerapipelinedevice.h \
    devices/videodisplaywidget.h \
    models/gyrodatamodel.h \
    models/lensdatamodel.h \
    models/nightcameradatamodel.h \
    ui/mainwindow.h \
    ui/custommenudialog.h \
    devices/servoactuatordevice.h \
    devices/plc42device.h \
    devices/plc21device.h \
    devices/nightcameracontroldevice.h \
    devices/daycameracontroldevice.h \
    devices/lrfdevice.h \
    devices/joystickdevice.h \
    devices/lensdevice.h \
    devices/servodriverdevice.h \
    devices/gyrodevice.h \
    models/daycameradatamodel.h \
    models/joystickdatamodel.h \
    models/lrfdatamodel.h \
    models/plc21datamodel.h \
    models/plc42datamodel.h \
    models/servoactuatordatamodel.h \
    models/servodriverdatamodel.h \
    models/systemstatedata.h \
    models/systemstatemodel.h \
    utils/cameracontainerwidget.h \
    utils/millenious.h \
    utils/dcftrackervpi.h \
    utils/targetstate.h \
    utils/videoglwidget_gl.h

FORMS += \
    ui/mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
