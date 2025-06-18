/*
 * 简单距离过滤 input-processor
 * 仅当指定时间内累计移动距离超过阈值时，才允许事件继续传递，否则吞掉事件
 */

#define DT_DRV_COMPAT zmk_input_processor_distance_filter_layer

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <drivers/input_processor.h>
#include <zmk/events/mouse_move_state_changed.h>

struct distance_filter_layer_config {
    uint16_t timeout_ms;
    uint16_t min_move_threshold;
};

struct distance_filter_layer_state {
    int64_t start_timestamp;
    int32_t accumulated_move;
    bool passed;
};

struct distance_filter_layer_data {
    struct distance_filter_layer_state state;
};

static int distance_filter_layer_handle_event(const struct device *dev, struct input_event *event,
                                             uint32_t param1, uint32_t param2,
                                             struct zmk_input_processor_state *state) {
    struct distance_filter_layer_data *data = dev->data;
    const struct distance_filter_layer_config *cfg = dev->config;

    // 只处理鼠标相对移动事件
    if (event->type != INPUT_EV_REL || (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    int64_t now = k_uptime_get();

    // 初始化
    if (data->state.start_timestamp == 0 || now - data->state.start_timestamp > cfg->timeout_ms) {
        data->state.start_timestamp = now;
        data->state.accumulated_move = 0;
        data->state.passed = false;
    }

    // 累加本次移动距离
    data->state.accumulated_move += abs(event->value);

    // 判断是否超过阈值
    if (!data->state.passed && data->state.accumulated_move >= cfg->min_move_threshold) {
        data->state.passed = true;
    }

    // 如果未通过阈值，吞掉事件
    if (!data->state.passed) {
        return ZMK_INPUT_PROC_STOP;
    }

    // 通过阈值，允许后续处理器处理
    return ZMK_INPUT_PROC_CONTINUE;
}

static int distance_filter_layer_init(const struct device *dev) {
    struct distance_filter_layer_data *data = dev->data;
    data->state.start_timestamp = 0;
    data->state.accumulated_move = 0;
    data->state.passed = false;
    return 0;
}

static const struct zmk_input_processor_driver_api distance_filter_layer_driver_api = {
    .handle_event = distance_filter_layer_handle_event,
};

#define DISTANCE_FILTER_LAYER_INST(n) \
    static struct distance_filter_layer_data distance_filter_layer_data_##n = {}; \
    static const struct distance_filter_layer_config distance_filter_layer_config_##n = { \
        .timeout_ms = DT_INST_PROP_OR(n, timeout_ms, 400), \
        .min_move_threshold = DT_INST_PROP_OR(n, min_move_threshold, 10), \
    }; \
    DEVICE_DT_INST_DEFINE(n, distance_filter_layer_init, NULL, \
        &distance_filter_layer_data_##n, \
        &distance_filter_layer_config_##n, POST_KERNEL, \
        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &distance_filter_layer_driver_api);

DT_INST_FOREACH_STATUS_OKAY(DISTANCE_FILTER_LAYER_INST)