/*
 * Copyright (c) 2022 Libre Solar Technologies GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/lora.h>

#include <string.h>

/* change this to any other UART peripheral if desired */
#define UART_DEVICE_NODE			DT_CHOSEN(zephyr_shell_uart)

#define DEFAULT_RADIO_NODE			DT_ALIAS(lora0)

#define MSG_SIZE 32

/*
#if (DT_NUM_INST_STATUS_OKAY(zephyr_shell_uart) == 0)
#error	"uart is not defined in DTS"
#endif
*/

#define MAX_DATA_LEN 10
char data[MAX_DATA_LEN] = {'h', 'e', 'l', 'l', 'o', 'w', 'o', 'r', 'l', 'd'};

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);

static const struct device *const lora_dev = DEVICE_DT_GET(DEFAULT_RADIO_NODE);
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

/* receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE];
static int rx_buf_pos;

/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void serial_cb(const struct device *dev, void *user_data)
{
	uint8_t c;

	if (!uart_irq_update(uart_dev)) {
		return;
	}

	if (!uart_irq_rx_ready(uart_dev)) {
		return;
	}

	/* read until FIFO empty */
	while (uart_fifo_read(uart_dev, &c, 1) == 1) {
		if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {
			/* terminate string */
			rx_buf[rx_buf_pos] = '\0';

			/* if queue is full, message is silently dropped */
			k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);

			/* reset the buffer (it was copied to the msgq) */
			rx_buf_pos = 0;
		} else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
			rx_buf[rx_buf_pos++] = c;
		}
		/* else: characters beyond buffer size are dropped */
	}
}

/*
 * Print a null-terminated string character by character to the UART interface
 */
void print_uart(char *buf)
{
	int msg_len = strlen(buf);

	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(uart_dev, buf[i]);
	}
}

int main(void)
{
	char tx_buf[MSG_SIZE];
	struct lora_modem_config config;

	if (!device_is_ready(uart_dev)) {
		printk("UART device not found!");
		return 0;
	}

	if (!device_is_ready(lora_dev)) {
		printk("%s Device not ready", lora_dev->name);
		return 0;
	}

	/* configure interrupt and callback to receive data */
	int ret = uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);

	if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk("Interrupt-driven UART API support not enabled\n");
		} else if (ret == -ENOSYS) {
			printk("UART device does not support interrupt-driven API\n");
		} else {
			printk("Error setting UART callback: %d\n", ret);
		}
		return 0;
	}
	uart_irq_rx_enable(uart_dev);

	config.frequency = 915000000;
	config.bandwidth = BW_125_KHZ;
	config.datarate = SF_10;
	config.preamble_len = 8;
	config.coding_rate = CR_4_5;
	config.iq_inverted = false;
	config.public_network = false;
	config.tx_power = 4;
	config.tx = true;

	ret = lora_config(lora_dev, &config);
	if (ret < 0) {
		printk("LoRa config failed");
		return 0;
	}

	print_uart("Aug-8th Hello! I'm your echo bot.\r\n");
	print_uart("Tell me something and press enter:\r\n");

	/* indefinitely wait for input from the user */
	while (true) {
		//ret = k_msgq_get(&uart_msgq, &tx_buf, K_FOREVER);
		ret = k_msgq_get(&uart_msgq, &tx_buf, K_MSEC(1000));
		if (ret == 0) {
			print_uart("Echo: ");
			print_uart(tx_buf);
			print_uart("\r\n");
		} else if (ret == -ENOMSG) {
			// returned without waiting.
			print_uart("ENOMSG didnt wait\r\n");
		} else if (ret == -EAGAIN) {
			//Waiting period timed out. 
			print_uart("EAGAIN timed out\r\n");
			ret = lora_send(lora_dev, data, MAX_DATA_LEN);
			if (ret < 0) {
				printk("LoRa send failed");
				return 0;
			}
		}

	}
	return 0;
}
