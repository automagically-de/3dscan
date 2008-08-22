#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/videodev2.h>

#include "v4l2.h"
#include "config.h"

struct mmap_buffer {
	void *start;
	size_t length;
};

struct _V4l2Data {
	Config *config;

	int fd;
	gsize width;
	gsize height;
	enum v4l2_buf_type type;

	size_t n_buffers;
	struct mmap_buffer *buffers;
};

static gboolean v4l2_select_format(V4l2Data *data)
{
	struct v4l2_fmtdesc fmtdesc;
	struct v4l2_format format;
	int i = 0;

	while(TRUE) {
		memset(&fmtdesc, 0, sizeof(struct v4l2_fmtdesc));
		fmtdesc.index = i ++;
		fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if(ioctl(data->fd, VIDIOC_ENUM_FMT, &fmtdesc) != 0)
			break;

		g_debug("FMT: %d: %s (%c%c%c%c)", fmtdesc.index, fmtdesc.description,
			(fmtdesc.pixelformat) & 0xFF, (fmtdesc.pixelformat >> 8) & 0xFF,
			(fmtdesc.pixelformat >> 16) & 0xFF,
			(fmtdesc.pixelformat >> 24) & 0xFF);

		if(fmtdesc.pixelformat == V4L2_PIX_FMT_YUYV) {
			memset(&format, 0, sizeof(struct v4l2_format));
			format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			/* query current format */
			if(ioctl(data->fd, VIDIOC_G_FMT, &format) != 0) {
				g_warning("could not get current format: %s (%d)",
					strerror(errno), errno);
				return FALSE;
			}
			format.fmt.pix.pixelformat = fmtdesc.pixelformat;
			format.fmt.pix.width = 960;
			format.fmt.pix.height = 720;
			/* try to set format */
			if(ioctl(data->fd, VIDIOC_S_FMT, &format) != 0) {
				g_warning("could not set current format: %s (%d)",
					strerror(errno), errno);
				return FALSE;
			}
			data->width = format.fmt.pix.width;
			data->height = format.fmt.pix.height;
			data->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			g_debug("size: %dx%d", data->width, data->height);
			return TRUE;
		}
	}
	g_warning("unable to find YUYV format");
	return FALSE;
}

static gboolean v4l2_request_buffers(V4l2Data *data)
{
	struct v4l2_requestbuffers reqbuf;
	struct v4l2_buffer buffer;
	enum v4l2_buf_type type;
	gint32 i;

	/* mmap */
	memset(&reqbuf, 0, sizeof(reqbuf));
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.count = 15;
	if(ioctl(data->fd, VIDIOC_REQBUFS, &reqbuf) == -1) {
		g_warning("requesting buffer failed: %s (%d)", strerror(errno), errno);
		return FALSE;
	}

	if(reqbuf.count < 5) {
		g_warning("too few buffers: %d", reqbuf.count);
		return FALSE;
	}

	data->n_buffers = reqbuf.count;
	data->buffers = g_new0(struct mmap_buffer, data->n_buffers);

	for(i = 0; i < data->n_buffers; i ++) {
		memset(&buffer, 0, sizeof(buffer));
		buffer.index = i;
		buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buffer.memory = V4L2_MEMORY_MMAP;

		/* query buffer */
		if(ioctl(data->fd, VIDIOC_QUERYBUF, &buffer) == -1) {
			g_warning("getting buffer failed: %s (%d)",
				strerror(errno), errno);
			return FALSE;
		}

		data->buffers[i].length = buffer.length;
		data->buffers[i].start = mmap(NULL, buffer.length,
			PROT_READ | PROT_WRITE,
			MAP_SHARED, data->fd, buffer.m.offset);

		if(data->buffers[i].start == MAP_FAILED) {
			g_warning("mmapping failed: %s (%d)", strerror(errno), errno);
			return FALSE;
		}
		g_debug("mmapped %d bytes", data->buffers[i].length);
	}

	/* start streaming */
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(data->fd, VIDIOC_STREAMON, &type) == -1) {
		g_warning("starting stream failed: %s (%d)", strerror(errno), errno);
		return FALSE;
	}

	for(i = 0; i < data->n_buffers; i ++) {
		memset(&buffer, 0, sizeof(buffer));
		buffer.index = i;
		buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buffer.memory = V4L2_MEMORY_MMAP;
		/* queue buffer */
		if(ioctl(data->fd, VIDIOC_QBUF, &buffer) == -1) {
			g_warning("queuing buffer #%d failed: %s (%d)", i, strerror(errno),
				errno);
			return FALSE;
		}
	}
	return TRUE;
}

V4l2Data *v4l2_init(Config *config)
{
	V4l2Data *data;
	struct v4l2_capability cap;
	gchar *devname;
	int ret;

	g_type_init();

	data = g_new0(V4l2Data, 1);
	data->config = config;

	devname = config_get_string(config, "v4l2", "device", "/dev/video0");

	data->fd = open(devname, O_RDWR);
	if(data->fd < 0) {
		g_warning("failed to open %s: %s (%d)", devname,
			strerror(errno), errno);
		g_free(data);
		return NULL;
	}

	ret = ioctl(data->fd, VIDIOC_QUERYCAP, &cap);
	if(ret != 0) {
		g_warning("failed to query capabilities from %s: %s (%d)", devname,
			strerror(errno), errno);
		close(data->fd);
		g_free(data);
		return NULL;
	}

	g_debug("%s: driver: %s, card: %s, businfo: %s, version: %d.%d.%d",
		devname, cap.driver, cap.card, cap.bus_info,
		(cap.version >> 16) & 0xFF, (cap.version >> 8) & 0xFF,
		cap.version & 0xFF);

	if(!v4l2_select_format(data)) {
		close(data->fd);
		g_free(data);
		return NULL;
	}

	if(!v4l2_request_buffers(data)) {
		close(data->fd);
		g_free(data);
		return NULL;
	}

	g_free(devname);
	return data;
}

void v4l2_cleanup(V4l2Data *data)
{
	struct v4l2_buffer buffer;
	gint32 i;

	for(i = 0; i < data->n_buffers; i ++) {
		if(data->buffers[i].start != MAP_FAILED) {
			buffer.index = i;
			buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buffer.memory = V4L2_MEMORY_MMAP;
			ioctl(data->fd, VIDIOC_DQBUF, &buffer);
			munmap(data->buffers[i].start, data->buffers[i].length);
		}
	}
	g_free(data->buffers);
	g_free(data);
}

GdkPixbuf *v4l2_get_pixbuf(V4l2Data *data)
{
	GdkPixbuf *pixbuf;
	struct v4l2_buffer buffer;
	gint32 x, y;
	guint8 *buf, *pixels, *pixel;
	guint32 rowstride, n_channels;
	guint8 cy, cu, cv, r, g, b;

	buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buffer.memory = V4L2_MEMORY_MMAP;
	if(ioctl(data->fd, VIDIOC_DQBUF, &buffer) == -1) {
		g_warning("dequeuing buffer failed: %s (%d)", strerror(errno), errno);
		return NULL;
	}
	buf = data->buffers[buffer.index].start;

	pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
		data->width, data->height);
	pixels = gdk_pixbuf_get_pixels(pixbuf);
	rowstride = gdk_pixbuf_get_rowstride(pixbuf);
	n_channels = gdk_pixbuf_get_n_channels(pixbuf);

	for(y = 0; y < data->height; y ++) {
		for(x = 0; x < data->width; x ++) {
			pixel = pixels + rowstride * y + x * n_channels;
			cy = buf[y * data->width * 2 + x * 2 + 0];
			cu = buf[y * data->width * 2 + (x & ~1UL) * 2 + 1];
			cv = buf[y * data->width * 2 + (x | 1) * 2 + 1];

#if 0
			r = cy + 1.403 * cv;
			g = cy - 0.344 * cu - 0.714 * cv;
			b = cy + 1.770 * cu;
#endif
			r = cy + (1.370705 * (cv - 128));
			g = cy - (0.698001 * (cv - 128)) - (0.337633 * (cu - 128));
			b = cy + (1.732446 * (cu - 128));

			pixel[0] = r;
			pixel[1] = g;
			pixel[2] = b;
		}
	}

	if(ioctl(data->fd, VIDIOC_QBUF, &buffer) == -1) {
		g_warning("queuing buffer failed: %s (%d)", strerror(errno), errno);
	}
	return pixbuf;
}
