TEMPLATE	= app
LANGUAGE	= C++

CONFIG	+= qt warn_on release

DEFINES += CONFIG_CTRL_IFACE WPS_OPT_UPNP WPS_OPT_NFC

win32 {
  LIBS += -lws2_32
  DEFINES += CONFIG_NATIVE_WINDOWS CONFIG_CTRL_IFACE_NAMED_PIPE USE_WINIFLIST
  SOURCES += ../win_if_list.c
} else:win32-g++ {
  # cross compilation to win32
  LIBS += -lws2_32
  DEFINES += CONFIG_NATIVE_WINDOWS CONFIG_CTRL_IFACE_NAMED_PIPE USE_WINIFLIST
  SOURCES += ../win_if_list.c
} else {
  DEFINES += CONFIG_CTRL_IFACE_UNIX
}

INCLUDEPATH	+= . .. ../../common

HEADERS	+= \
	testbedap.h \
	about.h \
	mainprocess.h \
	pagetemplate.h \
	setupinterface.h \
	netconfig.h \
	wepkey.h \
	status.h \
	debugwindow.h \
	wpamsg.h \

SOURCES	+= \
	main.cpp \
	testbedap.cpp \
	mainprocess.cpp \
	setupinterface.cpp \
	netconfig.cpp \
	wepkey.cpp \
	status.cpp \
	../../common/wpa_ctrl.c \

FORMS	= \
	testbedap.ui \
	about.ui \
	setupinterface.ui \
	netconfig.ui \
	wepkey.ui \
	status.ui \
	debugwindow.ui \


unix {
  UI_DIR = .ui
  MOC_DIR = .moc
  OBJECTS_DIR = .obj
}

