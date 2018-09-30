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
#include <QtCore/qloggingcategory.h>
#include <QtCore/qdebug.h>

#include "CSystrace.h"

Q_LOGGING_CATEGORY(lcMouse, "qt.qpa.gtk.mouse");
Q_LOGGING_CATEGORY(lcMouseMotion, "qt.qpa.gtk.mouse.motion");

static bool qt_gdkMouseEventToFields(GdkEvent *event, Qt::MouseButton &button, QPointF &coords, QPointF &rootCoords, Qt::KeyboardModifiers &qtMods) {
    guint gdkButton = 0;
    if (!gdk_event_get_button(event, &gdkButton)) {
        return false;
    }
    button = qt_convertGButtonToQButton(gdkButton);

    gdouble x = 0, y = 0;
    if (!gdk_event_get_coords(event, &x, &y)) {
        return false;
    }
    coords = QPointF(x, y);

    if (!gdk_event_get_root_coords(event, &x, &y)) {
        return false;
    }
    rootCoords = QPointF(x, y);

    GdkModifierType state = (GdkModifierType)0;
    if (!gdk_event_get_state(event, &state)) {
        return false;
    }
    qtMods = qt_convertToQtKeyboardMods(state);

    return true;
}

bool QGtkWindow::onButtonPress(GdkEvent *event)
{
    TRACE_EVENT0("input", "QGtkWindow::onButtonPress");
    TRACE_EVENT_ASYNC_BEGIN0("input", "QGtkWindow::mouseDown", this);

    // ### would be nice if we could support GDK_2BUTTON_PRESS/GDK_3BUTTON_PRESS
    // directly (and not via emulation internally).

    Qt::MouseButton b;
    QPointF coords, rootCoords;
    Qt::KeyboardModifiers qtMods;
    if (!qt_gdkMouseEventToFields(event, b, coords, rootCoords, qtMods)) {
        return false;
    }

    m_buttons |= b;
    qCDebug(lcMouse) << "Pressed " << b << " at " << coords << rootCoords << " total pressed " << m_buttons;

    bool isTabletEvent = false;
    QWindowSystemInterface::handleMouseEvent(
        window(),
        gdk_event_get_time(event),
        coords,
        rootCoords, // ### root is probably wrong.
        m_buttons,
        qtMods,
        isTabletEvent ? Qt::MouseEventSynthesizedByQt : Qt::MouseEventNotSynthesized
    );
    return true;
}

bool QGtkWindow::onButtonRelease(GdkEvent *event)
{
    TRACE_EVENT0("input", "QGtkWindow::onButtonRelease");

    Qt::MouseButton b;
    QPointF coords, rootCoords;
    Qt::KeyboardModifiers qtMods;
    if (!qt_gdkMouseEventToFields(event, b, coords, rootCoords, qtMods)) {
        return false;
    }

    m_buttons &= ~b;
    qCDebug(lcMouse) << "Released " << b << " at " << coords << rootCoords << " total pressed " << m_buttons;

    bool isTabletEvent = false;
    QWindowSystemInterface::handleMouseEvent(
        window(),
        gdk_event_get_time(event),
        coords,
        rootCoords,
        m_buttons,
        qtMods,
        isTabletEvent ? Qt::MouseEventSynthesizedByQt : Qt::MouseEventNotSynthesized
    );
    TRACE_EVENT_ASYNC_END0("input", "QGtkWindow::mouseDown", this);
    return true;
}

bool QGtkWindow::onMotionNotify(GdkEvent *event)
{
    Qt::MouseButton b;
    QPointF coords, rootCoords;
    Qt::KeyboardModifiers qtMods;
    if (!qt_gdkMouseEventToFields(event, b, coords, rootCoords, qtMods)) {
        return false;
    }

    TRACE_EVENT0("input", "QGtkWindow::onMotionNotify");
    qCDebug(lcMouseMotion) << "Moved mouse at " << coords << rootCoords;

    QPoint mousePos = window()->mapToGlobal(coords.toPoint());
    QCursor::setPos(mousePos);

    bool isTabletEvent = false;
    QWindowSystemInterface::handleMouseEvent(
        window(),
        gdk_event_get_time(event),
        coords,
        rootCoords,
        m_buttons,
        qtMods,
        isTabletEvent ? Qt::MouseEventSynthesizedByQt : Qt::MouseEventNotSynthesized
    );
    return true;
}

bool QGtkWindow::onScrollEvent(GdkEvent *event)
{
    TRACE_EVENT0("input", "QGtkWindow::onScrollEvent");

    Qt::MouseButton b;
    QPointF coords, rootCoords;
    Qt::KeyboardModifiers qtMods;
    if (!qt_gdkMouseEventToFields(event, b, coords, rootCoords, qtMods)) {
        return false;
    }

    QPoint angleDelta;
    QPoint pixelDelta;
    Qt::MouseEventSource source = Qt::MouseEventNotSynthesized;
    Qt::ScrollPhase phase = Qt::ScrollUpdate;

    // We cache the modifiers as they should not change after the scroll has
    // started. Doing that means that you'll zoom text in Creator or something,
    // which is pretty annoying.
    if (!m_scrollStarted) {
        m_scrollStarted = true;
        m_scrollModifiers = qtMods;
        phase = Qt::ScrollBegin;
    }
    if (gdk_event_is_scroll_stop_event(event)) {
        m_scrollStarted = false;
        m_scrollModifiers = Qt::NoModifier;
        phase = Qt::ScrollEnd;
    }

    GdkScrollDirection direction;
    if (!gdk_event_get_scroll_direction(event, &direction)) {
        return false;
    }

    gdouble deltaX, deltaY;
    if (!gdk_event_get_scroll_deltas(event, &deltaX, &deltaY)) {
        return false;
    }

    if (direction == GDK_SCROLL_SMOOTH) {
        // ### I have literally no idea what I'm doing here
        const int pixelsToDegrees = 50;
        angleDelta.setX(-deltaX * pixelsToDegrees);
        angleDelta.setY(-deltaY * pixelsToDegrees);
        source = Qt::MouseEventSynthesizedBySystem;

        pixelDelta.setX(deltaX * pixelsToDegrees);
        pixelDelta.setY(-deltaY * pixelsToDegrees);
    } else if (direction == GDK_SCROLL_UP ||
               direction == GDK_SCROLL_DOWN) {
        angleDelta.setY(qBound(-120, int(deltaY * 10000), 120));
    } else if (direction == GDK_SCROLL_LEFT ||
               direction == GDK_SCROLL_RIGHT) {
        angleDelta.setX(qBound(-120, int(deltaX * 10000), 120));
    } else {
        Q_UNREACHABLE();
    }

    qCDebug(lcMouseMotion) << "Scrolled mouse at " << coords << rootCoords << " angle delta " << angleDelta << " pixelDelta " << pixelDelta << " original deltas " << deltaX << deltaY;

    QWindowSystemInterface::handleWheelEvent(
        window(),
        gdk_event_get_time(event),
        coords,
        rootCoords,
        pixelDelta,
        angleDelta,
        m_scrollModifiers,
        phase,
        source,
        false /* isInverted */
    );
    return true;
}


