/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: testbedap.h
//  Description: WiFi - Protected Setup external registar graphical user interface header
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

#ifndef TESTBEDAP_H
#define TESTBEDAP_H

#include "ui_testbedap.h"

#include <QLinkedList>

class PageTemplate;
class SetupInterface;
class NetConfig;
class WepKey;
class Status;
class DebugWindow;


class TestbedAp: public QMainWindow, public Ui::TestbedAp
{
Q_OBJECT
public:
	TestbedAp(QWidget *parent = 0, Qt::WindowFlags f = 0);
	~TestbedAp();

public slots:
	virtual void back();
	virtual void next();
	virtual void cancel();

	virtual void close();
	virtual void about();

	void receiveMsgs();

private slots:
	void debugging();
	void changeMode(int mode);

	void writeConfigToken();
	void startRegStaPbc();
	void authRegSta();
	void readConfigToken();
	void startGetConfigPbc();
	void authGetConfig();

	bool startAuthentication();
	bool stopAuthentication();
	void increment();

	void reset();
	void restart();

protected:
	void closeEvent(QCloseEvent *);

private:
	QLinkedList<PageTemplate *> listPage;
	SetupInterface *setupInterface;
	NetConfig *netConfig;
	WepKey *wepKey;
	Status *status;
	DebugWindow *debugWindow;

	QTimer *timer;

	void processCtrlRequest(char *buf, size_t len);
};

#endif // TESTBEDAP_H
