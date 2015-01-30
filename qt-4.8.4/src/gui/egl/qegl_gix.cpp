

#include <QtCore/qdebug.h>


#include <QtGui/private/qpixmapdata_p.h>
#include <QtGui/qpaintdevice.h>
#include <QtGui/qpixmap.h>
#include <QtGui/qwidget.h>
#include <QtGui/private/qapplication_p.h>
#include <QtGui/private/qimagepixmapcleanuphooks_p.h>

#include <QtGui/qpaintdevice.h>
#include <QtGui/qpixmap.h>
#include <QtGui/qwidget.h>
#include <QtGui/qcolormap.h>

#include "QtGui/private/qegl_p.h"
#include "QtGui/private/qeglcontext_p.h"

#include <EGL/egl.h>
#include <EGL/egl_utils.h>

QT_BEGIN_NAMESPACE


// Set pixel format and other properties based on a paint device.
void QEglProperties::setPaintDeviceFormat(QPaintDevice *dev)
{
    if(!dev)
        return;

    int devType = dev->devType();
    if (devType == QInternal::Image) {
        setPixelFormat(static_cast<QImage *>(dev)->format());
    } else {
        QImage::Format format = QImage::Format_RGB32;
        //if (QApplicationPrivate::instance() && QApplicationPrivate::instance()->useTranslucentEGLSurfaces)
         //   format = QImage::Format_ARGB32_Premultiplied;
        setPixelFormat(format);
    }
}

EGLNativeDisplayType QEgl::nativeDisplay()
{
    return  EGLNativeDisplayType(EGL_DEFAULT_DISPLAY);
}
#include <pthread.h>

struct list_head {
	struct list_head *next, *prev;
};



#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define MY_LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)


#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))


#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)


static __inline__ void __list_add(struct list_head * pnew,
	struct list_head * prev,
	struct list_head * next)
{
	next->prev = pnew;
	pnew->next = next;
	pnew->prev = prev;
	prev->next = pnew;
}

static __inline__ void __list_del(struct list_head * prev,
				  struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

static __inline__ void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
}

static __inline__ void list_add_tail(struct list_head *pnew, struct list_head *head)
{
	__list_add(pnew, head->prev, head);
}

static MY_LIST_HEAD(gles_win_list);
static pthread_mutex_t mlock = PTHREAD_MUTEX_INITIALIZER;
struct gles_win
{
	struct list_head list;
	gi_window_id_t	wid;
	EGLNativeWindowType egl_window;
};


struct gles_win* lookup_gles_window(gi_window_id_t wid)
{
	struct gles_win *t;
	struct list_head *pos;
	struct gles_win *ret = NULL;

	pthread_mutex_lock(&mlock);
	list_for_each (pos, &gles_win_list) {
		t = list_entry(pos, struct gles_win, list);
		if (t->wid == wid){
			ret = t;
			break;
		}
	}
	pthread_mutex_unlock(&mlock);
	return ret;
}

EGLNativeWindowType get_gles_window(gi_window_id_t wid)
{
	struct gles_win *t;
	struct list_head *pos;
	EGLNativeWindowType EGL_win = NULL;
	
	t = lookup_gles_window(wid);

	if (t)
	{
		return t->egl_window;
	}

	t = (struct gles_win *)calloc(1,sizeof(*t));
	if (!t)
	{
		return NULL;
	}

	pthread_mutex_lock(&mlock);

	EGL_win = egl_nativewindow_create_surface(wid,0);
	if (!EGL_win)
	{
		free(t);
		goto fail;
	}

	t->egl_window = EGL_win;
	t->wid = wid;
	list_add_tail(&t->list, &gles_win_list);
fail:
	pthread_mutex_unlock(&mlock);
	return EGL_win;
}

void remove_gles_window(gi_window_id_t wid)
{
	struct gles_win *t;
	EGLNativeWindowType EGL_win = NULL;

	t = lookup_gles_window(wid);
	if (!t){
		return;
	}
	
	pthread_mutex_lock(&mlock);
	EGL_win = t->egl_window;
	list_del(&t->list);
	pthread_mutex_unlock(&mlock);
	egl_nativewindow_destroy(EGL_win);
	free(t);

}


int resize_gles_window(gi_window_id_t wid, int w, int h)
{
	struct gles_win *t;
	EGLNativeWindowType EGL_win = NULL;

	//printf("resize_gles_window = %d,%d\n",w,h);

	t = lookup_gles_window(wid);
	if (!t){
		return 0;
	}
	
	pthread_mutex_lock(&mlock);
	EGL_win = t->egl_window;
	pthread_mutex_unlock(&mlock);
	egl_nativewindow_resize(EGL_win,w,h);
	return 1;

}

void QEgl::releaseNativeWindow(QWidget* widget)
{
	gi_window_id_t wid = widget->winId();
	remove_gles_window(wid);
}

EGLNativeWindowType QEgl::nativeWindow(QWidget* widget)
{
	gi_window_id_t wid = widget->winId();
	EGLNativeWindowType awin;

	awin = (EGLNativeWindowType)get_gles_window(wid); // Might work
	printf("+++ nativeWindow for %d awin=%p\n",wid,awin);
    return awin;
}

EGLNativePixmapType QEgl::nativePixmap(QPixmap*)
{
    qWarning("QEgl: EGL pixmap surfaces not supported on QWS");
    return (EGLNativePixmapType)0;
}




QT_END_NAMESPACE
