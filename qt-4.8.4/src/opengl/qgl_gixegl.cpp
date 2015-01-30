
#include "qgl.h"


#include <private/qgl_p.h>
#include <private/qbackingstore_p.h>
#include <private/qfont_p.h>
#include <private/qfontengine_p.h>
#include <private/qpaintengine_opengl_p.h>
#include <private/qwidget_p.h> // to access QWExtra
#include <private/qnativeimagehandleprovider_p.h>
#include <private/qapplication_p.h>
#include <private/qgraphicssystem_p.h>
#include <EGL/egl.h>
#include <EGL/egl_utils.h>

#include "qgl_egl_p.h"
#include "qcolormap.h"
#include "qglpixelbuffer.h"



#include <qpixmap.h>
#include <qtimer.h>
#include <qapplication.h>
#include <qstack.h>
#include <qdesktopwidget.h>
#include <qdebug.h>
#include <qvarlengtharray.h>



QT_BEGIN_NAMESPACE

#if 1

/*
    QGLTemporaryContext implementation
*/

class QGLTemporaryContextPrivate
{
public:
	void remove_native_window(){
		gi_window_id_t id = egl_nativewindow_get_id(this->window);
		egl_nativewindow_destroy(this->window);
		gi_destroy_window(id);
	}
    bool initialized;


	EGLNativeWindowType window;
    EGLContext context;
    EGLSurface surface;
    EGLDisplay display;
};

QGLTemporaryContext::QGLTemporaryContext(bool, QWidget *)
    : d(new QGLTemporaryContextPrivate)
{
    d->initialized = false;
    d->window = 0;
    d->context = 0;
    d->surface = 0;
    int screen = 0;

    d->display = QEgl::display();

    EGLConfig config;
    int numConfigs = 0;
    EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
#ifdef QT_OPENGL_ES_2
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
#endif
        EGL_NONE
    };

    eglChooseConfig(d->display, attribs, &config, 1, &numConfigs);
    if (!numConfigs) {
        qWarning("QGLTemporaryContext: No EGL configurations available.");
        return;
    }


	d->window = GLES_create_native_window(1,1,"QT GLES2 Temporary", FALSE);
    d->surface = eglCreateWindowSurface(d->display, config, (EGLNativeWindowType) d->window, NULL);
	printf("QGLTemporaryContext for %d awin=%p\n",egl_nativewindow_get_id(d->window),d->window);

    if (d->surface == EGL_NO_SURFACE) {
        qWarning("QGLTemporaryContext: Error creating EGL surface.");
		d->remove_native_window();
        return;
    }

    EGLint contextAttribs[] = {
#ifdef QT_OPENGL_ES_2
        EGL_CONTEXT_CLIENT_VERSION, 2,
#endif
        EGL_NONE
    };
    d->context = eglCreateContext(d->display, config, 0, contextAttribs);
    if (d->context != EGL_NO_CONTEXT
        && eglMakeCurrent(d->display, d->surface, d->surface, d->context))
    {
        d->initialized = true;
    } else {
        qWarning("QGLTemporaryContext: Error creating EGL context.");
        eglDestroySurface(d->display, d->surface);
        //XDestroyWindow(X11->display, d->window);
		d->remove_native_window();
    }
}



QGLTemporaryContext::~QGLTemporaryContext()
{
    if (d->initialized) {
        eglMakeCurrent(d->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(d->display, d->context);
        eglDestroySurface(d->display, d->surface);
		d->remove_native_window();
    }
}

#else


class QGLTemporaryContextPrivate
{
public:
    QGLWidget *widget;
};

QGLTemporaryContext::QGLTemporaryContext(bool, QWidget *)
    : d(new QGLTemporaryContextPrivate)
{
    d->widget = new QGLWidget;
    d->widget->makeCurrent();
}

QGLTemporaryContext::~QGLTemporaryContext()
{
    delete d->widget;
}

#endif

bool QGLFormat::hasOpenGLOverlays()
{
    return false;
}


void QGLWidget::resizeEvent(QResizeEvent *)
{
    Q_D(QGLWidget);
    if (!isValid())
        return;
    makeCurrent();
    if (!d->glcx->initialized())
        glInit();
    resizeGL(width(), height());
    //handle overlay
}

const QGLContext* QGLWidget::overlayContext() const
{
    return 0;
}

void QGLWidget::makeOverlayCurrent()
{
    //handle overlay
}

void QGLWidget::updateOverlayGL()
{
    //handle overlay
}

void QGLWidget::setContext(QGLContext *context, const QGLContext* shareContext, bool deleteOldContext)
{
    Q_D(QGLWidget);
    if (context == 0) {
        qWarning("QGLWidget::setContext: Cannot set null context");
        return;
    }
    if (!context->deviceIsPixmap() && context->device() != this) {
        qWarning("QGLWidget::setContext: Context must refer to this widget");
        return;
    }

    if (d->glcx)
        d->glcx->doneCurrent();
    QGLContext* oldcx = d->glcx;
    d->glcx = context;

    bool createFailed = false;
    if (!d->glcx->isValid()) {
        // Create the QGLContext here, which in turn chooses the EGL config
        // and creates the EGL context:
        if (!d->glcx->create(shareContext ? shareContext : oldcx))
            createFailed = true;
    }
    if (createFailed) {
        if (deleteOldContext)
            delete oldcx;
        return;
    }
    d->eglSurfaceWindowId = winId(); // Remember the window id we created the surface for
}

void QGLWidgetPrivate::init(QGLContext *context, const QGLWidget* shareWidget)
{
    Q_Q(QGLWidget);

#if 0
    QGLScreen *glScreen = glScreenForDevice(q);
    if (glScreen) {
        wsurf = static_cast<QWSGLWindowSurface*>(glScreen->createSurface(q));
        q->setWindowSurface(wsurf);
    }
#endif

    initContext(context, shareWidget);

    if (q->isValid() && glcx->format().hasOverlay()) {
        //no overlay
        qWarning("QtOpenGL ES doesn't currently support overlays");
    }
}

void QGLWidgetPrivate::cleanupColormaps()
{
}

const QGLColormap & QGLWidget::colormap() const
{
    return d_func()->cmap;
}

void QGLWidget::setColormap(const QGLColormap &)
{
}

// Re-creates the EGL surface if the window ID has changed or if there isn't a surface
void QGLWidgetPrivate::recreateEglSurface()
{
    Q_Q(QGLWidget);

    WId currentId = q->winId();

    // If the window ID has changed since the surface was created, we need to delete the
    // old surface before re-creating a new one. Note: This should not be the case as the
    // surface should be deleted before the old window id.
    if (glcx->d_func()->eglSurface != EGL_NO_SURFACE && (currentId != eglSurfaceWindowId)) {
        qWarning("EGL surface for deleted window %lx was not destroyed", uint(eglSurfaceWindowId));
        glcx->d_func()->destroyEglSurfaceForDevice();
    }

    if (glcx->d_func()->eglSurface == EGL_NO_SURFACE) {
        glcx->d_func()->eglSurface = glcx->d_func()->eglContext->createSurface(q);
        eglSurfaceWindowId = currentId;
    }
}


bool QGLContext::chooseContext(const QGLContext* shareContext)
{
    Q_D(QGLContext);

    // Validate the device.
    if (!device())
        return false;
    int devType = device()->devType();
    if (devType != QInternal::Pixmap && devType != QInternal::Image && devType != QInternal::Widget) {
        qWarning("QGLContext::chooseContext(): Cannot create QGLContext's for paint device type %d", devType);
        return false;
    }

    // Get the display and initialize it.
    d->eglContext = new QEglContext();
    d->ownsEglContext = true;
    d->eglContext->setApi(QEgl::OpenGL);

    // Construct the configuration we need for this surface.
    QEglProperties configProps;
    qt_eglproperties_set_glformat(configProps, d->glFormat);
    configProps.setDeviceType(devType);
    configProps.setPaintDeviceFormat(device());
    configProps.setRenderableType(QEgl::OpenGL);

    // Search for a matching configuration, reducing the complexity
    // each time until we get something that matches.
    if (!d->eglContext->chooseConfig(configProps)) {
        delete d->eglContext;
        d->eglContext = 0;
        return false;
    }

    // Inform the higher layers about the actual format properties.
    qt_glformat_from_eglconfig(d->glFormat, d->eglContext->config());


    // Do don't create the EGLSurface for everything.
    //    QWidget - yes, create the EGLSurface and store it in QGLContextPrivate::eglSurface
    //    QGLWidget - yes, create the EGLSurface and store it in QGLContextPrivate::eglSurface
    //    QPixmap - yes, create the EGLSurface but store it in QX11PixmapData::gl_surface
    //    QGLPixelBuffer - no, it creates the surface itself and stores it in QGLPixelBufferPrivate::pbuf

    if (devType == QInternal::Widget) {
        if (d->eglSurface != EGL_NO_SURFACE)
            eglDestroySurface(d->eglContext->display(), d->eglSurface);
		//QEgl::releaseNativeWindow();
        // extraWindowSurfaceCreationProps default to NULL unless were specifically set before
        //d->eglSurface = QEgl::createSurface(device(), d->eglContext->config(), d->extraWindowSurfaceCreationProps);
        //XFlush(X11->display);
        //setWindowCreated(true);
    }

    // Create a new context for the configuration.
    if (!d->eglContext->createContext
            (shareContext ? shareContext->d_func()->eglContext : 0)) {
        delete d->eglContext;
        d->eglContext = 0;
        return false;
    }
    d->sharing = d->eglContext->isSharing();
    if (d->sharing && shareContext)
        const_cast<QGLContext *>(shareContext)->d_func()->sharing = true;

#if defined(EGL_VERSION_1_1)
    if (d->glFormat.swapInterval() != -1 && devType == QInternal::Widget)
        eglSwapInterval(d->eglContext->display(), d->glFormat.swapInterval());
#endif

	// Create the EGL surface to draw into.
    d->eglSurface = d->eglContext->createSurface(device());
    if (d->eglSurface == EGL_NO_SURFACE) {
        delete d->eglContext;
        d->eglContext = 0;
        return false;
    }

    return true;
}

QT_END_NAMESPACE
