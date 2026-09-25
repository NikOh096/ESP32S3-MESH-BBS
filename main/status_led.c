#include "status_led.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char* TAG = "status_led";
static rmt_channel_handle_t channel;
static rmt_encoder_handle_t encoder;
static rmt_symbol_word_t symbols[25];
static bool available;
static int last_color = -1, last_mode = -1;
static bool unread;
void status_led_unread(bool value) { unread = value; }
static int64_t changed_at;

void status_led_init(void)
{
    /* Addressable RGB indicator; keep the hardware UART pins for USB diagnostics. */
    rmt_tx_channel_config_t cfg = { .gpio_num = CONFIG_MESHBBS_RGB_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,
        .mem_block_symbols = 64,
        .trans_queue_depth = 1 };
    rmt_copy_encoder_config_t enc_cfg = {};
    esp_err_t rc = rmt_new_tx_channel(&cfg, &channel);
    if (rc == ESP_OK)
        rc = rmt_new_copy_encoder(&enc_cfg, &encoder);
    if (rc == ESP_OK)
        rc = rmt_enable(channel);
    available = rc == ESP_OK;
    if (available)
        ESP_LOGI(TAG, "RGB status on GPIO%d", CONFIG_MESHBBS_RGB_GPIO);
    else
        ESP_LOGW(TAG, "LED unavailable: %s; Bluetooth will continue", esp_err_to_name(rc));
}

void status_led_tick(status_led_mode_t mode)
{
    int64_t now = esp_timer_get_time();
    if ((int)mode != last_mode) {
        last_mode = mode;
        changed_at = now;
        ESP_LOGI(TAG, "LED state=%d", mode);
    }
    if (!available)
        return;
    unsigned ms = (unsigned)(now / 1000), phase = ms % 5000;
    /* Gamma-adjusted perceived brightness: blue ~35%, red ~50%. */
    unsigned blue = mode == STATUS_READY ? 25 : 0, red = 0, green = 0;
    if (mode == STATUS_CONNECTING)
        blue = ms % 100 < 50 ? 25 : 0;
    else if (mode == STATUS_PAIRING) {
        unsigned p = ms % 700, ramp = p < 350 ? p : 700 - p;
        blue = 1 + 24 * ramp * ramp / (350 * 350);
    } else {
        /* Normal operation: two quiet green heartbeat taps every five seconds.
         * A separate red tap persists until the owner reads the new comments. */
        if (phase < 90 || (phase >= 190 && phase < 280)) {
            green = 12;
            blue = 0;
        }
        if (unread && phase >= 1000 && phase < 1300) {
            red = 55;
            blue = green = 0;
        }
    }
    unsigned color = (green << 16) | (red << 8) | blue;
    if ((int)color == last_color)
        return;
    /* Keep the previous payload intact if an RMT transfer has not completed. */
    if (rmt_tx_wait_all_done(channel, 0) != ESP_OK)
        return;
    for (unsigned i = 0; i < 24; ++i) {
        bool one = (color >> (23 - i)) & 1;
        symbols[i] = (rmt_symbol_word_t) { .level0 = 1, .duration0 = one ? 9 : 3, .level1 = 0, .duration1 = one ? 3 : 9 };
    }
    /* GRB pixel; 300 us reset for WS2812 revisions. */
    symbols[24] = (rmt_symbol_word_t) { .level0 = 0, .duration0 = 1500, .level1 = 0, .duration1 = 1500 };
    rmt_transmit_config_t tx = { .flags.queue_nonblocking = 1 };
    esp_err_t rc = rmt_transmit(channel, encoder, symbols, sizeof(symbols), &tx);
    if (rc == ESP_OK)
        last_color = (int)color;
    else {
        available = false;
        ESP_LOGW(TAG, "LED transmit failed: %s", esp_err_to_name(rc));
    }
}
