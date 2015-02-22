/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: mainprocess.h
//  Description: main process control header
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in
//       the documentation and/or other materials provided with the
//       distribution.
//     * Neither the name of Sony Corporation nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**************************************************************************/

#ifndef MAINPROCESS_H
#define MAINPROCESS_H

#include <QProcess>
#include <QMutex>
#if defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
#include <QSocketNotifier>
#endif // defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)

struct wpa_ctrl;

#define CTRL_REQ_EAP_WPS_COMP		"EAP_WPS_COMP"
#define CTRL_REQ_EAP_WPS_FAIL		"EAP_WPS_FAIL"
#define CTRL_REQ_EAP_WPS_PASSWORD	"EAP_WPS_PASSWORD"

#define CTRL_REQ_UPNP_COMP			"UPNP_COMP"
#define CTRL_REQ_UPNP_FAIL			"UPNP_FAIL"
#define CTRL_REQ_UPNP_PASSWORD		"UPNP_PASSWORD"

#define CTRL_REQ_NFC_READ_TIMEOUT	"NFC_READ_TIMEOUT"
#define CTRL_REQ_NFC_WRITE_TIMEOUT	"NFC_WRITE_TIMEOUT"
#define CTRL_REQ_NFC_FAIL_READ		"NFC_FAIL_READ"
#define CTRL_REQ_NFC_COMP_READ		"NFC_COMP_READ"
#define CTRL_REQ_NFC_COMP_WRITE		"NFC_COMP_WRITE"
#define CTRL_REQ_NFC_ADD_NEW_AP		"NFC_ADD_NEW_AP"


class MainProcess
{
public:
	MainProcess();
	~MainProcess();

	enum MODE {
		MODE_ENR_GETCONF = 0,
		MODE_REG_REGSTA
	};

	enum METHOD {
		METHOD_NONE = 0,
		METHOD_NFC,
		METHOD_PIN,
		METHOD_PBC
	};

	enum AUTHENTICATION {
		AUTH_OPEN = 0,
		AUTH_WPA,
		AUTH_WPA2,
	};

	enum ENCRIPTION {
		ENCR_NONE = 0,
		ENCR_WEP,
		ENCR_TKIP,
		ENCR_CCMP
	};

	enum WPS_STATE {
		WPS_STATE_NOTCONFIGURED = 1,
		WPS_STATE_CONFIGURED
	};

	enum WIRELESS_MODE {
		WIRELESS_MODE_11AGB = 0,
		WIRELESS_MODE_11A,
		WIRELESS_MODE_11GB
	};

	static bool start();
	static void terminate();

private:
	static int ctrlRequest(char *cmd, char *res, size_t *len);

public:
	static bool readConfigFile(char *filename = 0);
	static bool writeConfigFile();

#if defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
	static bool connectMonitor(QObject *receiver, const char *method);
	static bool disconnectMonitor(QObject *receiver, const char *method);
#endif // defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
	static bool ctrlPending();
	static bool receive(char *msg, size_t *len);
	static bool getCtrlRequelst(char *buf, size_t len, int *priority, char *req, char *msg);
	static void closeCtrl();

	static bool reload();
	static bool getStatus(char *result, size_t *len);

	static bool setRegMode(int regmode);
	static bool setWpsPassword(const char *pwd);
	static bool clearWpsPassword();
	static bool ctrlWpsState(uchar state);

	static bool writeNfcConfig();
	static bool readNfcConfig();
	static bool writeNfcPassword();
	static bool readNfcPassword();
	static bool cancelScanNfcToken();

	static void generatePIN(char pwd[9]);
	static bool validatePIN(const char pwd[9]);

	static bool startPbc();
	static bool stopPbc();

	static bool setDebugOut(QObject *receiver, const char *method);
	static QString readDebugMsg();

	static void setMode(MODE _mode) { mode = _mode; };
	static MODE getMode() { return mode; };

	static void setMethod(METHOD  _method) { method = _method; };
	static METHOD getMethod() { return method; };

	static void setWirelessInterface(const char *ifname);
	static char *getWirelessInterface();
	static void setWirelessDriver(const char *driver);
	static char *setWirelessDriver();
	static void setNfcInterface(const char *ifname);
	static char *getNfcInterface();
	static void setWiredInterface(const char *ifname);
	static char *getWiredInterface();
	static void setBridgeInterface(const char *ifname);
	static char *getBridgeInterface();
	static void setIpAddress(const char *ipAddr);
	static char *getIpAddress();
	static void setNetMask(const char *netMask);
	static char *getNetMask();
	static void setSsid(const char *_ssid);
	static const char *getSsid();
	static void setAuthType(ushort auth);
	static ushort getAuthType();
	static void setEncrType(ushort encr);
	static ushort getEncrType();
	static void setNetKey(const char *key);
	static const char *getNetKey();
	static void setWepKeyIndex(ushort index);
	static ushort getWepKeyIndex();
	static void setWepKey(ushort key, const char *key);
	static const char *getWepKey(ushort index);
	static void setWpsState(uchar state);
	static uchar getWpsState();

	static void setWirelessMode(WIRELESS_MODE _mode) { wirelessMode = _mode; };
	static WIRELESS_MODE getWirelessMode() { return wirelessMode; };
	static void setChannelIndex(int index) { channelIndex = index; };
	static int getChannelIndex() { return channelIndex; };
	static const char *getChannelList(WIRELESS_MODE _mode);

	static bool setChannel(WIRELESS_MODE _mode, int channel);

private slots:
	void receiveMsgs();

private:
	static QProcess	*mainProcess;

	static struct wpa_ctrl *monitor;
	static struct wpa_ctrl *ctrl;

	static char *iface;

	static QMutex *mtx;

	static MODE mode;
	static METHOD method;

	static char *wirelessInterface;
	static char *wirelessDriver;
	static char *wiredInterface;
	static char *bridgeInterface;
	static char *nfcInterface;

	static char *ipAddress;
	static char *netMask;

	static char ssid[32 + 1];
	static ushort authType;
	static ushort encrType;
	static char netKey[64 + 1];
	static uchar wpsState;
	static ushort wepKeyIndex;
	static char wepKey[4][27];

	static WIRELESS_MODE wirelessMode;
	static int channel;
	static int channelIndex;
	static char *channelList;

#if defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
	static QSocketNotifier *msgNotifier;
#endif // defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
};

#endif // MAINPROCESS_H
