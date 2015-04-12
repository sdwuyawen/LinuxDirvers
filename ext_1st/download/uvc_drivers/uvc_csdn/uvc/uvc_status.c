/*
 *      uvc_status.c  --  USB Video Class driver
 *
 *      Copyright (C) 2007
 *          Laurent Pinchart (laurent.pinchart@skynet.be)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/usb.h>

#include "uvcvideo.h"

static void uvc_event_streaming(struct uvc_device *dev, __u8 *data, int len)
{
	if (len < 3) {
		uvc_trace(UVC_TRACE_STATUS, "Invalid streaming status event "
				"received.\n");
		return;
	}

	if (data[2] == 0) {
		if (len < 4)
			return;
		uvc_trace(UVC_TRACE_STATUS, "Button (intf %u) %s len %d\n",
			data[1], data[3] ? "pressed" : "released", len);
	} else {
		uvc_trace(UVC_TRACE_STATUS, "Stream %u error event %02x %02x "
			"len %d.\n", data[1], data[2], data[3], len);
	}
}

static void uvc_event_control(struct uvc_device *dev, __u8 *data, int len)
{
	char *attrs[3] = { "value", "info", "failure" };

	if (len < 6 || data[2] != 0 || data[4] > 2) {
		uvc_trace(UVC_TRACE_STATUS, "Invalid control status event "
				"received.\n");
		return;
	}

	uvc_trace(UVC_TRACE_STATUS, "Control %u/%u %s change len %d.\n",
		data[1], data[3], attrs[data[4]], len);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static void uvc_status_complete(struct urb *urb, struct pt_regs *regs)
#else
static void uvc_status_complete(struct urb *urb)
#endif
{
	struct uvc_device *dev = urb->context;
	int len, ret;

	switch (urb->status) {
	case 0:
		break;

	case -ENOENT:		/* usb_kill_urb() called. */
	case -ECONNRESET:	/* usb_unlink_urb() called. */
	case -ESHUTDOWN:	/* The endpoint is being disabled. */
	case -EPROTO:		/* Device is disconnected (reported by some
				 * host controller). */
		return;

	default:
		uvc_printk(KERN_WARNING, "Non-zero status (%d) in status "
			"completion handler.\n", urb->status);
		return;
	}

	len = urb->actual_length;
	if (len > 0) {
		switch (dev->status[0] & 0x0f) {
		case UVC_STATUS_TYPE_CONTROL:
			uvc_event_control(dev, dev->status, len);
			break;

		case UVC_STATUS_TYPE_STREAMING:
			uvc_event_streaming(dev, dev->status, len);
			break;

		default:
			uvc_printk(KERN_INFO, "unknown event type %u.\n",
				dev->status[0]);
			break;
		}
 	}

	/* Resubmit the URB. */
	urb->interval = dev->int_ep->desc.bInterval;
	if ((ret = usb_submit_urb(urb, GFP_ATOMIC)) < 0) {
		uvc_printk(KERN_ERR, "Failed to resubmit status URB (%d).\n",
			ret);
	}
}

int uvc_status_init(struct uvc_device *dev)
{
	struct usb_host_endpoint *ep = dev->int_ep;
	unsigned int pipe;
	int interval;

	if (ep == NULL)
		return 0;

	dev->int_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (dev->int_urb == NULL)
		return -ENOMEM;

	pipe = usb_rcvintpipe(dev->udev, ep->desc.bEndpointAddress);

	/* For high-speed interrupt endpoints, the bInterval value is used as
	 * an exponent of two. Some developers forgot about it.
	 */
	interval = ep->desc.bInterval;
	if (interval > 16 && dev->udev->speed == USB_SPEED_HIGH &&
	    (dev->quirks & UVC_QUIRK_STATUS_INTERVAL))
		interval = fls(interval) - 1;

	usb_fill_int_urb(dev->int_urb, dev->udev, pipe,
		dev->status, sizeof dev->status, uvc_status_complete,
		dev, interval);

	return usb_submit_urb(dev->int_urb, GFP_KERNEL);
}

void uvc_status_cleanup(struct uvc_device *dev)
{
	usb_kill_urb(dev->int_urb);
	usb_free_urb(dev->int_urb);
}

int uvc_status_suspend(struct uvc_device *dev)
{
	usb_kill_urb(dev->int_urb);
	return 0;
}

int uvc_status_resume(struct uvc_device *dev)
{
	if (dev->int_urb == NULL)
		return 0;

	return usb_submit_urb(dev->int_urb, GFP_KERNEL);
}

