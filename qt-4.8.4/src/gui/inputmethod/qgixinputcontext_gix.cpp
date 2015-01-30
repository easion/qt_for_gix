/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "QtGui/qinputcontext.h"
#include "qgixinputcontext_p.h"
#include "qinputcontext_p.h"
//#include "qwsdisplay_qws.h"
//#include "qwsevent_qws.h"
//#include "private/qwscommand_qws_p.h"
//#include "qwindowsystem_qws.h"
#include "qevent.h"
#include "qtextformat.h"

//#include "QtCore/qt_windows.h"

#include <qbuffer.h>

#include <qdebug.h>

#ifndef QT_NO_QWS_INPUTMETHODS

QT_BEGIN_NAMESPACE

static QWidget* activeWidget = 0;

//#define EXTRA_DEBUG

QGixInputContext::QGixInputContext(QObject *parent)
    :QInputContext(parent)
{
}

void QGixInputContext::reset()
{
    //QPaintDevice::qwsDisplay()->resetIM();
	printf("### QGixInputContext::%s line %d (pid%d)not implemented ###\n",__FUNCTION__, __LINE__,getpid());
	
	gi_ime_associate_window(0, NULL);
}


void QGixInputContext::setFocusWidget( QWidget *w )
{
    QWidget *oldFocus = focusWidget();
    if (oldFocus == w)
        return;

	printf("### QGixInputContext::%s line %d (pid%d)not implemented ###\n",__FUNCTION__, __LINE__,getpid());

    if (w) {
        QGixInputContext::updateImeStatus(w, true);
    } else {
        if (oldFocus)
            QGixInputContext::updateImeStatus(oldFocus, false);
    }

    if (oldFocus) {
        QWidget *tlw = oldFocus->window();
        int winid = tlw->internalWinId();

        int widgetid = oldFocus->internalWinId();

		//int oldwinid = tlw->winId();		
		
		gi_ime_associate_window(winid, NULL); //new
		//gi_ime_set_server_window();

        //QPaintDevice::qwsDisplay()->sendIMUpdate(QWSInputMethod::FocusOut, winid, widgetid);
    }

    QInputContext::setFocusWidget(w);

    if (!w)
        return;

    QWidget *tlw = w->window();
    int winid = tlw->winId();

    int widgetid = w->winId();

	gi_composition_form_t attr;	
	attr.x = 0;//
	attr.y = 0;//	
	gi_ime_associate_window(winid, &attr);
	//gi_ime_set_server_window(winid, &attr);
    //QPaintDevice::qwsDisplay()->sendIMUpdate(QWSInputMethod::FocusIn, winid, widgetid);
    //setfocus ???

    update();
}


void QGixInputContext::widgetDestroyed(QWidget *w)
{
	printf("### QGixInputContext::%s line %d (pid%d)not implemented ###\n",__FUNCTION__, __LINE__,getpid());
    if (w == QT_PREPEND_NAMESPACE(activeWidget))
        QT_PREPEND_NAMESPACE(activeWidget) = 0;
    QInputContext::widgetDestroyed(w);
}

void QGixInputContext::update()
{
    QWidget *w = focusWidget();
    if (!w)
        return;
	printf("### QGixInputContext::%s line %d (pid%d)not implemented ###\n",__FUNCTION__, __LINE__,getpid());

    QWidget *tlw = w->window();
    int winid = tlw->winId();

    int widgetid = w->winId();

	gi_composition_form_t attr;	
	attr.x = 0;//
	attr.y = 0;//	
	gi_ime_associate_window(winid, &attr);
	//gi_ime_set_server_window(winid, &attr);

   // QPaintDevice::qwsDisplay()->sendIMUpdate(QWSInputMethod::Update, winid, widgetid);

}

void QGixInputContext::mouseHandler( int x, QMouseEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease){
        //QPaintDevice::qwsDisplay()->sendIMMouseEvent( x, event->type() == QEvent::MouseButtonPress );
		printf("### QGixInputContext::%s line %d (pid%d)not implemented ###\n",__FUNCTION__, __LINE__,getpid());
	}
}

QWidget *QGixInputContext::activeWidget()
{
    return QT_PREPEND_NAMESPACE(activeWidget);
}


bool QGixInputContext::isComposing() const
{
    return QT_PREPEND_NAMESPACE(activeWidget) != 0;
}

bool QGixInputContext::translateIMQueryEvent(QWidget *w, const gi_msg_t *e)
{
    Qt::InputMethodQuery type = Qt::ImMicroFocus; //static_cast<Qt::InputMethodQuery>(e->simpleData.property);
    QVariant result = w->inputMethodQuery(type);
    QWidget *tlw = w->window();
    int winId = tlw->winId();

    if ( type == Qt::ImMicroFocus ) {
        // translate to relative to tlw
        QRect mf = result.toRect();
        mf.moveTopLeft(w->mapTo(tlw,mf.topLeft()));
        result = mf;
    }
    printf("### QGixInputContext::%s line %d (pid%d)not implemented ###\n",__FUNCTION__, __LINE__,getpid());

	//gi_msg_t event;

    //QPaintDevice::qwsDisplay()->sendIMResponse(winId, e->simpleData.property, result);
	//gi_post_wc_string(winId, result,1,&event, FALSE);

    return false;
}

bool QGixInputContext::translateIMInitEvent(const gi_msg_t *e)
{
    Q_UNUSED(e);
    qDebug("### QGixInputContext::%s line %d (pid%d)not implemented ###\n",__FUNCTION__, __LINE__,getpid());
    return false;
}

bool QGixInputContext::translateIMEvent(QWidget *w, const gi_msg_t *e)
{
#if 1
	QInputMethodEvent ime;
    //ime.setCommitString(currentText);
#else
	QList<QInputMethodEvent::Attribute> attrs;
    attrs << QInputMethodEvent::Attribute(QInputMethodEvent::Selection, startPos, endPos-startPos, QVariant());
    QInputMethodEvent ime(QString(), attrs);
#endif
	printf("### QGixInputContext::%s line %d (pid%d)not implemented ###\n",__FUNCTION__, __LINE__,getpid());
    extern bool qt_sendSpontaneousEvent(QObject *, QEvent *); //qapplication_qws.cpp
    qt_sendSpontaneousEvent(w, &ime);

    return true;
}


Q_GUI_EXPORT void (*qt_qws_inputMethodStatusChanged)(QWidget*) = 0;

void QGixInputContext::updateImeStatus(QWidget *w, bool hasFocus)
{
    Q_UNUSED(hasFocus);
	printf("### QGixInputContext::%s line %d (pid%d)not implemented ###\n",__FUNCTION__, __LINE__,getpid());

    if (!w || !qt_qws_inputMethodStatusChanged)
        return;
    qt_qws_inputMethodStatusChanged(w);
}


QT_END_NAMESPACE

#endif // QT_NO_QWS_INPUTMETHODS
