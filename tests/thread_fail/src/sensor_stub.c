#include <inttypes.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app_crypto.h"
#include "log_utils.h"
#include "supervisor.h"

LOG_MODULE_REGISTER(sensor_stub, LOG_LEVEL_INF);

#define PLAIN_SAMPLE_TARGET 5U
#define ENCRYPTED_SAMPLE_TARGET 10U

struct sensor_sample_payload {
	int64_t temp_mc;
	int64_t humidity_mpct;
};

static struct k_work_delayable stub_work;
static uint32_t stub_interval_ms;
static uint32_t stub_counter;
static bool stub_running;
static bool stub_hung;
static bool stub_led_ready;

#define LED0_NODE DT_ALIAS(led0)
#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "Unsupported board: led0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static void blink_sensor_led(void)
{
    if (!stub_led_ready) {
        return;
    }

    for (int i = 0; i < 2; i++) {
        if (gpio_pin_set_dt(&led, 1) != 0) {
            stub_led_ready = false;
            return;
        }
        k_msleep(40);
        if (gpio_pin_set_dt(&led, 0) != 0) {
            stub_led_ready = false;
            return;
        }
        k_msleep(40);
    }
}

static void emit_sample(bool encrypted)
{
	int64_t temp_mc = 25000 + (int64_t)stub_counter * 100;
	int64_t humid_mpct = 50000 + (int64_t)stub_counter * 80;

	if (!encrypted) {
		LOG_EVT(INF, "SENSOR", "HTS221_SAMPLE",
			"temp_mc=%" PRId64 ",humidity_mpc=%" PRId64,
			temp_mc, humid_mpct);
		return;
	}

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
	if (enc_rc != 0) {
		LOG_ERR("Stub encryption failed: %d", enc_rc);
		return;
	}

	char iv_hex[APP_CRYPTO_IV_LEN * 2U + 1U];
	char data_hex[sizeof(cipher) * 2U + 1U];
	if (app_crypto_bytes_to_hex(iv, sizeof(iv), iv_hex, sizeof(iv_hex)) != 0 ||
	    app_crypto_bytes_to_hex(cipher, cipher_len, data_hex, sizeof(data_hex)) != 0) {
		LOG_ERR("Stub hex encode failed");
		return;
	}

	LOG_EVT(INF, "SENSOR", "HTS221_SAMPLE", "enc=1,iv=%s,data=%s", iv_hex, data_hex);
}

static void stub_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	bool emit_encrypted = stub_counter >= PLAIN_SAMPLE_TARGET;
	if (stub_counter == PLAIN_SAMPLE_TARGET) {
		LOG_INF("Test stub: switching to encrypted telemetry");
	}

    emit_sample(emit_encrypted);
    blink_sensor_led();
    stub_counter++;

	if (stub_counter >= ENCRYPTED_SAMPLE_TARGET) {
		LOG_WRN("Test stub reached %u samples; simulating hang", stub_counter);
		stub_running = false;
		stub_hung = true;
		return;
	}

	supervisor_notify_led_alive();
	supervisor_notify_system_alive();

	k_work_schedule(&stub_work, K_MSEC(stub_interval_ms));
}

int sensor_hts221_start(bool safe_mode_active)
{
	ARG_UNUSED(safe_mode_active);

    if (stub_running) {
        return 0;
    }

    stub_interval_ms = CONFIG_APP_SENSOR_SAMPLE_INTERVAL_MS;
    if (stub_interval_ms == 0U) {
        stub_interval_ms = 1000U;
    }

    int led_rc = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (led_rc == 0) {
        stub_led_ready = true;
    } else {
        stub_led_ready = false;
        LOG_WRN("LED config failed: %d", led_rc);
    }

    LOG_INF("HTS221 stub active (interval=%ums)", stub_interval_ms);
    stub_counter = 0U;
    stub_running = true;
    stub_hung = false;
    k_work_init_delayable(&stub_work, stub_work_handler);
    k_work_schedule(&stub_work, K_NO_WAIT);
    return 0;
}
