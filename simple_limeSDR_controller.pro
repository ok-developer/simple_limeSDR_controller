QT -= gui
QT += core network

CONFIG += c++17 console
CONFIG -= app_bundle

TARGET      = simple_limeSDR_controller
TEMPLATE    = app
UI_DIR      = ui
MOC_DIR     = moc
OBJECTS_DIR = obj
RCC_DIR     = rcc
DESTDIR     = deploy
INSTALLS        = target
target.files    = $${TARGET}
target.path     = $${TARGET}

DEFINES += QT_DEPRECATED_WARNINGS

LIBS += -lLimeSuite

SOURCES += \
        Application.cpp \
        hardware/LimeSDRDevice.cpp \
        main.cpp \
        types/RxMissionConfig.cpp \
        types/TxMissionConfig.cpp

HEADERS += \
        Application.hpp \
        hardware/LimeSDRDevice.hpp \
        types/AbstractMissionConfig.hpp \
        types/RxMissionConfig.hpp \
        types/TxMissionConfig.hpp

DISTFILES += \
    README.md
