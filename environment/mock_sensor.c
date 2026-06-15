/* mock_sensor.c (Dummy Stub for Agent Workspace) */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>

// --- Dummy I2C Transfer Handler ---
// Simply returns 0 (success) so the agent's local build doesn't crash
static int adxl345_dummy_transfer(const struct emul *target, struct i2c_msg *msgs, int num_msgs, int addr)
{
    return 0; 
}

static const struct i2c_emul_api adxl345_dummy_api = {
    .transfer = adxl345_dummy_transfer
};

// --- Dummy Device Initialization ---
static int adxl345_dummy_device_init(const struct device *dev)
{
    return 0;
}

// Bind the dummy device to the devicetree node so DEVICE_DT_GET passes
DEVICE_DT_DEFINE(DT_NODELABEL(adxl345), adxl345_dummy_device_init, NULL, NULL, NULL, POST_KERNEL, 99, NULL);

static int adxl345_dummy_emul_init(const struct emul *target, const struct device *parent) 
{ 
    return 0; 
}

// Attach the dummy emulator to the I2C bus
#define ADXL345_EMUL_DEFINE(inst) \
    EMUL_DT_DEFINE(DT_NODELABEL(adxl345), adxl345_dummy_emul_init, NULL, NULL, &adxl345_dummy_api, NULL)

ADXL345_EMUL_DEFINE(0);