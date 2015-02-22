/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: mainprocess.cpp
//  Description: main process control source
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

#include "mainprocess.h"
#include "wpa_ctrl.h"
#include "os.h"

#include <QTemporaryFile>

#define WAIT_FOR_PROCESS	3000 // [msec]

#define HOSTAPD				"./hostapd"

#define HOSTAPD_CTRL_DIR	"/var/run/hostapd"
#define HOSTAPD_CONF		"./hostapd.conf"


MainProcess::MainProcess()
{
}

MainProcess::~MainProcess()
{
	terminate();

	if (mtx) {
		mtx->unlock();
		delete mtx;
		mtx = 0;
	}

	if (mainProcess) {
		delete mainProcess;
		mainProcess = 0;
	}

	if (iface) {
		os_free(iface);
		iface = 0;
	}

	if (wirelessInterface) {
		os_free(wirelessInterface);
		wirelessInterface = 0;
	}

	if (wirelessDriver) {
		os_free(wirelessDriver);
		wirelessDriver = 0;
	}

	if (wiredInterface) {
		os_free(wiredInterface);
		wiredInterface = 0;
	}

	if (bridgeInterface) {
		os_free(bridgeInterface);
		bridgeInterface = 0;
	}

	if (ipAddress) {
		os_free(ipAddress);
		ipAddress = 0;
	}

	if (netMask) {
		os_free(netMask);
		netMask = 0;
	}

	if (nfcInterface) {
		os_free(nfcInterface);
		nfcInterface = 0;
	}
}

bool MainProcess::readConfigFile(char *filename /* = 0 */)
{
	bool ret = false;
	QFile *conf = new QFile(!filename?HOSTAPD_CONF:filename);
	char line[BUFSIZ];
	char *pos, *var, *value;
	qint64 end;

	do {
		if (!conf || !conf->exists())
			break;

		if (!conf->open(QIODevice::ReadOnly))
			break;

		os_memset(netKey, 0, sizeof(netKey));

		while (!conf->atEnd()) {
			if(-1 != (end = conf->readLine(line, sizeof(line)))) {
				line[end] = 0;
				pos = line;

				while (*pos == ' ' || *pos == '\t' || *pos == '\r')
					pos++;
				if (*pos == '#' || *pos == '\n' || *pos == '\0' || *pos == '\r')
					continue;

				var = pos;
				pos = os_strchr(var, '=');
				if (pos) {
					*pos++ = 0;
					value = pos;
					while (*pos != 0) {
						if ((*pos == '\n') || (*pos == '\r')) {
							*pos = 0;
							break;
						}
						pos++;
					}
				} else
					continue;

				if (var && value) {
					if (0 == os_strcmp(var, "interface")) {
						if (wirelessInterface) {
							os_free(wirelessInterface);
							wirelessInterface = 0;
						}
						wirelessInterface = os_strdup(value);
					} else if (0 == os_strcmp(var, "driver")) {
						if (wirelessDriver) {
							os_free(wirelessDriver);
							wirelessDriver = 0;
						}
						wirelessDriver = os_strdup(value);
					} else if (0 == os_strcmp(var, "bridge")) {
						if (bridgeInterface) {
							os_free(bridgeInterface);
							bridgeInterface = 0;
						}
						bridgeInterface = os_strdup(value);
					} else if (0 == os_strcmp(var, "own_ip_addr")) {
						if (ipAddress) {
							os_free(ipAddress);
							ipAddress = 0;
						}
						ipAddress = os_strdup(value);
					} else if (0 == os_strcmp(var, "nfc")) {
						if (nfcInterface) {
							os_free(nfcInterface);
							nfcInterface = 0;
						}
						nfcInterface = os_strdup(value);
					} else if (0 == os_strcmp(var, "ssid")) {
						os_memset(ssid, 0, sizeof(ssid));
						os_snprintf(ssid, sizeof(ssid), "%s", value);
					} else if (0 == os_strcmp(var, "wpa")) {
						authType = atoi(value);
					} else if (0 == os_strcmp(var, "wpa_pairwise")) {
						if (os_strstr(value, "CCMP"))
							encrType = ENCR_CCMP;
						else if (os_strstr(value, "TKIP"))
							encrType = ENCR_TKIP;
					} else if ((0 == os_strcmp(var, "wpa_psk")) ||
							   (0 == os_strcmp(var, "wpa_passphrase"))) {
						os_memset(netKey, 0, sizeof(netKey));
						os_snprintf(netKey, sizeof(netKey), "%s", value);
					} else if (0 == os_strcmp(var, "wep_default_key")) {
						wepKeyIndex = atoi(value) + 1;
						encrType = ENCR_WEP;
					} else if (0 == os_strcmp(var, "wep_key0")) {
						os_memset(&wepKey[0][0], 0, sizeof(wepKey[0]));
						if (*value == '"') {
							if (os_strrchr(value + 1, '"'))
								*os_strrchr(value + 1, '"') = 0;
							os_snprintf(&wepKey[0][0], sizeof(wepKey[0]), "%s", value + 1);
						} else
							os_snprintf(&wepKey[0][0], sizeof(wepKey[0]), "%s", value);
					} else if (0 == os_strcmp(var, "wep_key1")) {
						os_memset(&wepKey[1][0], 0, sizeof(wepKey[1]));
						if (*value == '"') {
							if (os_strrchr(value + 1, '"'))
								*os_strrchr(value + 1, '"') = 0;
							os_snprintf(&wepKey[1][0], sizeof(wepKey[1]), "%s", value + 1);
						} else
							os_snprintf(&wepKey[1][0], sizeof(wepKey[1]), "%s", value);
					} else if (0 == os_strcmp(var, "wep_key2")) {
						os_memset(&wepKey[2][0], 0, sizeof(wepKey[2]));
						if (*value == '"') {
							if (os_strrchr(value + 1, '"'))
								*os_strrchr(value + 1, '"') = 0;
							os_snprintf(&wepKey[2][0], sizeof(wepKey[2]), "%s", value + 1);
						} else
							os_snprintf(&wepKey[2][0], sizeof(wepKey[2]), "%s", value);
					} else if (0 == os_strcmp(var, "wep_key3")) {
						os_memset(&wepKey[3][0], 0, sizeof(wepKey[3]));
						if (*value == '"') {
							if (os_strrchr(value + 1, '"'))
								*os_strrchr(value + 1, '"') = 0;
							os_snprintf(&wepKey[3][0], sizeof(wepKey[3]), "%s", value + 1);
						} else
							os_snprintf(&wepKey[3][0], sizeof(wepKey[3]), "%s", value);
					} else if (0 == os_strcmp(var, "wps_state")) {
						wpsState = atoi(value);
					}
				}
			}
		}

		ret = true;
	} while (0);

	if (conf) {
		if (conf->isOpen())
			conf->close();
		delete conf;
	}

	return ret;
}

bool MainProcess::writeConfigFile()
{
	bool ret = false;
	QFile *conf = new QFile(HOSTAPD_CONF);
	QTemporaryFile *tmp = new QTemporaryFile();
	char line[BUFSIZ], buf[BUFSIZ];
	char *pos, *var, *value, *pos2;
	qint64 end;
	int remain;
	bool setWirelessInterface = false, setWirelessDriver = false,
		 setNfcInterface = false, setBridgeInterface = false, setIpAddress = false,
		 setSsid = false, setAuth = false, setEncr = false,
		 setKeyMgmt = false, setNetKey = false,
		 setWepKeyIndex = false, setWepKey[4] = {false, false, false, false},
		 setWpsState = false, setHwMode = false, setChan = false;

	do {
		if (!conf || !conf->exists())
			break;

		if (!conf->open(QIODevice::ReadOnly))
			break;

		if (!tmp->open())
			break;

		while (!conf->atEnd()) {
			if(-1 != (end = conf->readLine(line, sizeof(line)))) {
				do {
					line[end] = 0;
					os_snprintf(buf, sizeof(buf), "%s", line);
					pos = buf;

					while (*pos == ' ' || *pos == '\t' || *pos == '\r')
						pos++;
					if (*pos == '#' || *pos == '\n' || *pos == '\0' || *pos == '\r')
						break;

					var = pos;
					pos = os_strchr(var, '=');
					if (pos) {
						*pos++ = 0;
						value = pos;
						while (*pos != 0) {
							if ((*pos == '\n') || (*pos == '\r')) {
								*pos = 0;
								break;
							}
							pos++;
						}
					} else
						break;

					if (var && value) {
						pos2 = line + (value - buf);
						remain = sizeof(line) - (pos2 - line);
						if (0 == os_strcmp(var, "interface")) {
							if (wirelessInterface)
								os_snprintf(pos2, remain, "%s\n", wirelessInterface);
							setWirelessInterface = true;
						} else if (0 == os_strcmp(var, "driver")) {
							if (wirelessDriver)
								os_snprintf(pos2, remain, "%s\n", wirelessDriver);
							setWirelessDriver = true;
						} else if (0 == os_strcmp(var, "bridge")) {
							if (bridgeInterface)
								os_snprintf(pos2, remain, "%s\n", bridgeInterface);
							setBridgeInterface = true;
						} else if (0 == os_strcmp(var, "own_ip_addr")) {
							if (ipAddress)
								os_snprintf(pos2, remain, "%s\n", ipAddress);
							setIpAddress = true;
						} else if (0 == os_strcmp(var, "nfc")) {
							if (nfcInterface)
								os_snprintf(pos2, remain, "%s\n", nfcInterface);
							setNfcInterface = true;
						} else if (0 == os_strcmp(var, "hw_mode")) {
							if ((channel > 14) && (channel <= 64))
								os_snprintf(pos2, remain, "a\n");
							else
								os_snprintf(pos2, remain, "g\n");
							setHwMode = true;
						} else if (0 == os_strcmp(var, "channel")) {
							if ((channel <= 0) || (channel > 64))
								channel = 1;
							os_snprintf(pos2, remain, "%d\n", channel);
							setChan = true;
						} else if (0 == os_strcmp(var, "ssid")) {
							os_snprintf(pos2, remain, "%s\n", ssid);
							setSsid = true;
						} else if (0 == os_strcmp(var, "wpa")) {
							os_snprintf(pos2, remain, "%d\n", authType);
							setAuth = true;
						} else if (0 == os_strcmp(var, "wpa_key_mgmt")) {
							switch (authType) {
							case AUTH_OPEN:
								line[0] = 0;
								break;
							case AUTH_WPA:
							case AUTH_WPA2:
								os_snprintf(pos2, remain, "WPA-PSK\n");
								break;
							}
							setKeyMgmt = true;
						} else if (0 == os_strcmp(var, "wpa_pairwise")) {
							if (0 < authType) {
								switch (encrType) {
								case ENCR_TKIP:
									os_snprintf(pos2, remain, "TKIP\n");
									break;
								case ENCR_CCMP:
									os_snprintf(pos2, remain, "CCMP\n");
									break;
								default:
									line[0] = 0;
									break;
								}
							} else
								line[0] = 0;
							setEncr = true;
						} else if (0 == os_strcmp(var, "wpa_psk")) {
							if ((AUTH_OPEN != authType) && !setNetKey) {
								if (64 == os_strlen(netKey))
									os_snprintf(pos2, remain, "%s\n", netKey);
								else
									os_snprintf(line, sizeof(line), "wpa_passphrase=%s\n", netKey);
								setNetKey = true;
							} else
								line[0] = 0;
						} else if (0 == os_strcmp(var, "wpa_passphrase")) {
							if ((AUTH_OPEN != authType) && !setNetKey) {
								if (64 == os_strlen(netKey))
									os_snprintf(line, sizeof(line), "wpa_psk=%s\n", netKey);
								else
									os_snprintf(pos2, remain, "%s\n", netKey);
								setNetKey = true;
							} else
								line[0] = 0;
						} else if (0 == os_strcmp(var, "wep_default_key")) {
							if ((AUTH_OPEN == authType) && (ENCR_WEP == encrType) &&
								!setWepKeyIndex) {
								os_snprintf(pos2, remain, "%d\n", wepKeyIndex - 1);
								setWepKeyIndex = true;
							} else
								line[0] = 0;
						} else if (0 == os_strcmp(var, "wep_key0")) {
							if ((AUTH_OPEN == authType) && (ENCR_WEP == encrType) &&
								!setWepKey[0]) {
								if (os_strlen(wepKey[0])) {
									if ((5 == os_strlen(wepKey[0])) || (13 == os_strlen(wepKey[0])))
										os_snprintf(pos2, remain, "\"%s\"\n", wepKey[0]);
									else
										os_snprintf(pos2, remain, "%s\n", wepKey[0]);
								} else
									line[0] = 0;
								setWepKey[0] = true;
							} else
								line[0] = 0;
						} else if (0 == os_strcmp(var, "wep_key1")) {
							if ((AUTH_OPEN == authType) && (ENCR_WEP == encrType) &&
								!setWepKey[1]) {
								if (os_strlen(wepKey[1])) {
									if ((5 == os_strlen(wepKey[1])) || (13 == os_strlen(wepKey[1])))
										os_snprintf(pos2, remain, "\"%s\"\n", wepKey[1]);
									else
										os_snprintf(pos2, remain, "%s\n", wepKey[1]);
								} else
									line[0] = 0;
								setWepKey[1] = true;
							} else
								line[0] = 0;
						} else if (0 == os_strcmp(var, "wep_key2")) {
							if ((AUTH_OPEN == authType) && (ENCR_WEP == encrType) &&
								!setWepKey[2]) {
								if (os_strlen(wepKey[2])) {
									if ((5 == os_strlen(wepKey[2])) || (13 == os_strlen(wepKey[2])))
										os_snprintf(pos2, remain, "\"%s\"\n", wepKey[2]);
									else
										os_snprintf(pos2, remain, "%s\n", wepKey[2]);
								} else
									line[0] = 0;
								setWepKey[2] = true;
							} else
								line[0] = 0;
						} else if (0 == os_strcmp(var, "wep_key3")) {
							if ((AUTH_OPEN == authType) && (ENCR_WEP == encrType) &&
								!setWepKey[3]) {
								if (os_strlen(wepKey[3])) {
									if ((5 == os_strlen(wepKey[3])) || (13 == os_strlen(wepKey[3])))
										os_snprintf(pos2, remain, "\"%s\"\n", wepKey[3]);
									else
										os_snprintf(pos2, remain, "%s\n", wepKey[3]);
								} else
									line[0] = 0;
								setWepKey[3] = true;
							} else
								line[0] = 0;
						} else if (0 == os_strcmp(var, "wps_state")) {
							os_snprintf(pos2, remain, "%d\n", wpsState);
							setWpsState = true;
						}
					}
				} while (0);

				if (line[0])
					tmp->write(line, os_strlen(line));
			}
		}

		if (!setWirelessInterface && wirelessInterface) {
			os_snprintf(line, sizeof(line), "interface=%s\n", wirelessInterface);
			tmp->write(line, os_strlen(line));
		}
		if (!setWirelessDriver && wirelessDriver) {
			os_snprintf(line, sizeof(line), "driver=%s\n", wirelessDriver);
			tmp->write(line, os_strlen(line));
		}
		if (!setBridgeInterface && bridgeInterface) {
			os_snprintf(line, sizeof(line), "bridge=%s\n", bridgeInterface);
			tmp->write(line, os_strlen(line));
		}
		if (!setIpAddress && ipAddress) {
			os_snprintf(line, sizeof(line), "own_ip_addr=%s\n", ipAddress);
			tmp->write(line, os_strlen(line));
		}
		if (!setNfcInterface && nfcInterface) {
			#if 0 // HACK -Ted
			os_snprintf(line, sizeof(line), "nfc=%s\n", nfcInterface);
			tmp->write(line, os_strlen(line));
			#endif	// HACK
		}
		if (!setSsid && ssid) {
			os_snprintf(line, sizeof(line), "ssid=%s\n", ssid);
			tmp->write(line, os_strlen(line));
		}
		if (!setAuth) {
			os_snprintf(line, sizeof(line), "wpa=%d\n", authType);
			tmp->write(line, os_strlen(line));
		}
		if (!setEncr && (AUTH_OPEN != authType)) {
			switch (encrType) {
			case ENCR_TKIP:
				os_snprintf(line, sizeof(line), "wpa_pairwise=TKIP\n");
				break;
			case ENCR_CCMP:
				os_snprintf(line, sizeof(line), "wpa_pairwise=CCMP\n");
				break;
			default:
				line[0] = 0;
				break;
			}
			if (line[0])
				tmp->write(line, os_strlen(line));
		}
		if (!setKeyMgmt && (AUTH_OPEN != authType)) {
			os_snprintf(line, sizeof(line), "wpa_key_mgmt=WPA-PSK\n");
			tmp->write(line, os_strlen(line));
		}
		if (!setNetKey && (AUTH_OPEN != authType) && os_strlen(netKey)) {
			if (64 == os_strlen(netKey))
				os_snprintf(line, sizeof(line), "wpa_psk=%s\n", netKey);
			else
				os_snprintf(line, sizeof(line), "wpa_passphrase=%s\n", netKey);
			tmp->write(line, os_strlen(line));
		}
		if ((AUTH_OPEN == authType) && (ENCR_WEP == encrType)) {
			int index;
			if (!setWepKeyIndex) {
				os_snprintf(line, sizeof(line), "wep_default_key=%d\n", wepKeyIndex - 1);
				tmp->write(line, os_strlen(line));
			}
			for (index = 0; index < 4; index++) {
				if (!setWepKey[index] && os_strlen(wepKey[index])) {
					if ((5 == os_strlen(wepKey[index])) || (13 == os_strlen(wepKey[index])))
						os_snprintf(line, sizeof(line), "wep_key%d=\"%s\"\n", index, wepKey[index]);
					else
						os_snprintf(line, sizeof(line), "wep_key%d=%s\n", index, wepKey[index]);
					tmp->write(line, os_strlen(line));
				}
			}
		}
		if (!setHwMode) {
			if ((channel > 14) && (channel <= 64))
				os_snprintf(line, sizeof(line), "hw_mode=a\n");
			else
				os_snprintf(line, sizeof(line), "hw_mode=g\n");
		}
		if (!setChan) {
			if ((channel <= 0) || (channel > 64))
				channel = 1;
			os_snprintf(line, sizeof(line), "channel=%d\n", channel);
		}

		ret = true;
	} while (0);

	if (conf) {
		if (conf->isOpen())
			conf->close();
		if (ret)
			conf->remove();
		delete conf;
	}

	if (tmp) {
		if (tmp->isOpen())
			tmp->close();
		tmp->setPermissions((QFile::Permissions)0x6644);
		if (ret) {
			if (!tmp->copy(HOSTAPD_CONF))
				ret = false;
		}
		delete tmp;
		if (ret && !setWpsState) {
			ret = false;
			conf = new QFile(HOSTAPD_CONF);
			tmp = new QTemporaryFile();

			do {
				if (!conf || !conf->exists())
					break;

				if (!conf->open(QIODevice::ReadOnly))
					break;

				if (!tmp->open())
					break;

				while (!conf->atEnd()) {
					if(-1 != (end = conf->readLine(line, sizeof(line)))) {
						do {
							line[end] = 0;
							os_snprintf(buf, sizeof(buf), "%s", line);
							pos = buf;

							while (*pos == ' ' || *pos == '\t' || *pos == '\r')
								pos++;
							if (*pos == '#' || *pos == '\n' || *pos == '\0' || *pos == '\r')
								break;

							var = pos;
							pos = os_strchr(var, '=');
							if (pos) {
								*pos++ = 0;
								value = pos;
								while (*pos != 0) {
									if ((*pos == '\n') || (*pos == '\r')) {
										*pos = 0;
										break;
									}
									pos++;
								}
							} else
								break;

							if (var && (0 == os_strcmp(var, "wps_property"))) {
								tmp->write(line, os_strlen(line));
								if (!wpsState) {
									os_snprintf(line, sizeof(line), "wps_state=%d\n", wpsState);
									tmp->write(line, os_strlen(line));
								}
								line[0] = 0;
							}
						} while (0);

						if (line[0])
							tmp->write(line, os_strlen(line));
					}
				}
				ret = true;
			} while (0);

			if (conf) {
				if (conf->isOpen())
					conf->close();
				if (ret)
					conf->remove();
				delete conf;
			}

			if (tmp) {
				if (tmp->isOpen())
					tmp->close();
				tmp->setPermissions((QFile::Permissions)0x6644);
				if (ret){
					if (!tmp->copy(HOSTAPD_CONF))
						ret = false;
				}
				delete tmp;
			}
		}
	}
	return ret;
}

bool MainProcess::setDebugOut(QObject *receiver, const char *method)
{
	bool ret = false;

	do {
		if (!mainProcess)
			break;

		mainProcess->setReadChannel(QProcess::StandardOutput);
		mainProcess->setProcessChannelMode(QProcess::MergedChannels);
		QObject::connect(mainProcess, SIGNAL(readyReadStandardOutput()),
						 receiver, method);

		ret = true;
	} while (0);

	return ret;
}

QString MainProcess::readDebugMsg()
{
	QString out = "";
	do {
		if (!mainProcess)
			break;
		out = mainProcess->readLine();
	} while (0);

	return out;
}

bool MainProcess::start()
{
	bool ret = false;
	char cmd[BUFSIZ];

	do {
		terminate();
		if (!mainProcess)
			break;

		os_snprintf(cmd, sizeof(cmd), "%s %s -ddd", HOSTAPD, HOSTAPD_CONF);
		mainProcess->start(cmd);
		if (!mainProcess->waitForStarted(WAIT_FOR_PROCESS))
			break;

		iface = os_strdup(wirelessInterface);

		srand(time(0));

		ret = true;
	} while (0);

	return ret;
}

void MainProcess::terminate()
{
	QProcess *rem = new QProcess();
	char remCmd[BUFSIZ];

	if (mainProcess &&
		(QProcess::NotRunning != mainProcess->state())) {

		disconnectMonitor(0, 0);

		if (ctrl) {
			wpa_ctrl_close(ctrl);
			ctrl = 0;
		}

		mainProcess->terminate();
		if (!mainProcess->waitForFinished(WAIT_FOR_PROCESS)) {
			mainProcess->kill();
		}

		os_snprintf(remCmd, sizeof(remCmd), "rem -rf %s",
					HOSTAPD_CTRL_DIR);
		rem->start(remCmd);
		rem->waitForFinished(-1);
		delete rem;
		rem = 0;
	}

	if (iface) {
		os_free(iface);
		iface = 0;
	}
}

#if defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
bool MainProcess::connectMonitor(QObject *receiver, const char *method)
{ bool ret = false;
	char monitor_iface[BUFSIZ];

	do {
		if (!receiver || !method)
			break;

		if (msgNotifier) {
			delete msgNotifier;
			msgNotifier = 0;
		}

		if (!monitor) {
			os_snprintf(monitor_iface, sizeof(monitor_iface), "%s/%s",
						HOSTAPD_CTRL_DIR, iface);
			monitor = wpa_ctrl_open(monitor_iface);
			if (!monitor)
				break;
			if (wpa_ctrl_attach(monitor))
				break;
		}

		msgNotifier = new QSocketNotifier(wpa_ctrl_get_fd(monitor),
										  QSocketNotifier::Read, 0);
		if (!msgNotifier)
			break;

		ret = QObject::connect(msgNotifier, SIGNAL(activated(int)), receiver, method);
	} while (0);

	if (!ret) {
		printf("Could not connect with monitor.\n");
		disconnectMonitor(receiver, method);
	}

	return ret;
}

bool MainProcess::disconnectMonitor(QObject *receiver, const char *method)
{
	if (msgNotifier && receiver && method)
		QObject::disconnect(msgNotifier, SIGNAL(activated(int)), receiver, method);

	if (msgNotifier) {
		delete msgNotifier;
		msgNotifier = 0;
	}

	if (monitor) {
		wpa_ctrl_detach(monitor);
		wpa_ctrl_close(monitor);
		monitor = 0;
	}

	return true;
}
#endif // defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)

bool MainProcess::ctrlPending()
{
	return (monitor && (0 < wpa_ctrl_pending(monitor)));
}

bool MainProcess::receive(char *msg, size_t *len)
{
	int ret = false;
	do {
		if (!monitor || !msg || !len)
			break;

		ret = (0 == wpa_ctrl_recv(monitor, msg, len));
		if (ret)
			msg[*len] = 0;
	} while (0);

	return ret;
}

bool MainProcess::getCtrlRequelst(char *buf, size_t len, int *priority, char *req, char *msg)
{
	bool ret = false;
	char *pos = buf, *pos2;
	size_t req_len;

	do {
		if (!buf || !len)
			break;

		if (req) *req = 0;
		if (msg) *msg = 0;

		if (*pos == '<') {
			pos++;
			if (priority)
				*priority = atoi(pos);
			pos = os_strchr(pos, '>');
			if (pos)
				pos++;
			else
				pos = buf;
		}

		if (os_strncmp(pos, "CTRL-", 5) == 0) {
			pos2 = os_strchr(pos, !os_strncmp(pos, WPA_CTRL_REQ, os_strlen(WPA_CTRL_REQ))?':':' ');
			if (pos2) {
				pos2++;
				if (req) {
					req_len = pos2 - (pos + os_strlen(WPA_CTRL_REQ) + 1);
					os_strncpy(req, pos + os_strlen(WPA_CTRL_REQ), req_len);
					req[req_len] = 0;
				}
				if (msg) {
					os_strncpy(msg, pos2, os_strlen(pos2));
					msg[os_strlen(pos2)] = 0;
				}
			} else {
				pos2 = pos;
				if (req)
					*req = 0;
				if (msg) {
					os_strncpy(msg, pos2 + os_strlen(WPA_CTRL_REQ), os_strlen(pos2 + os_strlen(WPA_CTRL_REQ)));
					msg[os_strlen(pos2 + os_strlen(WPA_CTRL_REQ))] = 0;
				}
			}
		} else {
			os_strncpy(msg, pos, os_strlen(pos));
			msg[os_strlen(pos)] = 0;
		}

		ret = true;
	} while (0);

	return ret;
}

int MainProcess::ctrlRequest(char *cmd, char *res, size_t *len)
{
	int ret = 0;
	char ctrl_iface[BUFSIZ];

	do {
		os_snprintf(ctrl_iface, sizeof(ctrl_iface), "%s/%s",
					HOSTAPD_CTRL_DIR, iface);

		if (!ctrl) {
			ctrl = wpa_ctrl_open(ctrl_iface);
			if (!ctrl) {
				ret = -3;
				break;
			}
		}

		if (mtx)
			mtx->lock();

		#if 1	/* added by Atheros */
		fprintf(stderr, "SENDING COMMAND: %s\n", cmd);
		#endif
		ret = wpa_ctrl_request(ctrl, cmd, os_strlen(cmd), res, len, 0);
		if (0 > ret) {
			wpa_ctrl_close(ctrl);
			ctrl = 0;
		}

		if (mtx)
			mtx->unlock();
	} while (0);

	if (0 > ret)
		printf("Fail control-request : %d.\n", ret);

	return ret;
}

void MainProcess::closeCtrl()
{
	if (ctrl) {
		wpa_ctrl_close(ctrl);
		ctrl = 0;
	}
}

bool MainProcess::reload()
{
	bool ret = false;
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (0 > ctrlRequest("RECONFIGURE", res, &len))
			break;

		if (os_strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to reload configuration.\n");

	return ret;
}

bool MainProcess::getStatus(char *result, size_t *len)
{
	bool ret = false;

	do {
		if (!result && !len)
			break;

		if (0 > ctrlRequest("STATUS", result, len))
			break;
		result[*len] = 0;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to get status.\n");

	return ret;
}

bool MainProcess::setRegMode(int regmode)
{
	bool ret = false;
	char cmd[BUFSIZ];
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		os_snprintf(cmd, sizeof(cmd), "WPS_SET_REGMODE %d", regmode);
		if (0 > ctrlRequest(cmd, res, &len))
			break;

		if (os_strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to scan NFC token request.\n");

	return ret;
}

bool MainProcess::setWpsPassword(const char *pwd)
{
	bool ret = false;
	char cmd[BUFSIZ];
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		os_snprintf(cmd, sizeof(cmd), "WPS_SET_PASSWORD %s", pwd?pwd:"");
		if (0 > ctrlRequest(cmd, res, &len))
			break;

		if (os_strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to set WPS password.\n");

	return ret;
}

bool MainProcess::clearWpsPassword()
{
	bool ret = false;
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (0 > ctrlRequest("WPS_CLEAR_PASSWORD", res, &len))
			break;

		if (os_strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to clear WPS password.\n");

	return ret;
}

bool MainProcess::ctrlWpsState(uchar state)
{
	bool ret = false;
	char cmd[BUFSIZ];
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		os_snprintf(cmd, sizeof(cmd), "SET_WPS_STATE %d", state);
		if (0 > ctrlRequest(cmd, res, &len))
			break;

		if (os_strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to set WPS state.\n");

	return ret;
}

bool MainProcess::writeNfcConfig()
{
	bool ret = false;
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (0 > ctrlRequest("WRITE_CONFIG_TOKEN", res, &len))
			break;

		if (os_strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to scan NFC token request.\n");

	return ret;
}

bool MainProcess::readNfcConfig()
{
	bool ret = false;
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (0 > ctrlRequest("READ_CONFIG_TOKEN", res, &len))
			break;

		if (os_strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to scan NFC token request.\n");

	return ret;
}

bool MainProcess::writeNfcPassword()
{
	bool ret = false;
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (0 > ctrlRequest("WRITE_PASSWORD_TOKEN", res, &len))
			break;

		if (os_strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to scan NFC token request.\n");

	return ret;
}

bool MainProcess::readNfcPassword()
{
	bool ret = false;
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (0 > ctrlRequest("READ_PASSWORD_TOKEN", res, &len))
			break;

		if (os_strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to scan NFC token request.\n");

	return ret;
}

bool MainProcess::cancelScanNfcToken()
{
	bool ret = false;
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (0 > ctrlRequest("CANCEL_NFC_COMMAND", res, &len))
			break;

		if (os_strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to cancel NFC command.\n");

	return ret;
}

bool MainProcess::startPbc()
{
	bool ret = false;
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		len = sizeof(res) - 1;
		if (0 > ctrlRequest("WPS_PBC_ENABLED 1", res, &len))
			break;

		if (os_strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to start PBC method.\n");

	return ret;
}

bool MainProcess::stopPbc()
{
	bool ret = false;
	char res[BUFSIZ];
	size_t len = sizeof(res) - 1;

	do {
		if (0 > ctrlRequest("WPS_PBC_ENABLED 0", res, &len))
			break;

		if (os_strncmp(res, "OK", 2))
			break;

		ret = true;
	} while (0);

	if (!ret)
		printf("Fail to stop PBC method.\n");

	return ret;
}

void MainProcess::generatePIN(char pwd[9])
{
	unsigned long pin;
	unsigned char checksum;
	unsigned long acc = 0;
	unsigned long tmp;

	if (!pwd) {
		printf("Could not generate PIN with NULL-pointer.\n");
		return;
	}

	pin = rand() % 10000000;
	tmp = pin * 10;

	acc += 3 * ((tmp / 10000000) % 10);
	acc += 1 * ((tmp / 1000000) % 10);
	acc += 3 * ((tmp / 100000) % 10);
	acc += 1 * ((tmp / 10000) % 10);
	acc += 3 * ((tmp / 1000) % 10);
	acc += 1 * ((tmp / 100) % 10);
	acc += 3 * ((tmp / 10) % 10);

	checksum = (unsigned char)(10 - (acc % 10)) % 10;
	os_snprintf(pwd, 9, "%08lu", pin * 10 + checksum);
}

bool MainProcess::validatePIN(const char pwd[9])
{
	bool ret = true;
	unsigned long pin, check;
	unsigned char checksum;
	unsigned long acc = 0;
	char *tmp = 0;

	do {
		pin = strtol(pwd, &tmp, 10);
		if (!tmp || *tmp)
			break;
		check = (pin / 10) * 10;
		acc += 3 * ((check / 10000000) % 10);
		acc += 1 * ((check / 1000000) % 10);
		acc += 3 * ((check / 100000) % 10);
		acc += 1 * ((check / 10000) % 10);
		acc += 3 * ((check / 1000) % 10);
		acc += 1 * ((check / 100) % 10);
		acc += 3 * ((check / 10) % 10);
		checksum = (unsigned char)(10 - (acc % 10)) % 10;

		if (checksum != (unsigned char)atoi(&pwd[7]))
			ret = false;
	} while (0);

	return ret;
}

void MainProcess::setWirelessInterface(const char *ifname) {
	if (wirelessInterface) {
		os_free(wirelessInterface);
		wirelessInterface = 0;
	}
	if (ifname)
		wirelessInterface = os_strdup(ifname);
}

char *MainProcess::getWirelessInterface()
{
	return wirelessInterface;
}

void MainProcess::setWirelessDriver(const char *driver) {
	if (wirelessDriver) {
		os_free(wirelessDriver);
		wirelessDriver = 0;
	}
	if (driver)
		wirelessDriver = os_strdup(driver);
}

char *MainProcess::setWirelessDriver()
{
	return wirelessDriver;
};

void MainProcess::setWiredInterface(const char *ifname)
{
	if (wiredInterface) {
		os_free(wiredInterface);
		wiredInterface = 0;
	}
	if (ifname)
		wiredInterface = os_strdup(ifname);
}

char *MainProcess::getWiredInterface()
{
	return wiredInterface;
}

void MainProcess::setBridgeInterface(const char *ifname)
{
	if (bridgeInterface) {
		os_free(bridgeInterface);
		bridgeInterface = 0;
	}
	if (ifname)
		bridgeInterface = os_strdup(ifname);
}

char *MainProcess::getBridgeInterface()
{
	return bridgeInterface;
}

void MainProcess::setIpAddress(const char *ipAddr)
{
	if (ipAddress) {
		os_free(ipAddress);
		ipAddress = 0;
	}
	if (ipAddr)
		ipAddress = os_strdup(ipAddr);
}

char *MainProcess::getIpAddress()
{
	return ipAddress;
}

void MainProcess::setNetMask(const char *_netMask)
{
	if (netMask) {
		os_free(netMask);
		netMask = 0;
	}
	if (_netMask)
		netMask = os_strdup(_netMask);
}

char *MainProcess::getNetMask()
{
	return netMask;
}

void MainProcess::setNfcInterface(const char *ifname)
{
	if (nfcInterface) {
		os_free(nfcInterface);
		nfcInterface = 0;
	}
	if (ifname)
		nfcInterface = os_strdup(ifname);
}

char *MainProcess::getNfcInterface()
{
	return nfcInterface;
}

void MainProcess::setSsid(const char *_ssid)
{
	os_memset(ssid, 0, sizeof(ssid));
	if (_ssid)
		os_snprintf(ssid, sizeof(ssid), "%s", _ssid);
}

const char *MainProcess::getSsid()
{
	return ssid;
}
void MainProcess::setAuthType(ushort auth)
{
	authType = auth;
}

ushort MainProcess::getAuthType()
{
	return authType;
}

void MainProcess::setEncrType(ushort encr)
{
	encrType = encr;
}

ushort MainProcess::getEncrType()
{
	return encrType;
}

void MainProcess::setNetKey(const char *key)
{
	os_memset(netKey, 0, sizeof(netKey));
	if (key)
		os_snprintf(netKey, sizeof(netKey), "%s", key);
}

const char *MainProcess::getNetKey()
{
	return netKey;
}

void MainProcess::setWepKeyIndex(ushort index)
{
	if ((0 < index) && (4 >= index))
		wepKeyIndex = index;
}

ushort MainProcess::getWepKeyIndex()
{
	return wepKeyIndex;
}

void MainProcess::setWepKey(ushort index, const char *key)
{
	if ((0 < index) && (4 >= index)) {
		os_memset(wepKey[index - 1], 0, sizeof(wepKey[0]));
		if (key)
			os_snprintf(&wepKey[index - 1][0], sizeof(wepKey[index - 1]), "%s", key);
	}
}

const char *MainProcess::getWepKey(ushort index)
{
	if (!index || (4 < index))
		return NULL;
	else
		return &wepKey[index -1][0];
}

void MainProcess::setWpsState(uchar state)
{
	wpsState = state;
}

uchar MainProcess::getWpsState()
{
	return wpsState;
}

const char *MainProcess::getChannelList(WIRELESS_MODE _mode)
{
	QProcess *iwlist = 0;
	char cmd[BUFSIZ];
	QString out = "";
	QStringList list;
	QStringList::Iterator it;
	bool first = true;
	int _channel;

	do {
		if (channelList) {
			os_free(channelList);
			channelList = 0;
		}

		iwlist = new QProcess;
		if (!iwlist)
			break;
		iwlist->setReadChannel(QProcess::StandardOutput);
		iwlist->setProcessChannelMode(QProcess::MergedChannels);

		os_snprintf(cmd, sizeof(cmd), "iwlist %s channel", wirelessInterface);
		iwlist->start(cmd);
		if (!iwlist->waitForFinished(WAIT_FOR_PROCESS)) {
			break;
		}

		if (0 != iwlist->exitCode())
			break;

		out = iwlist->readAll();
		if (!out.length())
			break;

		channelList = (char *)os_malloc(out.length());
		if (!channelList)
			break;
		channelList[0] = 0;

		list = out.split(QChar('\n'));
		for (it = list.begin(); it != list.end(); it++) {
			if (first) {
				first = false;
				continue;
			}

			QStringList cols = it->split(QRegExp("\\s+"));
			_channel = cols.count() > 2?cols[2].toInt():0;
			if (!_channel)
				continue;

			switch (_mode) {
			case WIRELESS_MODE_11AGB:
				if (_channel > 64)
					continue;
				break;
			case WIRELESS_MODE_11A:
				if ((_channel <= 14) || (_channel > 64))
					continue;
				break;
			case WIRELESS_MODE_11GB:
				if (_channel > 14)
					continue;
				break;
			}

			it->replace(QRegExp("^\\s+"), "");;
			if (os_strlen(channelList))
				strcat(channelList, "\n");
			strcat(channelList, (const char *)it->toAscii());
		}
	} while (0);

	if (iwlist) {
		delete iwlist;
		iwlist = 0;
	}

	return channelList;
}

bool MainProcess::setChannel(WIRELESS_MODE _mode, int _channel)
{
	bool ret = false;
	QProcess *iwconfig = 0;
	QProcess *iwpriv = 0;
	char *stringMode = 0;
	char cmd[BUFSIZ];

	do {
		os_snprintf(cmd, sizeof(cmd), "iwconfig %s channel 0", wirelessInterface);
		iwconfig = new QProcess();
		if (!iwconfig)
			break;
		iwconfig->start(cmd);
		if (!iwconfig->waitForFinished(WAIT_FOR_PROCESS))
			break;
		if (0 != iwconfig->exitCode())
			break;

		switch (_mode) {
		case WIRELESS_MODE_11AGB:
		case WIRELESS_MODE_11GB:
		case WIRELESS_MODE_11A:
			stringMode = "auto";
			break;
		default:
			break;
		}

		if (!stringMode)
			break;

		os_snprintf(cmd, sizeof(cmd), "iwpriv %s mode %s",
					wirelessInterface, stringMode);
		iwpriv = new QProcess();
		if (!iwpriv)
			break;
		iwpriv->start(cmd);
		if (!iwpriv->waitForFinished(WAIT_FOR_PROCESS))
			break;
		if (0 != iwpriv->exitCode())
			break;

		os_snprintf(cmd, sizeof(cmd), "iwconfig %s channel %d",
					wirelessInterface, _channel);
		iwconfig->start(cmd);
		if (!iwconfig->waitForFinished(WAIT_FOR_PROCESS))
			break;
		if (0 != iwconfig->exitCode())
			break;

		channel = _channel;

		ret = true;
	} while (0);

	if (iwconfig) {
		delete iwconfig;
		iwconfig = 0;
	}

	if (iwpriv) {
		delete iwpriv;
		iwpriv = 0;
	}

	return ret;
}

