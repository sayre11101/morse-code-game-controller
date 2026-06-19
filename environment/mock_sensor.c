#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>

/*
 * Agent-visible dummy mock.
 * Verifier steps overwrite /app/mock_sensor.c with private test mocks.
 */

static int adxl345_dummy_transfer(const struct emul *target,
                                  struct i2c_msg *msgs,
                                  int num_msgs,
                                  int addr)
{
    ARG_UNUSED(target);
    ARG_UNUSED(msgs);
    ARG_UNUSED(num_msgs);
    ARG_UNUSED(addr);
    return 0;
}

static const struct i2c_emul_api adxl345_dummy_api = {
    .transfer = adxl345_dummy_transfer,
};

static int adxl345_dummy_device_init(const struct device *dev)
{
    ARG_UNUSED(dev);
    return 0;
}

DEVICE_DT_DEFINE(DT_NODELABEL(adxl345), adxl345_dummy_device_init, NULL, NULL, NULL, POST_KERNEL, 99, NULL);

static int adxl345_dummy_emul_init(const struct emul *target, const struct device *parent)
{
    ARG_UNUSED(target);
    ARG_UNUSED(parent);
    return 0;
}

#define ADXL345_EMUL_DEFINE(inst) \
    EMUL_DT_DEFINE(DT_NODELABEL(adxl345), adxl345_dummy_emul_init, NULL, NULL, &adxl345_dummy_api, NULL)

ADXL345_EMUL_DEFINE(0);
