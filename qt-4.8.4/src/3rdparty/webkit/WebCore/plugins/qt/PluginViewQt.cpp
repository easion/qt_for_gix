/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
 * Copyright (C) 2009 Girish Ramakrishnan <girish@forwardbias.in>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PluginView.h"

#if USE(JSC)
#include "BridgeJSC.h"
#endif
#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "FloatPoint.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "HostWindow.h"
#include "IFrameShimSupport.h"
#include "Image.h"
#if USE(JSC)
#include "JSDOMBinding.h"
#endif
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlatformKeyboardEvent.h"
#include "PluginContainerQt.h"
#include "PluginDebug.h"
#include "PluginPackage.h"
#include "PluginMainThreadScheduler.h"
#include "QWebPageClient.h"
#include "RenderLayer.h"
#include "Settings.h"
#include "npruntime_impl.h"
#include "qwebpage_p.h"
#if USE(JSC)
#include "runtime_root.h"
#endif

#include "npfunctions.h"
#include "npinterface.h"

#include "qgraphicswebview.h"
#include "qwebframe.h"
#include "qwebframe_p.h"

#include <QGraphicsProxyWidget>
#include <QKeyEvent>
#include <QPixmap>
#include <QRegion>
#include <QStyleOptionGraphicsItem>
#include <QVector>


#include <QApplication>
#include <QDesktopWidget>
#include <QGraphicsWidget>
#include <QKeyEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QWidget>

#ifdef Q_WS_GIX
#include <gi/gi.h>
#endif

#include <runtime/JSLock.h>
#include <runtime/JSValue.h>


typedef void (*_qtwebkit_page_plugin_created)(QWebFrame*, void*, void*); // frame, plugin instance, plugin functions
static _qtwebkit_page_plugin_created qtwebkit_page_plugin_created = 0;
QWEBKIT_EXPORT void qtwebkit_setPluginCreatedCallback(_qtwebkit_page_plugin_created cb)
{
    qtwebkit_page_plugin_created = cb;
}

using JSC::ExecState;
using JSC::Interpreter;
using JSC::JSLock;
using JSC::JSObject;
using JSC::UString;

using namespace std;

using namespace WTF;

namespace WebCore {

using namespace HTMLNames;

#if USE(ACCELERATED_COMPOSITING)
class PluginGraphicsLayerQt : public QGraphicsWidget {
public:
    PluginGraphicsLayerQt(PluginView* view) : m_view(view)
    {
        setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
    }

    ~PluginGraphicsLayerQt() { }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0)
    {
        Q_UNUSED(widget);

        m_view->m_npWindow.ws_info = (void*)(painter);
        m_view->setNPWindowIfNeeded();

        painter->save();
        QRectF clipRect(QPointF(0, 0), QSizeF(m_view->frameRect().size()));
        if (option && !option->exposedRect.isEmpty())
            clipRect &= option->exposedRect;
        painter->setClipRect(clipRect);

        QRect rect = clipRect.toRect();
       // QPaintEvent ev(rect);
       // QEvent& npEvent = ev;
		NPEvent npEvent;
		npEvent.type = GI_MSG_EXPOSURE;
		npEvent.ev_window = 0;
		npEvent.body.rect.x = rect.x();
	    npEvent.body.rect.y = rect.y();
	    npEvent.body.rect.w = rect.x() + rect.width(); 
	    npEvent.body.rect.h = rect.y() + rect.height(); 


        m_view->dispatchNPEvent(npEvent);

        painter->restore();
    }

private:
    PluginView* m_view;
};

bool PluginView::shouldUseAcceleratedCompositing() const
{
    return m_parentFrame->page()->chrome()->client()->allowsAcceleratedCompositing()
           && m_parentFrame->page()->settings()
           && m_parentFrame->page()->settings()->acceleratedCompositingEnabled();
}
#endif

void PluginView::updatePluginWidget()
{
    if (!parent())
        return;
    ASSERT(parent()->isFrameView());
    FrameView* frameView = static_cast<FrameView*>(parent());
    IntRect oldWindowRect = m_windowRect;
    IntRect oldClipRect = m_clipRect;
    
    m_windowRect = IntRect(frameView->contentsToWindow(frameRect().location()), frameRect().size());
    
    m_clipRect = windowClipRect();
    m_clipRect.move(-m_windowRect.x(), -m_windowRect.y());
    if (m_windowRect == oldWindowRect && m_clipRect == oldClipRect)
        return;

    setNPWindowIfNeeded();
}

void PluginView::setFocus(bool focused)
{
    if (platformPluginWidget()) {
        if (focused)
            platformPluginWidget()->setFocus(Qt::OtherFocusReason);
    } else {
        Widget::setFocus(focused);
    }
}

void PluginView::show()
{
    setSelfVisible(true);

    if (isParentVisible() && platformPluginWidget())
        platformPluginWidget()->setVisible(true);
}

void PluginView::hide()
{
    setSelfVisible(false);

    if (isParentVisible() && platformPluginWidget())
        platformPluginWidget()->setVisible(false);
}

void PluginView::paint(GraphicsContext* context, const IntRect& rect)
{
    if (!m_isStarted) {
        paintMissingPluginIcon(context, rect);
        return;
    }
    
    if (context->paintingDisabled())
        return;
    m_npWindow.ws_info = (void*)(context->platformContext());
    setNPWindowIfNeeded();

    if (m_isWindowed && platformPluginWidget())
        static_cast<PluginContainerGix*>(platformPluginWidget())->adjustGeometry();

    if (m_isWindowed)
        return;

#if USE(ACCELERATED_COMPOSITING)
    if (m_platformLayer)
        return;
#endif

    context->save();
    IntRect clipRect(rect);
    clipRect.intersect(frameRect());
    context->clip(clipRect);
    context->translate(frameRect().location().x(), frameRect().location().y());

    //QPaintEvent ev(rect);
    //QEvent& npEvent = ev;
	NPEvent npEvent;
	npEvent.type = GI_MSG_EXPOSURE;
	npEvent.ev_window = 0;
	npEvent.body.rect.x = rect.x();
	npEvent.body.rect.y = rect.y();
	npEvent.body.rect.w = rect.x() + rect.width(); 
	npEvent.body.rect.h = rect.y() + rect.height(); 
    dispatchNPEvent(npEvent);

    context->restore();
}

// TODO: Unify across ports.
bool PluginView::dispatchNPEvent(NPEvent& event)
{
    if (!m_plugin->pluginFuncs()->event)
        return false;

    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(JSC::SilenceAssertionsOnly);

    setCallingPlugin(true);
    bool accepted = m_plugin->pluginFuncs()->event(m_instance, &event);
    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);

    return accepted;
}

void PluginView::handleKeyboardEvent(KeyboardEvent* event)
{
    if (m_isWindowed)
        return;

    ASSERT(event->keyEvent()->qtEvent());
	NPEvent npEvent; //FIXME DPP
    //QEvent& npEvent = *(event->keyEvent()->qtEvent());
    if (!dispatchNPEvent(npEvent))
        event->setDefaultHandled();
}

void PluginView::handleMouseEvent(MouseEvent* event)
{
    if (m_isWindowed)
        return;

    if (event->type() == eventNames().mousedownEvent) {
        // Give focus to the plugin on click
        if (Page* page = m_parentFrame->page())
            page->focusController()->setActive(true);

        focusPluginElement();
    }

    QEvent::Type type;
    if (event->type() == eventNames().mousedownEvent)
        type = QEvent::MouseButtonPress;
    else if (event->type() == eventNames().mousemoveEvent)
        type = QEvent::MouseMove;
    else if (event->type() == eventNames().mouseupEvent)
        type = QEvent::MouseButtonRelease;
    else
        return;

    QPoint position(event->offsetX(), event->offsetY());
    Qt::MouseButton button;
    switch (event->which()) {
    case 1:
        button = Qt::LeftButton;
        break;
    case 2:
        button = Qt::MidButton;
        break;
    case 3:
        button = Qt::RightButton;
        break;
    default:
        button = Qt::NoButton;
    }
    Qt::KeyboardModifiers modifiers = 0;
    if (event->ctrlKey())
        modifiers |= Qt::ControlModifier;
    if (event->altKey())
        modifiers |= Qt::AltModifier;
    if (event->shiftKey())
        modifiers |= Qt::ShiftModifier;
    if (event->metaKey())
        modifiers |= Qt::MetaModifier;
    QMouseEvent mouseEvent(type, position, button, button, modifiers); 
    //QEvent& npEvent = mouseEvent;
	NPEvent npEvent;
    if (!dispatchNPEvent(npEvent))
        event->setDefaultHandled();
}

void PluginView::setParent(ScrollView* parent)
{
    Widget::setParent(parent);

    if (parent) {
        init();
        if (m_status == PluginStatusLoadedSuccessfully)
            updatePluginWidget();
    }
}

void PluginView::setNPWindowRect(const IntRect&)
{
    if (!m_isWindowed)
        setNPWindowIfNeeded();
}

void PluginView::setNPWindowIfNeeded()
{
    if (!m_isStarted || !parent() || !m_plugin->pluginFuncs()->setwindow)
        return;
    if (m_isWindowed) {
        ASSERT(platformPluginWidget());
        platformPluginWidget()->setGeometry(m_windowRect);
        // if setMask is set with an empty QRegion, no clipping will
        // be performed, so in that case we hide the plugin view
        platformPluginWidget()->setVisible(!m_clipRect.isEmpty());
        platformPluginWidget()->setMask(QRegion(m_clipRect));

        m_npWindow.x = m_windowRect.x();
        m_npWindow.y = m_windowRect.y();

        m_npWindow.clipRect.left = max(0, m_clipRect.x());
        m_npWindow.clipRect.top = max(0, m_clipRect.y());
        m_npWindow.clipRect.right = m_clipRect.x() + m_clipRect.width();
        m_npWindow.clipRect.bottom = m_clipRect.y() + m_clipRect.height();
    
    } else {
        // always call this method before painting.
        m_npWindow.x = m_windowRect.x();
        m_npWindow.y = m_windowRect.y();
    
        m_npWindow.clipRect.left = 0;
        m_npWindow.clipRect.top = 0;
        m_npWindow.clipRect.right = m_windowRect.width();
        m_npWindow.clipRect.bottom = m_windowRect.height();
        m_npWindow.window = 0;
    }

    m_npWindow.width = m_windowRect.width();
    m_npWindow.height = m_windowRect.height();
    
    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(JSC::SilenceAssertionsOnly);
    setCallingPlugin(true);
    m_plugin->pluginFuncs()->setwindow(m_instance, &m_npWindow);
    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);
}

void PluginView::setParentVisible(bool visible)
{
    if (isParentVisible() == visible)
        return;

    Widget::setParentVisible(visible);

    if (isSelfVisible() && platformPluginWidget())
        platformPluginWidget()->setVisible(visible);
}

NPError PluginView::handlePostReadFile(Vector<char>& buffer, uint32_t len, const char* buf)
{
    notImplemented();
    return NPERR_NO_ERROR;
}

bool PluginView::platformGetValueStatic(NPNVariable variable, void* value, NPError* result)
{
    switch (variable) {
    case NPNVjavascriptEnabledBool:
        *static_cast<NPBool*>(value) = true;
        *result = NPERR_NO_ERROR;
        return true;

    case NPNVSupportsWindowless:
        *static_cast<NPBool*>(value) = true;
        *result = NPERR_NO_ERROR;
        return true;

    default:
        return false;
    }
}

bool PluginView::platformGetValue(NPNVariable, void*, NPError*)
{
    return false;
}

void PluginView::invalidateRect(const IntRect& rect)
{
#if USE(ACCELERATED_COMPOSITING) && !USE(TEXTURE_MAPPER)
    if (m_platformLayer) {
        m_platformLayer->update(QRectF(rect));
        return;
    }
#endif

    if (m_isWindowed) {
        platformWidget()->update(rect);
        return;
    }
    
    invalidateWindowlessPluginRect(rect);
}
    
void PluginView::invalidateRect(NPRect* rect)
{
    if (m_isWindowed)
        return;
    if (!rect) {
        invalidate();
        return;
    }
    IntRect r(rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top);
    m_invalidRects.append(r);
    if (!m_invalidateTimer.isActive())
        m_invalidateTimer.startOneShot(0.001);
}

void PluginView::invalidateRegion(NPRegion region)
{
    if (m_isWindowed)
        return;
    
    if (!region) 
        return;
 #if 0   
    QVector<gi_boxrec_t> rects = region->rects();
    for (int i = 0; i < rects.size(); ++i) {
        const QRect& qRect = rects.at(i);
        m_invalidRects.append(qRect);
        if (!m_invalidateTimer.isActive())
            m_invalidateTimer.startOneShot(0.001);
    }
#else
	gi_boxrec_t* rects = region->rects;
    for (int i = 0; i < region->numRects; ++i) {
        //const QRect& qRect = rects.at(i);
		const QRect qRect(rects->x1,rects->x2,rects->x2-rects->x1,rects->y2-rects->y1);
        m_invalidRects.append(qRect);
        if (!m_invalidateTimer.isActive())
            m_invalidateTimer.startOneShot(0.001);
		rects++;
    }
#endif
}

void PluginView::forceRedraw()
{
    if (m_isWindowed)
        return;
    invalidate();
}

bool PluginView::platformStart()
{
    ASSERT(m_isStarted);
    ASSERT(m_status == PluginStatusLoadedSuccessfully);
    
    show();

    if (m_isWindowed) {
        QWebPageClient* client = m_parentFrame->view()->hostWindow()->platformPageClient();
        QGraphicsProxyWidget* proxy = 0;
        if (QGraphicsWebView *webView = qobject_cast<QGraphicsWebView*>(client->pluginParent()))
            proxy = new QGraphicsProxyWidget(webView);

        PluginContainerGix* container = new PluginContainerGix(this, proxy ? 0 : client->ownerWidget(), proxy);
        setPlatformWidget(container);
        if (proxy)
            proxy->setWidget(container);
        
        m_npWindow.type = NPWindowTypeWindow;
        m_npWindow.window = (void*)platformPluginWidget();
        //m_npWindow.window = (void*)platformPluginWidget()->handle();

    } else {
        setPlatformWidget(0);
        m_npWindow.type = NPWindowTypeDrawable;
        m_npWindow.window = 0; // Not used?
#if USE(ACCELERATED_COMPOSITING) && !USE(TEXTURE_MAPPER)
        if (shouldUseAcceleratedCompositing()) {
            m_platformLayer = new PluginGraphicsLayerQt(this);
            m_element->setNeedsStyleRecalc(SyntheticStyleChange);
        }
#endif
    }    
    updatePluginWidget();
    setNPWindowIfNeeded();

    if (qtwebkit_page_plugin_created)
        qtwebkit_page_plugin_created(QWebFramePrivate::kit(m_parentFrame.get()), m_instance, (void*)(m_plugin->pluginFuncs()));

    return true;
}

void PluginView::platformDestroy()
{
    if (platformPluginWidget()) {
        PluginContainerGix* container = static_cast<PluginContainerGix*>(platformPluginWidget());
        if (container && container->proxy())
            delete container->proxy();
        else
            delete container;
    }
}

void PluginView::halt()
{
}

void PluginView::restart()
{
}

#if USE(ACCELERATED_COMPOSITING)
PlatformLayer* PluginView::platformLayer() const
{
    return m_platformLayer.get();
}
#endif


#if defined(XP_UNIX) && ENABLE(NETSCAPE_PLUGIN_API)
void PluginView::handleFocusInEvent()
{
}

void PluginView::handleFocusOutEvent()
{
}
#endif

} // namespace WebCore

