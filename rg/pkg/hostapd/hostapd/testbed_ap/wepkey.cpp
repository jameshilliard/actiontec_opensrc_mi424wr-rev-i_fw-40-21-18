/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: wepkey.cpp
//  Description: Setting WEP key source
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
#include "wepkey.h"
#include "mainprocess.h"
#include "testbedap.h"

#include <QMessageBox>
#include <QFile>


WepKey::WepKey(QWidget *wizard, QWidget *parent /* = 0 */, Qt::WindowFlags f /* = 0 */)
: PageTemplate(parent, f)
{
	wiz = reinterpret_cast<TestbedAp *>(wizard);
	setupUi(this);

	connect(rbWepKey1, SIGNAL(clicked()), SLOT(selectKeyIndex1()));
	connect(rbWepKey2, SIGNAL(clicked()), SLOT(selectKeyIndex2()));
	connect(rbWepKey3, SIGNAL(clicked()), SLOT(selectKeyIndex3()));
	connect(rbWepKey4, SIGNAL(clicked()), SLOT(selectKeyIndex4()));
	connect(pbGenWepKey1, SIGNAL(clicked()), SLOT(generateWepKey1()));
	connect(pbGenWepKey2, SIGNAL(clicked()), SLOT(generateWepKey2()));
	connect(pbGenWepKey3, SIGNAL(clicked()), SLOT(generateWepKey3()));
	connect(pbGenWepKey4, SIGNAL(clicked()), SLOT(generateWepKey4()));

	srand(time(0));
}

WepKey::~WepKey()
{
}

bool WepKey::pre_back()
{
	wiz->pbBack->setEnabled(true);
	wiz->pbNext->setEnabled(true);
	wiz->pbCancel->setEnabled(true);

	wiz->pbNext->setFocus();
	return true;
}

bool WepKey::pre_next()
{
	wiz->pbBack->setEnabled(true);
	wiz->pbNext->setEnabled(true);
	wiz->pbCancel->setEnabled(true);

	switch (MainProcess::getWepKeyIndex()) {
	case 1:
	case 2:
	case 3:
	case 4:
		selectKeyIndex(MainProcess::getWepKeyIndex());
		break;
	default:
		QMessageBox::critical(this, label->text(), "Invalid WEP key Index");
		return false;
	}

	leWepKey1->setText(MainProcess::getWepKey(1));
	leWepKey2->setText(MainProcess::getWepKey(2));
	leWepKey3->setText(MainProcess::getWepKey(3));
	leWepKey4->setText(MainProcess::getWepKey(4));

	wiz->pbNext->setFocus();
	return true;
}

bool WepKey::post_next()
{
	bool ret = false;
	int keyIndex;

	do {
		if (!checkInputs()) {
			QMessageBox::critical(this, label->text(), "Input error");
			break;
		}

		if (rbWepKey1->isChecked())
			keyIndex = 1;
		else if (rbWepKey2->isChecked())
			keyIndex = 2;
		else if (rbWepKey3->isChecked())
			keyIndex = 3;
		else if (rbWepKey4->isChecked())
			keyIndex = 4;
		else {
			QMessageBox::critical(this, label->text(), "Cannot set WEP key index");
			break;
		}
		MainProcess::setWepKeyIndex(keyIndex);

		MainProcess::setWepKey(1, leWepKey1->text().toAscii());
		MainProcess::setWepKey(2, leWepKey2->text().toAscii());
		MainProcess::setWepKey(3, leWepKey3->text().toAscii());
		MainProcess::setWepKey(4, leWepKey4->text().toAscii());

		if (!MainProcess::writeConfigFile()) {
			QMessageBox::critical(this, label->text(), "Cannot write main process configuration file");
			break;
		}

		if (!MainProcess::start()) {
			QMessageBox::critical(this, label->text(), "Cannot start main process");
			break;
		}

		ret = true;
	} while (0);

	return ret;
}

void WepKey::cancel()
{
	MainProcess::terminate();
	wiz->close();
}

void WepKey::selectKeyIndex1()
{
	selectKeyIndex(1);
}

void WepKey::selectKeyIndex2()
{
	selectKeyIndex(2);
}

void WepKey::selectKeyIndex3()
{
	selectKeyIndex(3);
}

void WepKey::selectKeyIndex4()
{
	selectKeyIndex(4);
}

void WepKey::selectKeyIndex(int index)
{
	QRadioButton *rb[] = {
		rbWepKey1,
		rbWepKey2,
		rbWepKey3,
		rbWepKey4
	};

	for (int i = 0; i < 4; i++) {
		if (i == (index - 1))
			rb[i]->setChecked(true);
		else
			rb[i]->setChecked(false);
	}
}

void WepKey::generateWepKey1()
{
	char key[26 + 1];

	generateWepKey(key);

	leWepKey1->clear();
	leWepKey1->setText(key);
}

void WepKey::generateWepKey2()
{
	char key[26 + 1];

	generateWepKey(key);

	leWepKey2->clear();
	leWepKey2->setText(key);
}

void WepKey::generateWepKey3()
{
	char key[26 + 1];

	generateWepKey(key);

	leWepKey3->clear();
	leWepKey3->setText(key);
}

void WepKey::generateWepKey4()
{
	char key[26 + 1];

	generateWepKey(key);

	leWepKey4->clear();
	leWepKey4->setText(key);
}

void WepKey::generateWepKey(char *key)
{
	for (int i = 0; i < 26; i++) {
		key[i] = btoa(rand() % 16);
	}
	key[26] = 0;
}

char WepKey::btoa(int b, bool capital /* = true */ )
{
	if ((0 <= b) && (9 >= b)) {
		return b + '0';
	} else if ((0xA <= b) && (0xF >= b)) {
		return (b - 0xA) + (capital?'A':'a');
	} else {
		return '0';
	}
}

bool WepKey::checkInputs()
{
	bool ret = true;
	QLineEdit *le [] = {
		leWepKey1,
		leWepKey2,
		leWepKey3,
		leWepKey4
	};
	QRadioButton *rb [] = {
		rbWepKey1,
		rbWepKey2,
		rbWepKey3,
		rbWepKey4
	};
	int index, i;

	do {
		/* WEP KEY */
		for (index = 0; index < 4; index++) {
			if ((10 == le[index]->text().length()) ||
				(26 == le[index]->text().length())) {
				for (i = 0; i < le[index]->text().length(); i++) {
					if (!isxdigit(*(const char *)(le[index]->text().data() + i)))
						break;
				}
				if (le[index]->text().length() != i) {
					le[index]->setFocus();
					ret = false;
					break;
				}
			} else if ((0 != le[index]->text().length()) &&
					   (5 != le[index]->text().length()) &&
					   (13 != le[index]->text().length())) {
				le[index]->setFocus();
				ret = false;
				break;
			} else if (0 == le[index]->text().length()) {
				if (rb[index]->isChecked()) {
					le[index]->setFocus();
					ret = false;
					break;
				}
			}
		}
		if (!ret)
			break;
	} while (0);

	return ret;
}

