/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: netconfig.cpp
//  Description: Network configuration source
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
#include "netconfig.h"
#include "mainprocess.h"
#include "testbedap.h"
#include "os.h"

#include <QMessageBox>
#include <QFile>


NetConfig::NetConfig(QWidget *wizard, QWidget *parent /* = 0 */, Qt::WindowFlags f /* = 0 */)
: PageTemplate(parent, f)
{
	wiz = reinterpret_cast<TestbedAp *>(wizard);
	setupUi(this);

	cmbAuth->addItem("Open");
	cmbAuth->addItem("WPA-PSK");
	cmbAuth->addItem("WPA2-PSK");

	connect(cmbAuth, SIGNAL(currentIndexChanged(int)), SLOT(selectAuth(int)));
	connect(pbGenSsid, SIGNAL(clicked()), SLOT(generateSsid()));
	connect(pbGenNetKey, SIGNAL(clicked()), SLOT(generateNetKey()));

	connect(cb11a, SIGNAL(clicked()), SLOT(changeWirelessMode()));
	connect(cb11gb, SIGNAL(clicked()), SLOT(changeWirelessMode()));

	cmbAuth->setCurrentIndex(1);

	cmbConfFlag->addItem("Not configured");
	cmbConfFlag->addItem("Configured");
	cmbConfFlag->setCurrentIndex(0);

	srand(time(0));
}

NetConfig::~NetConfig()
{
}

bool NetConfig::pre_back()
{
	wiz->pbBack->setEnabled(true);
	wiz->pbNext->setEnabled(true);
	wiz->pbCancel->setEnabled(true);

	wiz->pbNext->setFocus();
	return true;
}

bool NetConfig::pre_next()
{
	wiz->pbBack->setEnabled(true);
	wiz->pbNext->setEnabled(true);
	wiz->pbCancel->setEnabled(true);

	leSsid->setText(MainProcess::getSsid());
	switch (MainProcess::getAuthType()) {
	case MainProcess::AUTH_OPEN:
		cmbAuth->setCurrentIndex(0);
		break;
	case MainProcess::AUTH_WPA:
		cmbAuth->setCurrentIndex(1);
		break;
	case MainProcess::AUTH_WPA2:
		cmbAuth->setCurrentIndex(2);
		break;
	}

	if (MainProcess::AUTH_OPEN == MainProcess::getAuthType()) {
		switch (MainProcess::getEncrType()) {
		case MainProcess::ENCR_NONE:
			cmbEncr->setCurrentIndex(0);
			break;
		case MainProcess::ENCR_WEP:
			cmbEncr->setCurrentIndex(1);
			break;
		}
	} else {
		switch (MainProcess::getEncrType()) {
		case MainProcess::ENCR_TKIP:
			cmbEncr->setCurrentIndex(0);
			break;
		case MainProcess::ENCR_CCMP:
			cmbEncr->setCurrentIndex(1);
			break;
		}
	}

	leNetKey->setText(MainProcess::getNetKey());

	switch (MainProcess::getWpsState()) {
	case MainProcess::WPS_STATE_NOTCONFIGURED:
		cmbConfFlag->setCurrentIndex(0);
		break;
	case MainProcess::WPS_STATE_CONFIGURED:
		cmbConfFlag->setCurrentIndex(1);
		break;
	}

	switch (MainProcess::getWirelessMode()) {
	case MainProcess::WIRELESS_MODE_11AGB:
		cb11a->setChecked(true);
		cb11gb->setChecked(true);
		break;
	case MainProcess::WIRELESS_MODE_11A:
		cb11a->setChecked(true);
		cb11gb->setChecked(false);
		break;
	case MainProcess::WIRELESS_MODE_11GB:
		cb11a->setChecked(false);
		cb11gb->setChecked(true);
		break;
	}

	changeWirelessMode();
	cmbChannelList->setCurrentIndex(MainProcess::getChannelIndex());

	wiz->pbNext->setFocus();
	return true;
}

bool NetConfig::post_next()
{
	bool ret = false;

	do {
		if (!checkInputs()) {
			QMessageBox::critical(this, label->text(), "Input error");
			break;
		}

		MainProcess::setSsid(leSsid->text().toAscii());
		switch (cmbAuth->currentIndex()) {
		case 0:
			MainProcess::setAuthType(MainProcess::AUTH_OPEN);
			break;
		case 1:
			MainProcess::setAuthType(MainProcess::AUTH_WPA);
			break;
		case 2:
			MainProcess::setAuthType(MainProcess::AUTH_WPA2);
			break;
		default:
			QMessageBox::critical(this, label->text(), "Unknown Authentication Type");
			cmbAuth->setFocus();
			return false;
		}

		if (0 == cmbAuth->currentIndex()) {
			MainProcess::setEncrType(MainProcess::ENCR_NONE);
			switch (cmbEncr->currentIndex()) {
			case 0:
				MainProcess::setEncrType(MainProcess::ENCR_NONE);
				break;
			case 1:
				MainProcess::setEncrType(MainProcess::ENCR_WEP);
				break;
			default:
				QMessageBox::critical(this, label->text(), "Unknown Encryption Type");
				cmbEncr->setFocus();
				return false;
			}
			MainProcess::setNetKey(0);
		} else {
			switch (cmbEncr->currentIndex()) {
			case 0:
				MainProcess::setEncrType(MainProcess::ENCR_TKIP);
				break;
			case 1:
				MainProcess::setEncrType(MainProcess::ENCR_CCMP);
				break;
			default:
				QMessageBox::critical(this, label->text(), "Unknown Encryption Type");
				cmbEncr->setFocus();
				return false;
			}
			MainProcess::setNetKey(leNetKey->text().toAscii());
		}

		switch (cmbConfFlag->currentIndex()) {
		case 0:
			MainProcess::setWpsState(MainProcess::WPS_STATE_NOTCONFIGURED);
			break;
		case 1:
			MainProcess::setWpsState(MainProcess::WPS_STATE_CONFIGURED);
			break;
		default:
			QMessageBox::critical(this, label->text(), "Unknown Configuration Flag");
			cmbConfFlag->setFocus();
			return false;
		}

		if (!setChannel()) {
			QMessageBox::critical(this, label->text(), "Could not set channel");
			cmbChannelList->setFocus();
			return false;
		}

		if (MainProcess::ENCR_WEP != MainProcess::getEncrType()) {
			if (!MainProcess::writeConfigFile()) {
				QMessageBox::critical(this, label->text(), "Cannot write main process configuration file");
				break;
			}

			if (!MainProcess::start()) {
				QMessageBox::critical(this, label->text(), "Cannot start main process");
				break;
			}
		}

		ret = true;
	} while (0);

	return ret;
}

void NetConfig::cancel()
{
	MainProcess::terminate();
	wiz->close();
}

void NetConfig::selectAuth(int selected)
{
	cmbEncr->clear();
	switch (selected) {
	case 0:
		cmbEncr->addItem("None");
		cmbEncr->addItem("WEP");
		leNetKey->setEnabled(false);
		pbGenNetKey->setEnabled(false);
		break;
	case 1:
	case 2:
		cmbEncr->addItem("TKIP");
		cmbEncr->addItem("CCMP");
		leNetKey->setEnabled(true);
		cmbEncr->setCurrentIndex(selected - 1);
		pbGenNetKey->setEnabled(true);
		break;
	default:
		break;
	}
}

void NetConfig::generateSsid()
{
	char ssid[32 + 1];

	for (int i = 0; i < 32; i++) {
		ssid[i] = btoa(rand() % 16);
	}
	ssid[32] = 0;
	leSsid->clear();
	leSsid->setText(ssid);
}

void NetConfig::generateNetKey()
{
	char netKey[64 + 1];

	for (int i = 0; i < 64; i++) {
		netKey[i] = btoa(rand() % 16);
	}
	netKey[64] = 0;
	leNetKey->clear();
	leNetKey->setText(netKey);
}

char NetConfig::btoa(int b, bool capital /* = true */ )
{
	if ((0 <= b) && (9 >= b)) {
		return b + '0';
	} else if ((0xA <= b) && (0xF >= b)) {
		return (b - 0xA) + (capital?'A':'a');
	} else {
		return '0';
	}
}

bool NetConfig::checkInputs()
{
	bool ret = false;
	int i;

	do {
		if (!leSsid->text().length() || (32 < leSsid->text().length())) {
			leSsid->setFocus();
			break;
		}

		if (0 != cmbAuth->currentIndex()) {
			/* WPA-PSK / WPA2-PSK */
			if (64 == leNetKey->text().length()) {
				for (i = 0; i < 64; i++) {
					if (!isxdigit(*(const char *)(leNetKey->text().data() + i)))
						break;
				}
				if (64 != i) {
					leNetKey->setFocus();
					break;
				}
			} else if (8 > leNetKey->text().length()) {
				leNetKey->setFocus();
				break;
			}
		}

		if (!cb11a->isChecked() && !cb11gb->isChecked()) {
			cb11a->setFocus();
		}

		ret = true;
	} while (0);

	return ret;
}

void NetConfig::changeWirelessMode()
{
	MainProcess::WIRELESS_MODE mode;
	if (cb11a->isChecked() && cb11gb->isChecked()) {
		mode = MainProcess::WIRELESS_MODE_11AGB;
		cb11a->setEnabled(true);
		cb11gb->setEnabled(true);
	} else if (cb11a->isChecked()) {
		mode = MainProcess::WIRELESS_MODE_11A;
		cb11a->setEnabled(false);
		cb11gb->setEnabled(true);
	} else if (cb11gb->isChecked()) {
		mode = MainProcess::WIRELESS_MODE_11GB;
		cb11a->setEnabled(true);
		cb11gb->setEnabled(false);
	} else
		return;

	if (setChannelList(MainProcess::getChannelList(mode))) {
		cmbChannelList->setCurrentIndex(0);
	}
}

bool NetConfig::setChannelList(const char *list)
{
	bool ret = false;
	QStringList channelList;

	do {
		cmbChannelList->clear();

		if (!list) {
			QMessageBox::warning(this, label->text(), "Could not get channel list");
			break;
		}

		channelList = QString(list).split(QChar('\n'));
		cmbChannelList->addItems(channelList);

		ret = true;
	} while (0);

	return ret;
}

bool NetConfig::setChannel()
{
#define CONFIG_FILE "./testbed_ap.conf"
	bool ret = false;
	MainProcess::WIRELESS_MODE mode;
	QStringList cols;
	int channel;
	QFile *conf = new QFile(CONFIG_FILE);
	char line[BUFSIZ];
	int i;

	do {
		if (cb11a->isChecked() && cb11gb->isChecked())
			mode = MainProcess::WIRELESS_MODE_11AGB;
		else if (cb11a->isChecked())
			mode = MainProcess::WIRELESS_MODE_11A;
		else if (cb11gb->isChecked())
			mode = MainProcess::WIRELESS_MODE_11GB;
		else
			break;

		cols = cmbChannelList->currentText().split(QRegExp("\\s+"));
		channel = cols[1].toInt();
		if (!channel)
			break;

		if (!MainProcess::setChannel(mode, channel))
			break;

		if (!conf || !conf->open(QIODevice::ReadWrite))
			break;

		for (i = 0; i <= 5; i++)
			conf->readLine(line, sizeof(line));
		os_snprintf(line, sizeof(line), "%d,%d\n",
					mode, cmbChannelList->currentIndex());
		conf->write(line);
		conf->close();

		MainProcess::setWirelessMode(mode);
		MainProcess::setChannelIndex(cmbChannelList->currentIndex());

		ret = true;
	} while (0);

	if (conf) {
		if (conf->isOpen())
			conf->close();
		delete conf;
		conf = 0;
	}

	return ret;
#undef CONFIG_FILE
}

