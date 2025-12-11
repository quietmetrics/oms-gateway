#include "app/led.h"

#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "led";
static const int LED_DUTY_MAX = 255;
static const int LED_FADE_STEP = 8;
static const int LED_FADE_DELAY_MS = 30;
static const int LED_BLINK_SLOW_MS = 800;
static const int LED_BLINK_FAST_MS = 150;

typedef struct
{
    status_led_pattern_t base;
    status_led_pattern_t override;
    bool override_active;
    uint32_t generation;
    gpio_num_t gpio;
    bool inverted;
} status_led_state_t;

static status_led_state_t s_state = {
    .base = STATUS_LED_PATTERN_OFF,
    .override = STATUS_LED_PATTERN_OFF,
    .override_active = false,
    .generation = 0,
    .gpio = STATUS_LED_GPIO,
};

static SemaphoreHandle_t s_lock = NULL;
static TaskHandle_t s_task = NULL;

static void led_lock(void)
{
    if (s_lock)
    {
        xSemaphoreTake(s_lock, portMAX_DELAY);
    }
}

static void led_unlock(void)
{
    if (s_lock)
    {
        xSemaphoreGive(s_lock);
    }
}

static uint32_t led_generation(void)
{
    uint32_t gen = 0;
    led_lock();
    gen = s_state.generation;
    led_unlock();
    return gen;
}

static void led_apply_duty(int duty)
{
    if (duty < 0)
    {
        duty = 0;
    }
    if (duty > LED_DUTY_MAX)
    {
        duty = LED_DUTY_MAX;
    }
    int hw_duty = duty;
    led_lock();
    if (s_state.inverted)
    {
        hw_duty = LED_DUTY_MAX - duty;
    }
    led_unlock();

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, hw_duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void led_finish_one_shot(uint32_t gen_at_start)
{
    led_lock();
    if (s_state.generation == gen_at_start && s_state.override_active)
    {
        s_state.override_active = false;
        s_state.generation++;
    }
    led_unlock();
}

static status_led_pattern_t led_active_pattern(uint32_t *gen_out, bool *is_override)
{
    status_led_pattern_t pattern = STATUS_LED_PATTERN_OFF;
    led_lock();
    if (is_override)
    {
        *is_override = s_state.override_active;
    }
    pattern = s_state.override_active ? s_state.override : s_state.base;
    if (gen_out)
    {
        *gen_out = s_state.generation;
    }
    led_unlock();
    return pattern;
}

static bool led_generation_changed(uint32_t gen)
{
    return led_generation() != gen;
}

static void led_blink_loop(int on_ms, int off_ms, uint32_t gen)
{
    while (!led_generation_changed(gen))
    {
        led_apply_duty(LED_DUTY_MAX);
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        if (led_generation_changed(gen))
        {
            break;
        }
        led_apply_duty(0);
        vTaskDelay(pdMS_TO_TICKS(off_ms));
    }
}

static void led_fade_loop(uint32_t gen)
{
    while (!led_generation_changed(gen))
    {
        for (int duty = 0; duty <= LED_DUTY_MAX && !led_generation_changed(gen); duty += LED_FADE_STEP)
        {
            led_apply_duty(duty);
            vTaskDelay(pdMS_TO_TICKS(LED_FADE_DELAY_MS));
        }
        for (int duty = LED_DUTY_MAX; duty >= 0 && !led_generation_changed(gen); duty -= LED_FADE_STEP)
        {
            led_apply_duty(duty);
            vTaskDelay(pdMS_TO_TICKS(LED_FADE_DELAY_MS));
        }
    }
}

static void led_double_blink(uint32_t gen)
{
    const int pulse_ms = 120;
    const int gap_ms = 100;

    for (int i = 0; i < 2; i++)
    {
        if (led_generation_changed(gen))
        {
            return;
        }
        led_apply_duty(LED_DUTY_MAX);
        vTaskDelay(pdMS_TO_TICKS(pulse_ms));
        led_apply_duty(0);
        vTaskDelay(pdMS_TO_TICKS(gap_ms));
    }
    led_finish_one_shot(gen);
}

static void led_pulse_once(uint32_t gen)
{
    const int pulse_ms = 120;
    led_apply_duty(LED_DUTY_MAX);
    vTaskDelay(pdMS_TO_TICKS(pulse_ms));
    led_apply_duty(0);
    led_finish_one_shot(gen);
}

static void led_task(void *arg)
{
    (void)arg;
    while (true)
    {
        uint32_t gen = 0;
        bool is_override = false;
        status_led_pattern_t pattern = led_active_pattern(&gen, &is_override);

        switch (pattern)
        {
        case STATUS_LED_PATTERN_OFF:
            led_apply_duty(0);
            vTaskDelay(pdMS_TO_TICKS(250));
            break;
        case STATUS_LED_PATTERN_ON:
            led_apply_duty(LED_DUTY_MAX);
            vTaskDelay(pdMS_TO_TICKS(250));
            break;
        case STATUS_LED_PATTERN_BLINK_SLOW:
            led_blink_loop(LED_BLINK_SLOW_MS, LED_BLINK_SLOW_MS, gen);
            break;
        case STATUS_LED_PATTERN_BLINK_FAST:
            led_blink_loop(LED_BLINK_FAST_MS, LED_BLINK_FAST_MS, gen);
            break;
        case STATUS_LED_PATTERN_FADE_SLOW:
            led_fade_loop(gen);
            break;
        case STATUS_LED_PATTERN_DOUBLE_BLINK:
            led_double_blink(gen);
            break;
        case STATUS_LED_PATTERN_PULSE_SHORT:
            led_pulse_once(gen);
            break;
        default:
            led_apply_duty(0);
            vTaskDelay(pdMS_TO_TICKS(200));
            break;
        }
    }
}

esp_err_t status_led_init(gpio_num_t gpio, bool inverted)
{
    if (s_task != NULL)
    {
        return ESP_OK;
    }

    s_lock = xSemaphoreCreateMutex();
    if (!s_lock)
    {
        return ESP_ERR_NO_MEM;
    }

    s_state.gpio = gpio;
    s_state.inverted = inverted;

    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t channel = {
        .gpio_num = gpio,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));

    BaseType_t task_ok = xTaskCreate(led_task, "status_led", 2048, NULL, 4, &s_task);
    if (task_ok != pdPASS)
    {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "status LED initialized on GPIO %d", gpio);
    return ESP_OK;
}

void status_led_set_base(status_led_pattern_t pattern)
{
    led_lock();
    s_state.base = pattern;
    s_state.generation++;
    led_unlock();
}

void status_led_trigger_once(status_led_pattern_t pattern)
{
    led_lock();
    s_state.override = pattern;
    s_state.override_active = true;
    s_state.generation++;
    led_unlock();
}
