/*
  Lightweight access to various Android APIs for Qt

  Author:
  Sergey A. Galin <sergey.galin@gmail.com>

  Distrbuted under The BSD License

  Copyright (c) 2014, DoubleGIS, LLC.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
	this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright notice,
	this list of conditions and the following disclaimer in the documentation
	and/or other materials provided with the distribution.
  * Neither the name of the DoubleGIS, LLC nor the names of its contributors
	may be used to endorse or promote products derived from this software
	without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
  THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <QAndroidQPAPluginGap.h>
#include <QAndroidScreenOrientation.h>
#include "QAndroidDialog.h"

static const char * const c_full_class_name_ = "ru/dublgis/androidhelpers/DialogHelper";
bool QAndroidDialog::interactive_ = true;

Q_DECL_EXPORT void JNICALL Java_DialogHelper_DialogHelper_showMessageCallback(JNIEnv *, jobject, jlong param, jint button)
{
	if (param)
	{
		void * vp = reinterpret_cast<void*>(param);
		QAndroidDialog * proxy = reinterpret_cast<QAndroidDialog*>(vp);
		if (proxy)
		{
			proxy->showMessageCallback(int(button));
			return;
		}
	}

	qWarning() << __FUNCTION__ << "Zero param, button =" << button;
}

QAndroidDialog::QAndroidDialog(QObject * parent /*= 0*/)
	: QObject(parent)
	, delete_self_on_close_(false)
	, result_button_(0)
{
	preloadJavaClasses();
	dialog_helper_.reset(new QJniObject(c_full_class_name_, "J", (jlong)this));

	if (!dialog_helper_->jObject())
	{
		qCritical() << "Failed to create DialogHelper instance!";
		dialog_helper_.reset();
	}
}

QAndroidDialog::~QAndroidDialog()
{
	if (dialog_helper_)
	{
		dialog_helper_->callVoid("cppDestroyed");
		dialog_helper_.reset();
	}
}

void QAndroidDialog::preloadJavaClasses()
{
	static bool s_preloaded = false;

	if (!s_preloaded)
	{
		QAndroidQPAPluginGap::preloadJavaClass(c_full_class_name_);
		QJniClass ov(c_full_class_name_);
		static const JNINativeMethod methods[] = {
			{"getActivity", "()Landroid/app/Activity;", (void*)QAndroidQPAPluginGap::getActivity},
			{"getContext", "()Landroid/content/Context;", (void*)QAndroidQPAPluginGap::getCurrentContext},
			{"showMessageCallback", "(JI)V", (void*)Java_DialogHelper_DialogHelper_showMessageCallback},
		};
		ov.registerNativeMethods(methods, sizeof(methods));
		s_preloaded = true;
	}
}

void QAndroidDialog::setInteractiveMode(bool interactive)
{
	if (interactive_ != interactive)
	{
		interactive_ = interactive;
	}
}

bool QAndroidDialog::isInteractiveMode()
{
	return interactive_;
}

void QAndroidDialog::showMessage(
    const QString & title,
    const QString & explanation,
    const QString & positive_button_text,
    const QString & negative_button_text,
    const QString & neutral_button_text,
    bool pause,
    bool lock_rotation)
{
	if (!isInteractiveMode())
	{
		qDebug() << "Dialog was not shown due to non-interactive mode";
		qDebug() << "title: \"" << title << "\"";
		qDebug() << "explanation: " << explanation << "\"";
		return;
	}

	if (dialog_helper_)
	{
		if (pause)
		{
			lock_rotation = true;
		}

		// TODO: We can't read or lock orientation when we don't have an activity.
		// Currently, we check it via customContextSet(), but this might not always be the right way.
		bool in_activity = !QAndroidQPAPluginGap::customContextSet();
		int orientation = (lock_rotation && in_activity)? QAndroidScreenOrientation::getCurrentFixedOrientation(): -1;
		dialog_helper_->callParamVoid("showMessage",
			"Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;ZIZ",
			QJniLocalRef(title).jObject(),
			QJniLocalRef(explanation).jObject(),
			QJniLocalRef(positive_button_text).jObject(),
			QJniLocalRef(negative_button_text).jObject(),
			QJniLocalRef(neutral_button_text).jObject(),
			jboolean(pause),
			jint(orientation),
			jboolean(in_activity)
		);
	}
	else
	{
		qCritical() << "Failed to show message because DialogHelper instance not created!";
	}
}

void QAndroidDialog::showMessage(
    const QString & title,
    const QString & explanation,
    const QString & positive_button_text,
    const QString & negative_button_text,
    bool pause,
    bool lock_rotation)
{
	showMessage(title, explanation, positive_button_text, negative_button_text, QString::null, pause, lock_rotation);
}

void QAndroidDialog::showMessage(
    const QString & title,
    const QString & explanation,
    const QString & positive_button_text,
    bool pause,
    bool lock_rotation)
{
	showMessage(title, explanation, positive_button_text, QString::null, QString::null, pause, lock_rotation);
}

void QAndroidDialog::showMessageCallback(int button)
{
	qDebug() << __FUNCTION__ << button;

	result_button_ = button;

	switch(button)
	{
		case ANDROID_DIALOGINTERFACE_BUTTON_POSITIVE:
			emit positiveClicked();
			break;

		case ANDROID_DIALOGINTERFACE_BUTTON_NEGATIVE:
			emit negativeClicked();
			break;

		case ANDROID_DIALOGINTERFACE_BUTTON_NEUTRAL:
			emit neutralClicked();
			break;

		case 0:
			emit cancelled();
			break;

		default:
			qWarning() << "Unexpected button number in showMessageCallback:" << button;
			break;
	}

	emit closed(button);
	emit closed();

	if (delete_self_on_close_)
	{
		deleteLater();
	}
}

