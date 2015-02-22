/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: wepkey.h
//  Description: Setting WEP key header
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

#ifndef WEPKEY_H
#define WEPKEY_H

#include "ui_wepkey.h"
#include "pagetemplate.h"

class TestbedAp;


class WepKey:
public PageTemplate, public Ui::WepKey
{
Q_OBJECT
public:
	WepKey(QWidget *wizard, QWidget *parent = 0, Qt::WindowFlags f = 0);
	~WepKey();

public:
	bool pre_back();
	bool pre_next();
	void cancel();

	bool post_next();

private slots:
	void selectKeyIndex(int index);
	void selectKeyIndex1();
	void selectKeyIndex2();
	void selectKeyIndex3();
	void selectKeyIndex4();
	void generateWepKey(char *key);
	void generateWepKey1();
	void generateWepKey2();
	void generateWepKey3();
	void generateWepKey4();


private:
	char btoa(int b, bool capital = true);
	bool checkInputs();

private:
	TestbedAp *wiz;
};

#endif // WEPKEY_H

