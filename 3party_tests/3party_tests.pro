# Tests for 3party libraries

TARGET = 3party_tests
CONFIG += console warn_on
CONFIG -= app_bundle
TEMPLATE = app

ROOT_DIR = ..
DEPENDENCIES = base torrent

include($$ROOT_DIR/common.pri)

INCLUDEPATH *= $$ROOT_DIR/3party/jansson/src $$ROOT_DIR/3party/torrent/include

SOURCES += \
  $$ROOT_DIR/testing/testingmain.cpp \
  torrent_tests.cpp \

HEADERS +=
