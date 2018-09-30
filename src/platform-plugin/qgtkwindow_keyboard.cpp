/****************************************************************************
**
** Copyright (C) 2017 Crimson AS <info@crimson.no>
** Contact: https://www.crimson.no
**
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "qgtkwindow.h"
#include "qgtkhelpers.h"

#include <qpa/qwindowsysteminterface.h>

#include <QtCore/qdebug.h>

#include "CSystrace.h"

static bool qt_gdkKeyEventToFields(GdkEvent *event, int &hardwareKeycode, QString &text, Qt::KeyboardModifiers &qtMods, Qt::Key &qtKey)
{
    hardwareKeycode = gdk_event_get_scancode(event);

    const char *rawText = NULL;
    if (!gdk_event_get_string(event, &rawText)) {
        return false;
    }
    Q_ASSERT(rawText);
    text = QString::fromUtf8(rawText);

    GdkModifierType state = (GdkModifierType)0;
    if (!gdk_event_get_state(event, &state)) {
        return false;
    }
    qtMods = qt_convertToQtKeyboardMods(state);

    guint keyval = 0;
    if (!gdk_event_get_keyval(event, &keyval)) {
        return false;
    }
    qtKey = qt_convertToQtKey(keyval);

    return true;
}

bool QGtkWindow::onKeyPress(GdkEvent *event)
{
    int hardwareKeycode;
    QString text;
    Qt::KeyboardModifiers qtMods;
    Qt::Key qtKey;
    if (!qt_gdkKeyEventToFields(event, hardwareKeycode, text, qtMods, qtKey)) {
        return false;
    }

    TRACE_EVENT_ASYNC_BEGIN0("input", "QGtkWindow::keyDown", (void*)(quintptr)hardwareKeycode);
    TRACE_EVENT0("input", "QGtkWindow::onKeyPress");
    return QWindowSystemInterface::handleExtendedKeyEvent(
        window(),
        gdk_event_get_time(event),
        QEvent::KeyPress,
        qtKey,
        qtMods,
        hardwareKeycode,
        hardwareKeycode,
        0,
        text
    );
}

bool QGtkWindow::onKeyRelease(GdkEvent *event)
{
    int hardwareKeycode;
    QString text;
    Qt::KeyboardModifiers qtMods;
    Qt::Key qtKey;
    if (!qt_gdkKeyEventToFields(event, hardwareKeycode, text, qtMods, qtKey)) {
        return false;
    }

    TRACE_EVENT_ASYNC_END0("input", "QGtkWindow::keyDown", (void*)(quintptr)hardwareKeycode);
    TRACE_EVENT0("input", "QGtkWindow::onKeyRelease");
    return QWindowSystemInterface::handleExtendedKeyEvent(
        window(),
        gdk_event_get_time(event),
        QEvent::KeyRelease,
        qtKey,
        qtMods,
        hardwareKeycode,
        hardwareKeycode,
        0,
        text
    );
}


