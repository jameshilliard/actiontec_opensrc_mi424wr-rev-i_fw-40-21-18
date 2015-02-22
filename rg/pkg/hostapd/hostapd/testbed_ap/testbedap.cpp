/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: testbedap.cpp
//  Description: WiFi - Protected Setup Access Point graphical user interface source
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

#include "testbedap.h"
#include "mainprocess.h"
#include "pagetemplate.h"
#include "setupinterface.h"
#include "netconfig.h"
#include "wepkey.h"
#include "status.h"
#include "debugwindow.h"
#include "about.h"
#include "os.h"

#include <QMessageBox>
#include <QTimer>

#define HOSTAPD_CONF		"./hostapd.conf"

QProcess *MainProcess::mainProcess = new QProcess();
struct wpa_ctrl *MainProcess::monitor = 0;
struct wpa_ctrl *MainProcess::ctrl = 0;
char *MainProcess::iface = 0;
QMutex *MainProcess::mtx = new QMutex();
QSocketNotifier *MainProcess::msgNotifier = 0;
MainProcess::MODE MainProcess::mode = MODE_REG_REGSTA;
MainProcess::METHOD MainProcess::method = METHOD_NONE;

char *MainProcess::wirelessInterface = 0;
char *MainProcess::wirelessDriver = 0;
char *MainProcess::wiredInterface = 0;
char *MainProcess::bridgeInterface = 0;
char *MainProcess::nfcInterface = 0;

char *MainProcess::ipAddress = 0;
char *MainProcess::netMask = 0;

char MainProcess::ssid[32 + 1] = {0};
ushort MainProcess::authType = 0;
ushort MainProcess::encrType = 0;
char MainProcess::netKey[64 + 1] = {0};
ushort MainProcess::wepKeyIndex = 1;
char MainProcess::wepKey[4][26 + 1] = {{0}, {0}, {0}, {0}};
uchar MainProcess::wpsState = 0;

MainProcess::WIRELESS_MODE MainProcess::wirelessMode = MainProcess::WIRELESS_MODE_11AGB;
int MainProcess::channel = 0;
int MainProcess::channelIndex = 0;
char *MainProcess::channelList = 0;


TestbedAp::TestbedAp(QWidget *parent /* = 0 */, Qt::WindowFlags f /* = 0*/)
: QMainWindow(parent, f)
{
	setupUi(this);

	connect(actQuit, SIGNAL(activated()), SLOT(close()));
	connect(actAbout, SIGNAL(activated()), SLOT(about()));
	connect(pbBack, SIGNAL(clicked()), SLOT(back()));
	connect(pbNext, SIGNAL(clicked()), SLOT(next()));
	connect(pbCancel, SIGNAL(clicked()), SLOT(cancel()));

	setupInterface = new SetupInterface(this, frame);
	setupInterface->close();
	netConfig = new NetConfig(this, frame);
	netConfig->close();
	wepKey = new WepKey(this, frame);
	wepKey->close();
	status = new Status(this, frame);
	status->close();

	listPage.push_front(reinterpret_cast<PageTemplate *>(setupInterface));
	listPage.front()->pre_next();
	listPage.front()->show();

	connect(tabWidget, SIGNAL(currentChanged(int)), SLOT(changeMode(int)));
	tabWidget->setEnabled(false);
	changeMode(0);

	connect(pbWriteConfig, SIGNAL(clicked()), SLOT(writeConfigToken()));
	connect(pbRegStaPbc, SIGNAL(clicked()), SLOT(startRegStaPbc()));
	connect(pbRegStaAuth, SIGNAL(clicked()), SLOT(authRegSta()));
	connect(pbReadConfig, SIGNAL(clicked()), SLOT(readConfigToken()));
	connect(pbGetConfigPbc, SIGNAL(clicked()), SLOT(startGetConfigPbc()));
	connect(pbGetConfigAuth, SIGNAL(clicked()), SLOT(authGetConfig()));

	timer = new QTimer;
	connect(timer, SIGNAL(timeout()), SLOT(increment()));

	debugWindow = 0;
	if (MainProcess::setDebugOut(this, SLOT(debugging()))) {
		if (MainProcess::readConfigFile()) {
			debugWindow = new DebugWindow();
			QHBoxLayout *hl = new QHBoxLayout(debugWindow);
			debugWindow->textEdit = new QTextEdit(debugWindow);
			debugWindow->textEdit->setReadOnly(true);
			hl->addWidget(debugWindow->textEdit);
			debugWindow->setLayout(hl);
			debugWindow->setGeometry(x(), y() + height() + 80,
									 debugWindow->width(),
									 debugWindow->height());
			debugWindow->show();
		}
	}
}

TestbedAp::~TestbedAp()
{
	disconnect(actQuit);
	disconnect(pbBack);
	disconnect(pbNext);
	disconnect(pbCancel);

	delete setupInterface;
	delete netConfig;
	delete status;

	delete timer;
}

void TestbedAp::back()
{
	PageTemplate *p;

	do {
		if(!listPage.front()->post_back())
			break;
		p = listPage.front();
		p->close();
		listPage.pop_front();

		if (!listPage.front()->pre_back()) {
			if (!p->pre_next()) {
				QMessageBox::critical(this, windowTitle(), "Critical Error");
				break;
			}
			listPage.push_front(p);
			break;
		}
		listPage.front()->show();
	} while (0);
}

void TestbedAp::next()
{
	PageTemplate *n = 0;

	do {
		if(!listPage.front()->post_next())
			break;
		listPage.front()->close();

		if (setupInterface == listPage.front())
			n = reinterpret_cast<PageTemplate *>(netConfig);
		else if (netConfig == listPage.front()) {
			if ((MainProcess::AUTH_OPEN == MainProcess::getAuthType()) &&
				(MainProcess::ENCR_WEP == MainProcess::getEncrType()))
				n = reinterpret_cast<PageTemplate *>(wepKey);
			else
				n = reinterpret_cast<PageTemplate *>(status);
		} else if (wepKey == listPage.front()) {
			n = reinterpret_cast<PageTemplate *>(status);
		}

		if (n) {
			if (!n->pre_next())
				break;
			listPage.push_front(n);
		} else if (!listPage.front()->pre_next()) {
			QMessageBox::critical(this, windowTitle(), "Critical Error");
			break;
		}
		listPage.front()->show();
	} while (0);
}

void TestbedAp::cancel()
{
	listPage.front()->cancel();
}

void TestbedAp::close()
{
	MainProcess::terminate();

	if (debugWindow) {
		debugWindow->close();
		delete debugWindow;
		debugWindow = 0;
	}

	QMainWindow::close();
}

void TestbedAp::closeEvent(QCloseEvent *)
{
	close();
}

void TestbedAp::about()
{
	About license;
	license.exec();
}

void TestbedAp::debugging()
{
	QString out;
	do {
		while (1) {
			out = MainProcess::readDebugMsg();
			if (!out.length())
				break;
			if (debugWindow) {
				out.remove(QChar('\n'));
				debugWindow->textEdit->append(out);
			}
		}
	} while (0);
}

void TestbedAp::changeMode(int mode)
{
	switch (mode) {
	case 0: // Register Station
		lblRegStaCmt->setText("Register Station Mode\n");
		lePin->setText("");
		prgRegSta->setVisible(false);
		MainProcess::setMode(MainProcess::MODE_REG_REGSTA);
		(void)MainProcess::setRegMode(2);
		break;
	case 1: // Get configuration
		lblGetConfigCmt->setText("Get configuration Mode\n");
		lblPin->setText("");
		prgGetConfig->setVisible(false);
		MainProcess::setMode(MainProcess::MODE_ENR_GETCONF);
		(void)MainProcess::setRegMode(0);
		break;
	}
}

void TestbedAp::writeConfigToken()
{
	bool ret = false;

	do {
		MainProcess::setMethod(MainProcess::METHOD_NFC);

		if (!startAuthentication()) {
			QMessageBox::warning(this, label->text(),
								 "Could not start WPS authentication\n");
			break;
		}

		if (!MainProcess::setRegMode(2)) {
			QMessageBox::warning(this, label->text(),
								 "Could not set WPS registrar mode\n");
			break;
		}

		if (!MainProcess::writeNfcConfig()) {
			QMessageBox::warning(this, label->text(),
								 "Could not write configuration on NFC token\n");
			break;
		}

		ret = true;
	} while (0);

	if (!ret) {
		(void)stopAuthentication();
	}
}

void TestbedAp::startRegStaPbc()
{
	bool ret = false;

	do {
		MainProcess::setMethod(MainProcess::METHOD_PBC);

		if (!pbRegStaAuth->text().compare("Start &Authentication")) {
			if (!startAuthentication()) {
				QMessageBox::warning(this, label->text(),
									 "Could not start WPS authentication\n");
				break;
			}

			if (!MainProcess::setRegMode(2)) {
				QMessageBox::warning(this, label->text(),
									 "Could not set WPS registrar mode\n");
				break;
			}
		} else
			prgRegSta->setValue(0);

		if (!MainProcess::startPbc()) {
			QMessageBox::warning(this, label->text(),
								 "Could not start WPS authentication with PBC method\n");
			break;
		}

		ret = true;
	} while (0);

	if (!ret) {
		(void)stopAuthentication();
	}
}

void TestbedAp::authRegSta()
{
	bool ret = false;

	do {
		if (!pbRegStaAuth->text().compare("Start &Authentication")) {
			do {
				MainProcess::setMethod(MainProcess::METHOD_PIN);

				if (!MainProcess::setRegMode(2)) {
					QMessageBox::warning(this, label->text(),
										 "Could not set WPS registrar mode\n");
					break;
				}

				if (lePin->text().length()) {
					if ((8 == lePin->text().length()) &&
						!MainProcess::validatePIN(lePin->text().toAscii())) {
						if (QMessageBox::No ==
							QMessageBox::question(this, label->text(),
									 "PIN has invalidate checksum.\n"
									 "Do you really use this PIN?\n",
									 QMessageBox::Yes|QMessageBox::No)) {
							lePin->setFocus();
							break;
						}
					}

					if (!MainProcess::setWpsPassword(lePin->text().toAscii())) {
						QMessageBox::warning(this, label->text(),
											 "Could not set Password\n");
						break;
					}
				} else {
					if (!MainProcess::readNfcPassword()) {
						QMessageBox::warning(this, label->text(),
											 "Could not read Password on NFC token\n");
						break;
					}
				}

				if (!startAuthentication()) {
					QMessageBox::warning(this, label->text(),
										 "Could not start WPS authentication\n");
					break;
				}

				ret = true;
			} while (0);

			if (!ret)
				stopAuthentication();
		} else {
			(void)MainProcess::clearWpsPassword();
			switch (MainProcess::getMethod()) {
			case MainProcess::METHOD_NFC:
			case MainProcess::METHOD_PIN:
				(void)MainProcess::cancelScanNfcToken();
				break;
			case MainProcess::METHOD_PBC:
				(void)MainProcess::stopPbc();
				break;
			default:
				break;
			}

			stopAuthentication();
		}
	} while (0);
}

void TestbedAp::readConfigToken()
{
	bool ret = false;

	do {
		MainProcess::setMethod(MainProcess::METHOD_NFC);

		if (!startAuthentication()) {
			QMessageBox::warning(this, label->text(),
								 "Could not start WPS authentication\n");
			break;
		}

		if (!MainProcess::setRegMode(0)) {
			QMessageBox::warning(this, label->text(),
								 "Could not set WPS registrar mode\n");
			break;
		}

		if (!MainProcess::readNfcConfig()) {
			QMessageBox::warning(this, label->text(),
								 "Could not read configuration on NFC token\n");
			break;
		}

		ret = true;
	} while (0);

	if (!ret) {
		(void)stopAuthentication();
	}
}

void TestbedAp::startGetConfigPbc()
{
	bool ret = false;

	do {
		MainProcess::setMethod(MainProcess::METHOD_PBC);

		if (!pbRegStaAuth->text().compare("Start &Authentication")) {
			if (!startAuthentication()) {
				QMessageBox::warning(this, label->text(),
									 "Could not start WPS authentication\n");
				break;
			}

			if (!MainProcess::setRegMode(0)) {
				QMessageBox::warning(this, label->text(),
									 "Could not set WPS registrar mode\n");
				break;
			}
		} else
			prgRegSta->setValue(0);

		if (!MainProcess::startPbc()) {
			QMessageBox::warning(this, label->text(),
								 "Could not start WPS authentication with PBC method\n");
			break;
		}

		ret = true;
	} while (0);

	if (!ret) {
		(void)stopAuthentication();
	}
}

void TestbedAp::authGetConfig()
{
	bool ret = false;
	char pin[9];
	do {
		if (!pbGetConfigAuth->text().compare("Start &Authentication")) {
			do {
				MainProcess::setMethod(MainProcess::METHOD_PIN);

				if (!MainProcess::setRegMode(0)) {
					QMessageBox::warning(this, label->text(),
										 "Could not set WPS registrar mode\n");
					break;
				}

				MainProcess::generatePIN(pin);
				lblPin->setText(pin);

				if (!MainProcess::writeNfcPassword()) {
                                        #if 0   /* Atheros */
					QMessageBox::warning(this, label->text(),
										 "Could not write Password on NFC token\n");
					break;
                                        #endif  /* Atheros */
				}

				if (!startAuthentication()) {
					QMessageBox::warning(this, label->text(),
										 "Could not start WPS authentication\n");
					break;
				}
				ret = true;
			} while (0);

			if (!ret)
				stopAuthentication();
		} else if (!pbGetConfigAuth->text().compare("use &Default PIN")) {
			do {
				(void)MainProcess::cancelScanNfcToken();

				if (!MainProcess::setWpsPassword(lblPin->text().toAscii())) {
					QMessageBox::warning(this, label->text(),
										 "Could not set PIN\n");
					break;
				}
				ret = true;
			} while (0);

			if (ret) {
				lblGetConfigCmt->setText("Get configuration Mode\n"
										 "Now authenticating with PIN\n");
				pbGetConfigAuth->setText("Stop &Authentication");
				prgGetConfig->setValue(0);
			} else
				stopAuthentication();
		} else {
			(void)MainProcess::clearWpsPassword();
			switch (MainProcess::getMethod()) {
			case MainProcess::METHOD_NFC:
			case MainProcess::METHOD_PIN:
				(void)MainProcess::cancelScanNfcToken();
				break;
			case MainProcess::METHOD_PBC:
				(void)MainProcess::stopPbc();
				break;
			default:
				break;
			}

			stopAuthentication();
		}
	} while (0);
}


void TestbedAp::processCtrlRequest(char *buf, size_t len)
{
	QProgressBar *progressBar = 0;
	QLabel *lblResult = 0;
	QPushButton *pbStop = 0, *pbPbc = 0;
	int priority;
	char req[BUFSIZ];
	char msg[BUFSIZ];
	char *filename;

	switch (MainProcess::getMode()) {
	case MainProcess::MODE_REG_REGSTA:
		progressBar = prgRegSta;
		lblResult = lblRegStaCmt;
		pbStop = pbRegStaAuth;
		pbPbc = pbRegStaPbc;
		break;
	case MainProcess::MODE_ENR_GETCONF:
		progressBar = prgGetConfig;
		lblResult = lblGetConfigCmt;
		pbStop = pbGetConfigAuth;
		pbPbc = pbGetConfigPbc;
		break;
	default:
		timer->stop();
		return;
	}

	if (MainProcess::getCtrlRequelst(buf, len, &priority, req, msg)) {
		if (!os_strcmp(req, CTRL_REQ_EAP_WPS_FAIL) ||
			(!os_strlen(req) && !os_strncmp(msg, CTRL_REQ_EAP_WPS_FAIL, os_strlen(CTRL_REQ_EAP_WPS_FAIL)))) {
			timer->stop();
			progressBar->setVisible(false);
			lblPin->setText("");

			(void)MainProcess::clearWpsPassword();
			switch (MainProcess::getMethod()) {
			case MainProcess::METHOD_NFC:
			case MainProcess::METHOD_PIN:
				(void)MainProcess::cancelScanNfcToken();
				break;
			case MainProcess::METHOD_PBC:
				(void)MainProcess::stopPbc();
				break;
			default:
				break;
			}

			lblResult->setText(lblResult->text() + "\nFail\n");
			pbStop->setEnabled(false);
			pbPbc->setEnabled(false);
			QTimer::singleShot(5000, this, SLOT(stopAuthentication()));
		} else if (((!os_strcmp(req, CTRL_REQ_EAP_WPS_COMP) ||
					(!os_strlen(req) && !os_strncmp(msg, CTRL_REQ_EAP_WPS_COMP, os_strlen(CTRL_REQ_EAP_WPS_COMP))))) ||
				   (((!os_strcmp(req, CTRL_REQ_UPNP_COMP) ||
				    (!os_strlen(req) && !os_strncmp(msg, CTRL_REQ_UPNP_COMP, os_strlen(CTRL_REQ_UPNP_COMP))))))) {
			timer->stop();
			progressBar->setVisible(false);

			lblPin->setText("");
			pbStop->setEnabled(false);
			pbPbc->setEnabled(false);

			filename = os_strstr(msg, "[");
			if (filename) {
				filename = msg + os_strlen("[");
				if (os_strchr(filename, ']'))
					*(os_strchr(filename, ']')) = 0;

				do {
					if (!MainProcess::readConfigFile(filename)) {
						lblResult->setText(lblResult->text() +
										   "Could not load new configuration\n");
						QTimer::singleShot(5000, this, SLOT(stopAuthentication()));
						break;
					}

                                        #if 0   /* original code */
					if (!MainProcess::writeConfigFile()) {
                                        #else
                                        if (rename(filename, HOSTAPD_CONF)) {
                                        #endif
						lblResult->setText(lblResult->text() +
										   "Could not save new configuration\n");
						QTimer::singleShot(5000, this, SLOT(stopAuthentication()));
						break;
					}

					pbStop->setEnabled(false);
					lblResult->setText("Get configuration Mode\n\nComplete\n");
					QTimer::singleShot(5000, this, SLOT(reset()));
				} while (0);
			} else {
				lblResult->setText(lblResult->text() + "\nComplete\n");
				QTimer::singleShot(5000, this, SLOT(stopAuthentication()));
			}
		} else if (!os_strcmp(req, CTRL_REQ_NFC_WRITE_TIMEOUT) ||
			(!os_strlen(req) && !os_strncmp(msg, CTRL_REQ_NFC_WRITE_TIMEOUT, os_strlen(CTRL_REQ_NFC_WRITE_TIMEOUT)))) {
			switch (MainProcess::getMode()) {
			case MainProcess::MODE_REG_REGSTA:
				MainProcess::writeNfcConfig();
				break;
			case MainProcess::MODE_ENR_GETCONF:
				MainProcess::writeNfcPassword();
				break;
			}
		} else if (!os_strcmp(req, CTRL_REQ_NFC_COMP_WRITE) ||
			(!os_strlen(req) && !os_strncmp(msg, CTRL_REQ_NFC_COMP_WRITE, os_strlen(CTRL_REQ_NFC_COMP_WRITE)))) {

			switch (MainProcess::getMode()) {
			case MainProcess::MODE_REG_REGSTA:
				timer->stop();
				progressBar->setVisible(false);
				pbStop->setEnabled(false);

				filename = os_strstr(msg, "[Config Token:");
				if (filename) {
					filename = msg + os_strlen("[Config Token:");
					if (os_strchr(filename, ']'))
						*(os_strchr(filename, ']')) = 0;

					do {
						if (!MainProcess::readConfigFile(filename)) {
							lblResult->setText(lblResult->text() +
											   "Could not load new configuration\n");
							QTimer::singleShot(5000, this, SLOT(stopAuthentication()));
							break;
						}

                                                #if 0   /* original code */
						if (!MainProcess::writeConfigFile()) {
                                                #else
                                                if (rename(filename, HOSTAPD_CONF)) {
                                                #endif
							lblResult->setText(lblResult->text() +
											   "Could not save new configuration\n");
							QTimer::singleShot(5000, this, SLOT(stopAuthentication()));
							break;
						}

						lblResult->setText("Register Station Mode\n\nComplete\nConfiguration generated\n");
						QTimer::singleShot(5000, this, SLOT(reset()));
					} while (0);
				} else {
					lblResult->setText(lblResult->text() + "\nComplete\n");
					QTimer::singleShot(5000, this, SLOT(stopAuthentication()));
				}
				break;
			case MainProcess::MODE_ENR_GETCONF:
				lblResult->setText("Get configuration Mode\n"
								   "Written PIN on NFC token\n"
								   "Now authenticating with PIN.\n");
				lblPin->setText("Written on token");
				pbGetConfigAuth->setText("Stop &Authentication");
				progressBar->setValue(0);
				break;
			}
		} else if (!os_strcmp(req, CTRL_REQ_NFC_READ_TIMEOUT) ||
			(!os_strlen(req) && !os_strncmp(msg, CTRL_REQ_NFC_READ_TIMEOUT, os_strlen(CTRL_REQ_NFC_READ_TIMEOUT)))) {
			switch (MainProcess::getMode()) {
			case MainProcess::MODE_REG_REGSTA:
				MainProcess::readNfcPassword();
				break;
			case MainProcess::MODE_ENR_GETCONF:
				MainProcess::readNfcConfig();
				break;
			}
		} else if (!os_strcmp(req, CTRL_REQ_NFC_FAIL_READ) ||
			(!os_strlen(req) && !os_strncmp(msg, CTRL_REQ_NFC_FAIL_READ, os_strlen(CTRL_REQ_NFC_FAIL_READ)))) {
			timer->stop();
			progressBar->setVisible(false);

			(void)MainProcess::cancelScanNfcToken();

			lblResult->setText(lblResult->text() + "\nFail\n"
							   "Probably the token is invalid format.");
			pbStop->setEnabled(false);
			QTimer::singleShot(5000, this, SLOT(stopAuthentication()));
		} else if (!os_strcmp(req, CTRL_REQ_NFC_COMP_READ) ||
			(!os_strlen(req) && !strncmp(msg, CTRL_REQ_NFC_COMP_READ, os_strlen(CTRL_REQ_NFC_COMP_READ)))) {

			switch (MainProcess::getMode()) {
			case MainProcess::MODE_REG_REGSTA:
				lblResult->setText("Register Station Mode\n"
								   "Read PIN on NFC token\n"
								   "Now authenticating with PIN.\n");
				progressBar->setValue(0);
				break;
			case MainProcess::MODE_ENR_GETCONF:
				timer->stop();
				progressBar->setVisible(false);
				filename = os_strstr(msg, "[Config Token:");
				if (filename) {
					filename = msg + os_strlen("[Config Token:");
					if (strchr(filename, ']'))
						*(strchr(filename, ']')) = 0;

					do {
						if (!MainProcess::readConfigFile(filename)) {
							lblResult->setText(lblResult->text() +
											   "Could not load new configuration\n");
							QTimer::singleShot(5000, this, SLOT(stopAuthentication()));
							break;
						}

                                                #if 0   /* original code */
						if (!MainProcess::writeConfigFile()) {
                                                #else
                                                if (rename(filename, HOSTAPD_CONF)) {
                                                #endif
							lblResult->setText(lblResult->text() +
											   "Could not save new configuration\n");
							QTimer::singleShot(5000, this, SLOT(stopAuthentication()));
							break;
						}

						pbStop->setEnabled(false);
						lblResult->setText("Get configuration Mode\n\nComplete\n");
						QTimer::singleShot(5000, this, SLOT(reset()));
					} while (0);
				} else {
					lblResult->setText(lblResult->text() +
									   "Could not get new configuration\n");
					QTimer::singleShot(5000, this, SLOT(stopAuthentication()));
				}
				break;
			}
		}
	}
}

void TestbedAp::receiveMsgs()
{
	char msg[BUFSIZ];
	size_t len;

	while (MainProcess::ctrlPending()) {
		len = sizeof(msg)  - 1;
		if (MainProcess::receive(msg, &len))
			processCtrlRequest(msg, len);
	}
}

bool TestbedAp::startAuthentication()
{
	bool ret = false;

	do {
		pbBack->setEnabled(false);

		if (!MainProcess::connectMonitor(this, SLOT(receiveMsgs()))) {
			break;
		}

		switch (MainProcess::getMode()) {
		case MainProcess::MODE_REG_REGSTA:
			tabWidget->setTabEnabled(1, false);
			pbWriteConfig->setEnabled(false);
			lePin->setEnabled(false);

			switch (MainProcess::getMethod()) {
			case MainProcess::METHOD_NFC:
				pbRegStaPbc->setEnabled(false);
				lblRegStaCmt->setText(lblRegStaCmt->text() +
									  "Touch NFC token to write \n"
									  "current configuration\n");
				break;
			case MainProcess::METHOD_PIN:
				pbRegStaPbc->setEnabled(false);
				if (lePin->text().length()) {
					lblRegStaCmt->setText(lblRegStaCmt->text() +
										  "Now authenticating with PIN\n");
				} else {
					lblRegStaCmt->setText(lblRegStaCmt->text() +
										  "Touch NFC token to read \n"
										  "PIN of target station\n");
				}
				break;
			case MainProcess::METHOD_PBC:
				pbRegStaPbc->setText("Restart using &PBC method");
				lblRegStaCmt->setText(lblRegStaCmt->text() +
									  "Now authenticating with PBC\n");
				break;
			default:
				break;
			}

			pbRegStaAuth->setText("Stop &Authentication");

			prgRegSta->setFormat("Processing");
			prgRegSta->setValue(0);
			prgRegSta->setVisible(true);
			timer->start(100);
			break;
		case MainProcess::MODE_ENR_GETCONF:
			tabWidget->setTabEnabled(0, false);
			pbReadConfig->setEnabled(false);

			switch (MainProcess::getMethod()) {
			case MainProcess::METHOD_NFC:
				pbGetConfigPbc->setEnabled(false);
				lblGetConfigCmt->setText(lblGetConfigCmt->text() +
										 "Touch NFC token to read \n"
										 "new configuration\n");
				pbGetConfigAuth->setText("Stop &Authentication");
				break;
			case MainProcess::METHOD_PIN:
				pbGetConfigPbc->setEnabled(false);
				lblGetConfigCmt->setText(lblGetConfigCmt->text() +
										 "Touch NFC token to write PIN,\n"
										 "or push [use Default PIN]\n"
										 "to use Default PIN\n");
				pbGetConfigAuth->setText("use &Default PIN");
				break;
			case MainProcess::METHOD_PBC:
				pbGetConfigPbc->setText("Restart using &PBC method");
				lblGetConfigCmt->setText(lblRegStaCmt->text() +
										 "Now authenticating with PBC\n");
				pbGetConfigAuth->setText("Stop &Authentication");
				break;
			default:
				break;
			}

			prgGetConfig->setFormat("Processing");
			prgGetConfig->setValue(0);
			prgGetConfig->setVisible(true);
			timer->start(100);
			break;
		}

		ret = true;
	} while (0);

	return ret;
}

bool TestbedAp::stopAuthentication()
{
	bool ret = false;

	do {
		pbBack->setEnabled(true);

		(void)MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));

		switch (MainProcess::getMode()) {
		case MainProcess::MODE_REG_REGSTA:
			tabWidget->setTabEnabled(1, true);
			pbWriteConfig->setEnabled(true);
			lePin->setEnabled(true);
			prgRegSta->setVisible(false);
			pbRegStaPbc->setText("using &PBC method");
			pbRegStaPbc->setEnabled(true);
			lePin->setText("");

			lblRegStaCmt->setText("Register Station Mode\n");
			pbRegStaAuth->setText("Start &Authentication");
			pbRegStaAuth->setEnabled(true);
			break;
		case MainProcess::MODE_ENR_GETCONF:
			tabWidget->setTabEnabled(0, true);
			pbReadConfig->setEnabled(true);
			prgGetConfig->setVisible(false);
			pbGetConfigPbc->setText("using &PBC method");
			pbGetConfigPbc->setEnabled(true);
			lblPin->setText("");

			lblGetConfigCmt->setText("Get configuration Mode\n");
			pbGetConfigAuth->setText("Start &Authentication");
			pbGetConfigAuth->setEnabled(true);
			break;
		}

		ret = true;
	} while (0);

	return ret;
}

void TestbedAp::increment()
{
	QProgressBar *progressBar = 0;
	QLabel *lblResult = 0;
	QPushButton *pbStop = 0, *pbPbc = 0;

	switch (MainProcess::getMode()) {
	case MainProcess::MODE_REG_REGSTA:
		progressBar = prgRegSta;
		lblResult = lblRegStaCmt;
		pbStop = pbRegStaAuth;
		pbPbc = pbRegStaPbc;
		break;
	case MainProcess::MODE_ENR_GETCONF:
		progressBar = prgGetConfig;
		lblResult = lblGetConfigCmt;
		pbStop = pbGetConfigAuth;
		pbPbc = pbGetConfigPbc;
		break;
	default:
		timer->stop();
		return;
	}

	if (progressBar->value() < progressBar->maximum())
		progressBar->setValue(progressBar->value() + 1);
	else {
		timer->stop();

		(void)MainProcess::clearWpsPassword();
		switch (MainProcess::getMethod()) {
		case MainProcess::METHOD_NFC:
		case MainProcess::METHOD_PIN:
			(void)MainProcess::cancelScanNfcToken();
			break;
		case MainProcess::METHOD_PBC:
			(void)MainProcess::stopPbc();
			break;
		default:
			break;
		}

		progressBar->setFormat("Timeout");
		progressBar->update();

		pbPbc->setEnabled(false);
		pbStop->setEnabled(false);
		lblResult->setText(lblResult->text() + "\nTimeout\n");
		QTimer::singleShot(5000, this, SLOT(stopAuthentication()));
	}
}

void TestbedAp::reset()
{
	bool ret = false;

	do {
		status->post_next();

		(void)MainProcess::disconnectMonitor(this, SLOT(receiveMsgs()));

		if (!MainProcess::reload()) {
			QMessageBox::critical(this, windowTitle(),
								  "Could not restart Access Point");
			break;
		}

		MainProcess::closeCtrl();

		lblRegStaCmt->setText("Restarting ...");
		lblGetConfigCmt->setText("Restarting ...");

		ret = true;
	} while (0);

	if (ret)
		QTimer::singleShot(5000, this, SLOT(restart()));
	else
		close();
}

void TestbedAp::restart()
{
	stopAuthentication();
	tabWidget->setCurrentIndex(0);
	status->pre_next();
}

