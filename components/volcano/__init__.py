import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, binary_sensor, sensor
from esphome.const import (
    CONF_CURRENT_TEMPERATURE,
    CONF_HEATER,
    CONF_ID,
    CONF_TARGET_TEMPERATURE,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_HEAT,
    DEVICE_CLASS_RUNNING,
    DEVICE_CLASS_TEMPERATURE,
    ICON_TIMER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_SECOND,
)

# Volcano component (ADR-0002, ADR-0003, ADR-0007).
#
# The BLE communication layer: connects via the configured ble_client,
# resolves characteristics by UUID, reads and decodes their values, and
# writes the ones documented in components/volcano/README.md. Each decoded
# value can optionally be published to a sensor/binary_sensor configured
# below, for use on e.g. an ESPHome web_server page -- see volcano.h /
# volcano.cpp for the TODO markers showing where the remaining protocol
# coverage and the hardware-independent Volcano abstraction layer belong.

CODEOWNERS = ["@SimmoDev"]
DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["sensor", "binary_sensor"]

CONF_PUMP = "pump"
CONF_AUTO_SHUTOFF_COUNTDOWN = "auto_shutoff_countdown"

volcano_ns = cg.esphome_ns.namespace("volcano")
VolcanoComponent = volcano_ns.class_(
    "VolcanoComponent", cg.Component, ble_client.BLEClientNode
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(VolcanoComponent),
            cv.Optional(CONF_CURRENT_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TARGET_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_AUTO_SHUTOFF_COUNTDOWN): sensor.sensor_schema(
                unit_of_measurement=UNIT_SECOND,
                icon=ICON_TIMER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DURATION,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HEATER): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_HEAT,
            ),
            cv.Optional(CONF_PUMP): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_RUNNING,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    if current_temperature_config := config.get(CONF_CURRENT_TEMPERATURE):
        sens = await sensor.new_sensor(current_temperature_config)
        cg.add(var.set_current_temperature_sensor(sens))

    if target_temperature_config := config.get(CONF_TARGET_TEMPERATURE):
        sens = await sensor.new_sensor(target_temperature_config)
        cg.add(var.set_target_temperature_sensor(sens))

    if countdown_config := config.get(CONF_AUTO_SHUTOFF_COUNTDOWN):
        sens = await sensor.new_sensor(countdown_config)
        cg.add(var.set_auto_shutoff_countdown_sensor(sens))

    if heater_config := config.get(CONF_HEATER):
        bsens = await binary_sensor.new_binary_sensor(heater_config)
        cg.add(var.set_heater_binary_sensor(bsens))

    if pump_config := config.get(CONF_PUMP):
        bsens = await binary_sensor.new_binary_sensor(pump_config)
        cg.add(var.set_pump_binary_sensor(bsens))
