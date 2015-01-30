/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/
#ifndef PluginContainerQt_h
#define PluginContainerQt_h


#include <QWidget>

class QGraphicsProxyWidget;

namespace WebCore {

    class PluginView;

    class PluginContainerGix : public QWidget {
        Q_OBJECT
    public:
        PluginContainerGix(PluginView*, QWidget* parent, QGraphicsProxyWidget* proxy = 0);
        ~PluginContainerGix();

        void requestGeometry(const QRect&, const QRegion& clip = QRegion());
        void adjustGeometry();
        QGraphicsProxyWidget* proxy() { return m_proxy; }

    protected:
        virtual void focusInEvent(QFocusEvent*);
        virtual void focusOutEvent(QFocusEvent*);
        virtual bool pfWinEvent(gi_msg_t*);
    private:
        PluginView* m_pluginView;
        QGraphicsProxyWidget* m_proxy;
        QRect m_windowRect;
        QRegion m_clipRegion;
        bool m_hasPendingGeometryChange;
    };
}

#if 0

namespace WebCore {

    class PluginView;

    class PluginContainerQt //: public QX11EmbedContainer
    {
        Q_OBJECT
    public:
        PluginContainerQt(PluginView*, QWidget* parent);
        ~PluginContainerQt();

        void redirectWheelEventsToParent(bool enable = true);

    protected:
        virtual bool pfWinEvent(gi_msg_t*);
        virtual void focusInEvent(QFocusEvent*);
        virtual void focusOutEvent(QFocusEvent*);

    public slots:
        void on_clientClosed();
        void on_clientIsEmbedded();

    private:
        PluginView* m_pluginView;
        QWidget* m_clientWrapper;
    };

    class PluginClientWrapper : public QWidget
    {
    public:
        PluginClientWrapper(QWidget* parent, WId client);
        ~PluginClientWrapper();
        bool pfWinEvent(gi_msg_t*);

    private:
        QWidget* m_parent;
    };
}
#endif

#endif // PluginContainerQt_h
