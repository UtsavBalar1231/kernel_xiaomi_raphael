/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/skbuff.h>
#include <linux/sizes.h>
#include <linux/spinlock.h>
#include <net/sock.h>
#include <uapi/linux/sched/types.h>

#include <soc/qcom/qrtr_ethernet.h>
#include "qrtr.h"

struct qrtr_ethernet_dl_buf {
	void *buf;
	struct mutex buf_lock;			/* lock to protect buf */
	size_t saved;
	size_t needed;
	size_t pkt_len;
};

struct qrtr_ethernet_dev {
	struct qrtr_endpoint ep;
	struct device *dev;
	spinlock_t ul_lock;			/* lock to protect ul_pkts */
	struct list_head ul_pkts;
	atomic_t in_reset;
	u32 net_id;
	bool rt;
	struct qrtr_ethernet_cb_info *cb_info;

	struct qrtr_ethernet_dl_buf dlbuf;
};

/* Buffer to parse packets from ethernet adaption layer to qrtr */
#define MAX_BUFSIZE SZ_64K

struct qrtr_ethernet_dev *qrtr_ethernet_device_endpoint;

static void qrtr_ethernet_link_up(void)
{
	struct qrtr_ethernet_dev *qdev = qrtr_ethernet_device_endpoint;
	int rc;

	if (!qdev) {
		pr_err("%s: qrtr ep dev ptr not found\n", __func__);
		return;
	}

	atomic_set(&qdev->in_reset, 0);

	mutex_lock(&qdev->dlbuf.buf_lock);
	memset(qdev->dlbuf.buf, 0, MAX_BUFSIZE);
	qdev->dlbuf.saved = 0;
	qdev->dlbuf.needed = 0;
	qdev->dlbuf.pkt_len = 0;
	mutex_unlock(&qdev->dlbuf.buf_lock);

	rc = qrtr_endpoint_register(&qdev->ep, qdev->net_id, qdev->rt);
	if (rc) {
		dev_err(qdev->dev, "%s: EP register fail: %d\n", __func__, rc);
		return;
	}
}

static void qrtr_ethernet_link_down(void)
{
	struct qrtr_ethernet_dev *qdev = qrtr_ethernet_device_endpoint;

	atomic_inc(&qdev->in_reset);

	mutex_lock(&qdev->dlbuf.buf_lock);
	memset(qdev->dlbuf.buf, 0, MAX_BUFSIZE);
	qdev->dlbuf.saved = 0;
	qdev->dlbuf.needed = 0;
	qdev->dlbuf.pkt_len = 0;
	mutex_unlock(&qdev->dlbuf.buf_lock);

	qrtr_endpoint_unregister(&qdev->ep);
}

/**
 * qcom_ethernet_qrtr_status_cb() - Notify qrtr-ethernet of status changes
 * @event:	Event type
 *
 * NETDEV_UP is posted when ethernet link is setup and NETDEV_DOWN is posted
 * when the ethernet link goes down by the transport layer.
 *
 * Return: None
 */
void qcom_ethernet_qrtr_status_cb(unsigned int event)
{
	if (event == NETDEV_UP)
		qrtr_ethernet_link_up();
	else if (event == NETDEV_DOWN)
		qrtr_ethernet_link_down();
	else
		pr_err("%s: Unknown state: %d\n", __func__, event);
}
EXPORT_SYMBOL(qcom_ethernet_qrtr_status_cb);

static size_t set_cp_size(size_t len)
{
	return ((len > MAX_BUFSIZE) ? MAX_BUFSIZE : len);
}

/**
 * qcom_ethernet_qrtr_dl_cb() - Post incoming stream to qrtr
 * @eth_res:	Pointer that holds the incoming data stream
 *
 * Transport layer posts the data from external AP to qrtr.
 * This is then posted to the qrtr endpoint.
 *
 * Return: None
 */
void qcom_ethernet_qrtr_dl_cb(struct eth_adapt_result *eth_res)
{
	struct qrtr_ethernet_dev *qdev = qrtr_ethernet_device_endpoint;
	struct qrtr_ethernet_dl_buf *dlbuf;
	size_t pkt_len, len;
	void *src;
	int rc;

	if (!eth_res)
		return;

	if (!qdev) {
		pr_err("%s: qrtr ep dev ptr not found\n", __func__);
		return;
	}

	dlbuf = &qdev->dlbuf;

	src = eth_res->buf_addr;
	if (!src) {
		dev_err(qdev->dev, "%s: Invalid input buffer\n", __func__);
		return;
	}

	len = eth_res->bytes_xferd;
	if (len > MAX_BUFSIZE) {
		dev_err(qdev->dev, "%s: Pkt len, 0x%x > MAX_BUFSIZE\n",
			__func__, len);
		return;
	}

	if (atomic_read(&qdev->in_reset) > 0) {
		dev_err(qdev->dev, "%s: link in reset\n", __func__);
		return;
	}

	mutex_lock(&dlbuf->buf_lock);
	while (len > 0) {
		if (dlbuf->needed > 0) {
			pkt_len = dlbuf->pkt_len;
			if (len >= dlbuf->needed) {
				dlbuf->needed = set_cp_size(dlbuf->needed);
				memcpy((dlbuf->buf + dlbuf->saved),
				       src, dlbuf->needed);
				rc = qrtr_endpoint_post(&qdev->ep, dlbuf->buf,
							pkt_len);
				if (rc == -EINVAL) {
					dev_err(qdev->dev,
						"Invalid qrtr packet\n");
					goto exit;
				}
				memset(dlbuf->buf, 0, MAX_BUFSIZE);
				len = len - dlbuf->needed;
				src = src + dlbuf->needed;
				dlbuf->needed = 0;
				dlbuf->pkt_len = 0;
			} else {
				/* Partial packet */
				len = set_cp_size(len);
				memcpy(dlbuf->buf + dlbuf->saved, src, len);
				dlbuf->saved = dlbuf->saved + len;
				dlbuf->needed = dlbuf->needed - len;
				break;
			}
		} else {
			pkt_len = qrtr_peek_pkt_size(src);
			if ((int)pkt_len < 0) {
				dev_err(qdev->dev,
					"Invalid pkt_len %zu\n", pkt_len);
				break;
			}

			if ((int)pkt_len == 0) {
				dlbuf->needed = 0;
				dlbuf->pkt_len = 0;
				break;
			}

			if (pkt_len > MAX_BUFSIZE) {
				dev_err(qdev->dev,
					"Unsupported pkt_len %zu\n", pkt_len);
				break;
			}

			if (pkt_len > len) {
				/* Partial packet */
				dlbuf->needed = pkt_len - len;
				dlbuf->pkt_len = pkt_len;
				dlbuf->saved = len;
				dlbuf->saved = set_cp_size(dlbuf->saved);
				memcpy(dlbuf->buf, src, dlbuf->saved);
				break;
			}
			pkt_len = set_cp_size(pkt_len);
			memcpy(dlbuf->buf, src, pkt_len);
			rc = qrtr_endpoint_post(&qdev->ep, dlbuf->buf, pkt_len);
			if (rc == -EINVAL) {
				dev_err(qdev->dev, "Invalid qrtr packet\n");
				goto exit;
			}
			memset(dlbuf->buf, 0, MAX_BUFSIZE);
			len = len - pkt_len;
			src = src + pkt_len;
			dlbuf->needed = 0;
			dlbuf->pkt_len = 0;
		}
	}
exit:
	mutex_unlock(&dlbuf->buf_lock);
}
EXPORT_SYMBOL(qcom_ethernet_qrtr_dl_cb);

/* from qrtr to ethernet adaption layer */
static int qcom_ethernet_qrtr_send(struct qrtr_endpoint *ep,
				   struct sk_buff *skb)
{
	struct qrtr_ethernet_dev *qdev = container_of(ep,
						      struct qrtr_ethernet_dev,
						      ep);
	int rc;

	rc = skb_linearize(skb);
	if (rc) {
		kfree_skb(skb);
		dev_err(qdev->dev, "%s: skb_linearize failed: %d\n",
			__func__, rc);
		return rc;
	}

	if (atomic_read(&qdev->in_reset) > 0) {
		dev_err(qdev->dev, "%s: link in reset\n", __func__);
		return -ECONNRESET;
	}

	rc = qdev->cb_info->eth_send(skb);
	if (rc) {
		dev_err(qdev->dev, "%s: eth_send failed: %d\n",
			__func__, rc);
	}

	consume_skb(skb);

	return 0;
}

/**
 * qcom_ethernet_init_cb() - Pass callback pointer to qrtr-ethernet
 * @cbinfo:	qrtr_ethernet_cb_info pointer providing the callback
 *          function for outgoing packets.
 *
 * Pass in a pointer to be used by this module to communicate with
 * eth-adaption layer. This needs to be called after the ethernet
 * link is up.
 *
 * Return: None
 */
void qcom_ethernet_init_cb(struct qrtr_ethernet_cb_info *cbinfo)
{
	struct qrtr_ethernet_dev *qdev = qrtr_ethernet_device_endpoint;

	if (!qdev) {
		pr_err("%s: qrtr ep dev ptr not found\n", __func__);
		return;
	}

	qdev->cb_info = cbinfo;

	qrtr_ethernet_link_up();
}
EXPORT_SYMBOL(qcom_ethernet_init_cb);

static int qcom_ethernet_qrtr_probe(struct platform_device *pdev)
{
	struct sched_param param = {.sched_priority = 1};
	struct device_node *node = pdev->dev.of_node;
	struct qrtr_ethernet_dev *qdev;
	int rc;

	qdev = devm_kzalloc(&pdev->dev, sizeof(*qdev), GFP_KERNEL);
	if (!qdev)
		return -ENOMEM;

	qdev->dlbuf.buf = devm_kzalloc(&pdev->dev, MAX_BUFSIZE, GFP_KERNEL);
	if (!qdev->dlbuf.buf)
		return -ENOMEM;

	mutex_init(&qdev->dlbuf.buf_lock);
	qdev->dlbuf.saved = 0;
	qdev->dlbuf.needed = 0;
	qdev->dlbuf.pkt_len = 0;

	qdev->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, qdev);

	qdev->ep.xmit = qcom_ethernet_qrtr_send;
	atomic_set(&qdev->in_reset, 0);

	INIT_LIST_HEAD(&qdev->ul_pkts);
	spin_lock_init(&qdev->ul_lock);

	rc = of_property_read_u32(node, "qcom,net-id", &qdev->net_id);
	if (rc < 0)
		qdev->net_id = QRTR_EP_NET_ID_AUTO;

	qdev->rt = of_property_read_bool(node, "qcom,low-latency");

	qrtr_ethernet_device_endpoint = qdev;

	return 0;
}

static int qcom_ethernet_qrtr_remove(struct platform_device *pdev)
{
	struct qrtr_ethernet_dev *qdev = dev_get_drvdata(&pdev->dev);

	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}

static const struct of_device_id qcom_qrtr_ethernet_match[] = {
	{ .compatible = "qcom,qrtr-ethernet-dev" },
	{}
};

static struct platform_driver qrtr_ethernet_dev_driver = {
	.probe = qcom_ethernet_qrtr_probe,
	.remove = qcom_ethernet_qrtr_remove,
	.driver = {
		.name = "qcom_ethernet_qrtr",
		.of_match_table = qcom_qrtr_ethernet_match,
	},
};
module_platform_driver(qrtr_ethernet_dev_driver);

MODULE_DESCRIPTION("QTI IPC-Router Ethernet interface driver");
MODULE_LICENSE("GPL v2");
