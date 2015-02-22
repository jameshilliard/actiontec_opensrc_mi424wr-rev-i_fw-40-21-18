/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: setupinterface.cpp
//  Description: setup interface source
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

#include "pagetemplate.h"
#include "setupinterface.h"
#include "mainprocess.h"
#include "testbedap.h"
#include "os.h"

#include <QMessageBox>
#include <QProcess>
#include <QFile>


SetupInterface::SetupInterface(QWidget *wizard, QWidget *parent /* = 0 */, Qt::WindowFlags f /* = 0 */)
: PageTemplate(parent, f)
{
#define CONFIG_FILE "./testbed_ap.conf"
#define DEFAULT_WIRELESS_INTERFACE "ath0"
#define DEFAULT_WIRELESS_DRIVER "madwifi"
#define DEFAULT_WIRED_INTERFACE "eth0"
#define DEFAULT_BRIDGE_INTERFACE "br0"
#define DEFAULT_IPADDRESS "192.168.0.1"
#define DEFAULT_NETMASK "255.255.255.0"
#define DEFAULT_NFC_INTERFACE "/dev/ttyUSB0"
	QFile *conf = new QFile(CONFIG_FILE);
	bool read_file = false;
	int index;

	setupUi(this);

	wiz = reinterpret_cast<TestbedAp *>(wizard);

	QRegExp rx("[0-9]\\d{0,2}\\.[0-9]\\d{0,2}\\.[0-9]\\d{0,2}\\.[0-9]\\d{0,2}");
	validator1 = new QRegExpValidator(rx, this);
	leIPAddress->setValidator(validator1);
	validator2 = new QRegExpValidator(rx, this);
	leNetMask->setValidator(validator2);

	if (conf && conf->exists() && conf->open(QIODevice::ReadOnly)) {
		char line[BUFSIZ], *tmp, *tmp2;

		MainProcess::setWirelessMode(MainProcess::WIRELESS_MODE_11AGB);
		MainProcess::setChannelIndex(0);

		index = 0;
		while((6 >= index) && !conf->atEnd()) {
			if (0 > conf->readLine(line, sizeof(line) - 1))
				break;

			if (0 != (tmp = os_strchr(line, '\n')))
				*tmp = 0;

			switch (index++) {
			case 0: leWInterface->setText(line); break;
			case 1: leWDriver->setText(line); break;
			case 2: leInterface2->setText(line); break;
			case 3: leInterface3->setText(line); break;
			case 4:
			{
				if (0 != (tmp = os_strchr(line, ','))) {
					*tmp = 0;
					tmp++;
					if (0 != (tmp2 = os_strchr(tmp, ','))) {
						*tmp2 = 0;
						tmp2++;
					}
					cbIPAddress->setChecked(atoi(line));
					leIPAddress->setText(tmp);
					if (tmp2)
						leNetMask->setText(tmp2);
					else
						leNetMask->setText(DEFAULT_NETMASK);
					leIPAddress->setEnabled(cbIPAddress->isChecked());
					leNetMask->setEnabled(cbIPAddress->isChecked());
				}
				break;
			}
			case 5:
			{
				if (0 != (tmp = os_strchr(line, ','))) {
					*tmp = 0;
					tmp++;
					cbNFCInterface->setChecked(atoi(line));
					leNFCInterface->setText(tmp);
				}
				break;
			}
			case 6:
			{
				if (0 != (tmp = os_strchr(line, ','))) {
					*tmp = 0;
					tmp++;
					MainProcess::setWirelessMode((MainProcess::WIRELESS_MODE)atoi(line));
					MainProcess::setChannelIndex(atoi(tmp));
				}
			}
			}
		}
		conf->close();

		if (5 < index)
			read_file = true;
	}

	if (conf)
		delete conf;

	if (!read_file) {
		leWInterface->setText(DEFAULT_WIRELESS_INTERFACE);
		leWDriver->setText(DEFAULT_WIRELESS_DRIVER);
		leInterface2->setText(DEFAULT_WIRED_INTERFACE);
		leInterface2->setText(DEFAULT_BRIDGE_INTERFACE);
		cbIPAddress->setChecked(true);
		leIPAddress->setText(DEFAULT_IPADDRESS);
		leNetMask->setText(DEFAULT_NETMASK);
		cbNFCInterface->setChecked(true);
		leNFCInterface->setText(DEFAULT_NFC_INTERFACE);
	}

	connect(cbIPAddress, SIGNAL(clicked()), SLOT(enabledDhcp()));

	cbIPAddress->setChecked(true);
	cbIPAddress->setVisible(false);

	QLabel *lblIPAddress = new QLabel("IP address");

	gridLayout1->addWidget(lblIPAddress, 4, 0, 1, 1);

	lblIPAddress->setText("IP address");

#undef CONFIG_FILE
#undef DEFAULT_WIRELESS_INTERFACE
#undef DEFAULT_WIRELESS_DRIVER
#undef DEFAULT_WIRED_INTERFACE
#undef DEFAULT_BRIDGE_INTERFACE
#undef DEFAULT_IPADDRESS
#undef DEFAULT_NETMASK
#undef DEFAULT_NFC_INTERFACE
}

SetupInterface::~SetupInterface()
{
	(void)end();

	disconnect(cbIPAddress);

	if (validator1)
		delete validator1;
	if (validator2)
		delete validator2;
}

bool SetupInterface::pre_back()
{
	MainProcess::terminate();
	wiz->pbBack->setEnabled(false);
	wiz->pbNext->setEnabled(true);
	wiz->pbCancel->setEnabled(true);
	return true;
}

bool SetupInterface::pre_next()
{
	wiz->pbBack->setEnabled(false);
	wiz->pbNext->setEnabled(true);
	wiz->pbCancel->setEnabled(true);
	return true;
}

bool SetupInterface::post_next()
{
#define AP_START	"./ap_start"
#define CONFIG_FILE "./testbed_ap.conf"
	bool ret = false;
	QProcess *prc = new QProcess(this);
	char cmd[BUFSIZ];
	QFile *conf = new QFile(CONFIG_FILE);
	char line[BUFSIZ];

	do {
		if (!checkInputs()) {
			QMessageBox::critical(this, label->text(), "Input error");
			break;
		}

		os_snprintf(cmd, sizeof(cmd), "%s %s %s %s %s %s",
					AP_START,
					(const char *)leInterface3->text().toAscii(),
					(const char *)leWInterface->text().toAscii(),
					(const char *)leInterface2->text().toAscii(),
					cbIPAddress->isChecked()?
						(const char *)leIPAddress->text().toAscii():"dynamic",
					cbIPAddress->isChecked()?
						(const char *)leNetMask->text().toAscii():"255.255.255.0");
		prc->start(cmd);
		prc->waitForFinished(-1);
		if(prc->exitCode()) {
			QMessageBox::critical(this, label->text(), "Set interface error");
			break;
		}

		if (conf && conf->open(QIODevice::WriteOnly)) {
			os_snprintf(line, sizeof(line), "%s\n",
						(const char *)leWInterface->text().toAscii());
			conf->write(line, os_strlen(line));
			os_snprintf(line, sizeof(line), "%s\n",
						(const char *)leWDriver->text().toAscii());
			conf->write(line, os_strlen(line));
			os_snprintf(line, sizeof(line), "%s\n",
						(const char *)leInterface2->text().toAscii());
			conf->write(line, os_strlen(line));
			os_snprintf(line, sizeof(line), "%s\n",
						(const char *)leInterface3->text().toAscii());
			conf->write(line, os_strlen(line));
			os_snprintf(line, sizeof(line), "%d,%s,%s\n",
						cbIPAddress->isChecked()?1:0,
						(const char *)leIPAddress->text().toAscii(),
						(const char *)leNetMask->text().toAscii());
			conf->write(line, os_strlen(line));
			os_snprintf(line, sizeof(line), "%d,%s\n",
						cbNFCInterface->isChecked()?1:0,
						(const char *)leNFCInterface->text().toAscii());
			conf->write(line, os_strlen(line));

			os_snprintf(line, sizeof(line), "%d,%d\n",
						MainProcess::getWirelessMode(),
						MainProcess::getChannelIndex());
			conf->write(line, os_strlen(line));
		}
		if (conf)
			delete conf;

		MainProcess::setWirelessInterface(leWInterface->text().toAscii());
		MainProcess::setWirelessDriver(leWDriver->text().toAscii());
		MainProcess::setWiredInterface(leInterface2->text().toAscii());
		MainProcess::setBridgeInterface(leInterface3->text().toAscii());

		MainProcess::setIpAddress(leIPAddress->text().toAscii());
		MainProcess::setNetMask(leNetMask->text().toAscii());

		MainProcess::setNfcInterface(leNFCInterface->text().toAscii());

		ret = true;
	} while (0);

	if (prc) {
		delete prc;
		prc = 0;
	}

	return ret;
#undef AP_START
#undef CONFIG_FILE
}

bool SetupInterface::checkInputs()
{
	bool ret = false;
	QString check1 = leIPAddress->text();
	QString check2 = leNetMask->text();
	int pos = 0;

	do {
		if (!leWInterface->text().length()) {
			leWInterface->setFocus();
			break;
		}

		if (!leInterface2->text().length()) {
			leInterface2->setFocus();
			break;
		}

		if (!leInterface3->text().length()) {
			leInterface3->setFocus();
			break;
		}

		if (cbIPAddress->isChecked() && 
			(QValidator::Acceptable != validator1->validate(check1, pos))) {
			leIPAddress->setFocus();
			break;
		}

		if (cbIPAddress->isChecked() && 
			(QValidator::Acceptable != validator2->validate(check2, pos))) {
			leNetMask->setFocus();
			break;
		}

		if (cbNFCInterface->isChecked() &&
			!leNFCInterface->text().length()) {
			leNFCInterface->setFocus();
			break;
		}

		ret = true;
	} while(0);
	return ret;
}

void SetupInterface::cancel()
{
	wiz->close();
}

void SetupInterface::enabledDhcp()
{
	leIPAddress->setEnabled(cbIPAddress->isChecked());
	leNetMask->setEnabled(cbIPAddress->isChecked());
}

const char *SetupInterface::getWirelessInterface()
{
	return (const char *)leWInterface->text().toAscii();
}

const char *SetupInterface::getWiredInterface()
{
	return (const char *)leInterface2->text().toAscii();
}

bool SetupInterface::end()
{
#define AP_END	"./ap_end"
	bool ret = false;
	QProcess *prc = new QProcess(this);
	char cmd[BUFSIZ];

	do {
		if (!checkInputs()) {
			break;
		}

		os_snprintf(cmd, sizeof(cmd), "%s %s %s %s",
					AP_END,
					(const char *)leInterface3->text().toAscii(),
					(const char *)leWInterface->text().toAscii(),
					(const char *)leInterface2->text().toAscii());
		prc->start(cmd);
		prc->waitForFinished(-1);

		ret = true;
	} while (0);

	if (prc) {
		delete prc;
		prc = 0;
	}

	return ret;
#undef AP_END
}

