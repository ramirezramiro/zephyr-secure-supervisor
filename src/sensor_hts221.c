#include "sensor_hts221.h"

#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app_crypto.h"
#include "log_utils.h"
#include "supervisor.h"

LOG_MODULE_REGISTER(sensor_hts221, LOG_LEVEL_INF);

#if !DT_HAS_COMPAT_STATUS_OKAY(st_hts221)
#error "No HTS221 instance found in the devicetree. Enable the X-NUCLEO-IKS01A2 shield."
#endif

static const struct device *const hts221_dev = DEVICE_DT_GET_ONE(st_hts221);
#define LED0_NODE DT_ALIAS(led0)
#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "Unsupported board: led0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static bool sensor_led_ready;
static uint32_t sensor_poll_interval_ms;
static struct k_work_delayable sensor_work;
static uint32_t sample_counter;

struct sensor_sample_payload {
	int64_t temp_mc;
	int64_t humidity_mpct;
};

static void blink_sensor_led(void)
{
	if (!sensor_led_ready) {
		return;
	}

	for (int i = 0; i < 2; i++) {
		if (gpio_pin_set_dt(&led, 1) != 0) {
			sensor_led_ready = false;
			return;
		}
		k_msleep(40);
		if (gpio_pin_set_dt(&led, 0) != 0) {
			sensor_led_ready = false;
			return;
		}
		k_msleep(40);
	}
}

static void schedule_next_poll(void)
{
	k_work_schedule_for_queue(&k_sys_work_q, &sensor_work,
				  K_MSEC(sensor_poll_interval_ms));
}

static void sensor_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	int fetch_status = sensor_sample_fetch(hts221_dev);
	if (fetch_status == 0) {
		struct sensor_value temp_reading = {0};
		struct sensor_value humidity_reading = {0};

		fetch_status = sensor_channel_get(hts221_dev, SENSOR_CHAN_AMBIENT_TEMP,
						  &temp_reading);
		if (fetch_status == 0) {
			fetch_status = sensor_channel_get(hts221_dev,
							  SENSOR_CHAN_HUMIDITY,
							  &humidity_reading);
		}

		if (fetch_status == 0) {
			int64_t temp_mc = sensor_value_to_milli(&temp_reading);
			int64_t humid_mpct = sensor_value_to_milli(&humidity_reading);
			bool use_encryption = app_crypto_is_enabled() && sample_counter >= 10U;
			bool logged = false;

			if (sample_counter == 10U) {
				LOG_INF("Enabling AES telemetry after initial plaintext samples");
			}

			if (use_encryption) {
				struct sensor_sample_payload payload = {
					.temp_mc = temp_mc,
					.humidity_mpct = humid_mpct,
				};
				uint8_t cipher[sizeof(payload)];
				uint8_t iv[APP_CRYPTO_IV_LEN];
				size_t cipher_len = 0U;
				int enc_rc = app_crypto_encrypt_buffer((const uint8_t *)&payload,
								       sizeof(payload),
								       cipher, sizeof(cipher),
								       &cipher_len, iv);
				if (enc_rc == 0) {
					char iv_hex[APP_CRYPTO_IV_LEN * 2U + 1U];
					char data_hex[sizeof(cipher) * 2U + 1U];
					if (app_crypto_bytes_to_hex(iv, sizeof(iv),
								     iv_hex, sizeof(iv_hex)) == 0 &&
					    app_crypto_bytes_to_hex(cipher, cipher_len,
								     data_hex, sizeof(data_hex)) == 0) {
						LOG_EVT(INF, "SENSOR", "HTS221_SAMPLE",
							"enc=1,iv=%s,data=%s", iv_hex, data_hex);
						logged = true;
					} else {
						LOG_ERR("Sensor payload hex encoding failed");
					}
				} else {
					LOG_ERR("Sensor payload encryption failed: %d", enc_rc);
				}
			}

			if (!logged) {
				LOG_EVT(INF, "SENSOR", "HTS221_SAMPLE",
					"temp_mc=%" PRId64 ",humidity_mpc=%" PRId64,
					temp_mc, humid_mpct);
			}

			sample_counter++;

			blink_sensor_led();
		} else {
			LOG_EVT(WRN, "SENSOR", "HTS221_CHAN_FAIL",
				"rc=%d", fetch_status);
		}
	} else {
		LOG_EVT(WRN, "SENSOR", "HTS221_FETCH_FAIL", "rc=%d", fetch_status);
	}

	supervisor_notify_led_alive();
	supervisor_notify_system_alive();

	schedule_next_poll();
}

int sensor_hts221_start(bool safe_mode_active)
{
	if (!device_is_ready(hts221_dev)) {
		LOG_ERR("HTS221 device not ready");
		return -ENODEV;
	}

	sensor_poll_interval_ms = safe_mode_active ?
		CONFIG_APP_SENSOR_SAFE_MODE_INTERVAL_MS :
		CONFIG_APP_SENSOR_SAMPLE_INTERVAL_MS;
	if (sensor_poll_interval_ms == 0U) {
		sensor_poll_interval_ms = 1000U;
	}

	int led_rc = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (led_rc == 0) {
		sensor_led_ready = true;
	} else {
		sensor_led_ready = false;
		LOG_WRN("LED config failed: %d", led_rc);
	}

	LOG_EVT(INF, "SENSOR", "HTS221_READY",
		"interval_ms=%u,fallback=%s,led=%s",
		sensor_poll_interval_ms, safe_mode_active ? "yes" : "no",
		sensor_led_ready ? "on" : "off");

	k_work_init_delayable(&sensor_work, sensor_work_handler);
	schedule_next_poll();
	sample_counter = 0U;
	return 0;
}
