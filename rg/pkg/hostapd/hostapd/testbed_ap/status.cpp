/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: status.cpp
//  Description: Display status source
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
#include "status.h"
#include "testbedap.h"
#include "mainprocess.h"
#include "os.h"

#include <QMessageBox>
#include <QTimer>


Status::Status(QWidget *wizard, QWidget *parent /* = 0 */, Qt::WindowFlags f /* = 0 */)
: PageTemplate(parent, f)
{
	wiz = reinterpret_cast<TestbedAp *>(wizard);
	setupUi(this);

	timer = new QTimer;
	connect(timer, SIGNAL(timeout()), SLOT(update()));
}

Status::~Status()
{
	delete timer;
}

bool Status::pre_next()
{
	bool ret = false;

	wiz->pbBack->setText("Set &Unconfigured State");
	wiz->pbBack->setEnabled(false);
	wiz->pbNext->setVisible(false);
	wiz->pbCancel->setText("Exit");
	wiz->pbCancel->setEnabled(true);

	wiz->tabWidget->setEnabled(false);

	do {
		lblBssid->setText("");
		lblSsid->setText("");
		lblAuth->setText("");
		lblEncr->setText("");
		leNetKey->setText("");
		lblConfState->setText("");
		lblIpAddress->setText("");

		QPalette p = leNetKey->palette();
		p.setColor(QPalette::Normal, QPalette::Base,
				   lblBssid->palette().color(QPalette::Normal, QPalette::Background));
		p.setColor(QPalette::Inactive, QPalette::Base,
				   lblBssid->palette().color(QPalette::Normal, QPalette::Background));
		leNetKey->setPalette(p);
		leNetKey->setAutoFillBackground(true);

		timer->start(5000);

		ret = true;
	} while (0);

	return ret;
}

void Status::cancel()
{
	timer->stop();
	wiz->close();
}

bool Status::post_back()
{
	do {
		if (!MainProcess::ctrlWpsState(MainProcess::WPS_STATE_NOTCONFIGURED)) {
			QMessageBox::warning(this, label->text(),
								 "Could not set configuration state\n");
			break;
		}

		MainProcess::setWpsState(MainProcess::WPS_STATE_NOTCONFIGURED);
	} while (0);

	return false;
}

bool Status::post_next()
{
	timer->stop();

	lblBssid->setText("");
	lblSsid->setText("");
	lblAuth->setText("");
	lblEncr->setText("");
	leNetKey->setText("");
	lblConfState->setText("");
	lblIpAddress->setText("");

	return true;
}

void Status::update()
{
	bool ret = false;
	char res[0x1000];
	size_t len = sizeof(res) - 1;
	char *start, *end, *pos;

	do {
		lblBssid->setText("");
		lblSsid->setText("");
		lblAuth->setText("");
		lblEncr->setText("");
		lblConfState->setText("");

		if (!MainProcess::getStatus(res, &len)) {
			break;
		}

		start = res;
		while (*start) {
			bool last = false;
			end = os_strchr(start, '\n');
			if (!end) {
				last = true;
				end = start;
				while (end[0] && end[1])
					end++;
			}
			*end = 0;

			pos = os_strchr(start, '=');
			if (pos) {
				*pos++=0;
				if (!os_strcmp(start, "bssid")) {
					lblBssid->setText(pos);
				} else if (!os_strcmp(start, "ssid")) {
					lblSsid->setText(pos);
				} else if (!os_strcmp(start, "key_mgmt")) {
					lblAuth->setText(pos);
				} else if (!os_strcmp(start, "encription")) {
					lblEncr->setText(pos);
				} else if (!os_strcmp(start, "wps_state")) {
					lblConfState->setText(lblConfState->text() + pos);
				} else if (!strcmp(start, "selected_registrar")) {
					if (*pos && !os_strcmp(pos, "TRUE"))
						lblConfState->setText(lblConfState->text() + " + SR");
				} else if (!os_strcmp(start, "dev_pwd_id")) {
					if (*pos)
						lblConfState->setText(lblConfState->text() + " + " + pos);
				} else if (!os_strcmp(start, "ip_address")) {
					lblIpAddress->setText(pos);
				}
			}

			if (last)
				break;
			start = end + 1;
		}

		if (!leNetKey->text().length()) {
			if (MainProcess::AUTH_OPEN != MainProcess::getAuthType())
				leNetKey->setText(MainProcess::getNetKey());
			else if (MainProcess::ENCR_WEP == MainProcess::getEncrType())
				leNetKey->setText(MainProcess::getWepKey(MainProcess::getWepKeyIndex()));
			leNetKey->setCursorPosition(0);
		}
		if (!lblIpAddress->text().length()) {
			lblIpAddress->setText(MainProcess::getIpAddress());
		}

		ret = true;
	} while (0);

	if (ret) {
		wiz->tabWidget->setEnabled(true);
		if (wiz->tabWidget->isTabEnabled(0) &&
			wiz->tabWidget->isTabEnabled(1))
			wiz->pbBack->setEnabled(true);
	} else {
		leNetKey->setText("");
		lblIpAddress->setText("");

		wiz->tabWidget->setEnabled(false);
		wiz->pbBack->setEnabled(false);
	}
}

