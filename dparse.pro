TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    dparse.cpp \
    FiniteAutomata.cpp

LIBS += -llua

HEADERS += \
    dparse.h \
    FiniteAutomata.h
