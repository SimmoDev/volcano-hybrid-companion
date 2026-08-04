import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, number, sensor, switch
from esphome.const import (
    CONF_CURRENT_TEMPERATURE,
    CONF_HEATER,
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_STEP,
    CONF_TARGET_TEMPERATURE,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_SWITCH,
    DEVICE_CLASS_TEMPERATURE,
    ICON_FAN,
    ICON_RADIATOR,
    ICON_TIMER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_MINUTE,
    UNIT_SECOND,
)

# Volcano component (ADR-0002, ADR-0003, ADR-0007).
#
# The BLE communication layer: connects via the configured ble_client,
# resolves characteristics by UUID, reads and decodes their values, and
# writes the ones documented in components/volcano/README.md. Each decoded
# value can optionally be exposed as an entity configured below -- a sensor
# for the read-only ones, a number or switch for the writable ones, each of
# those reporting the device's state as well as setting it -- for use on
# e.g. an ESPHome web_server page. See volcano.h / volcano.cpp for the TODO
# markers showing where the remaining protocol coverage and the
# hardware-independent Volcano abstraction layer belong.

CODEOWNERS = ["@SimmoDev"]
DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["sensor", "number", "switch"]

CONF_PUMP = "pump"
CONF_AUTO_SHUTOFF_COUNTDOWN = "auto_shutoff_countdown"
CONF_AUTO_SHUTOFF_DURATION = "auto_shutoff_duration"

volcano_ns = cg.esphome_ns.namespace("volcano")
VolcanoComponent = volcano_ns.class_(
    "VolcanoComponent", cg.Component, ble_client.BLEClientNode
)
VolcanoTargetTemperatureNumber = volcano_ns.class_(
    "VolcanoTargetTemperatureNumber", number.Number, cg.Parented.template(VolcanoComponent)
)
VolcanoAutoShutoffDurationNumber = volcano_ns.class_(
    "VolcanoAutoShutoffDurationNumber", number.Number, cg.Parented.template(VolcanoComponent)
)
VolcanoHeaterSwitch = volcano_ns.class_(
    "VolcanoHeaterSwitch", switch.Switch, cg.Parented.template(VolcanoComponent)
)
VolcanoPumpSwitch = volcano_ns.class_(
    "VolcanoPumpSwitch", switch.Switch, cg.Parented.template(VolcanoComponent)
)

# Defaults for the two writable entities are the ranges the protocol
# documentation records as confirmed accepted -- 40.0-230.0 degC for CMD-001
# and 1-360 minutes for CMD-003. They are configurable so an example can
# deliberately span wider to exercise the component's own refusal of an
# out-of-range value; the component enforces the confirmed range regardless
# of what the entity advertises.
DEFAULT_TARGET_TEMPERATURE_MIN = 40.0
DEFAULT_TARGET_TEMPERATURE_MAX = 230.0
DEFAULT_AUTO_SHUTOFF_DURATION_MIN = 1.0
DEFAULT_AUTO_SHUTOFF_DURATION_MAX = 360.0


def _number_range_schema(default_min, default_max):
    return {
        cv.Optional(CONF_MIN_VALUE, default=default_min): cv.float_,
        cv.Optional(CONF_MAX_VALUE, default=default_max): cv.float_,
        cv.Optional(CONF_STEP, default=1.0): cv.positive_not_null_float,
    }

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
            cv.Optional(CONF_TARGET_TEMPERATURE): number.number_schema(
                VolcanoTargetTemperatureNumber,
                unit_of_measurement=UNIT_CELSIUS,
                device_class=DEVICE_CLASS_TEMPERATURE,
            ).extend(
                _number_range_schema(
                    DEFAULT_TARGET_TEMPERATURE_MIN, DEFAULT_TARGET_TEMPERATURE_MAX
                )
            ),
            cv.Optional(CONF_AUTO_SHUTOFF_DURATION): number.number_schema(
                VolcanoAutoShutoffDurationNumber,
                unit_of_measurement=UNIT_MINUTE,
                icon=ICON_TIMER,
                device_class=DEVICE_CLASS_DURATION,
            ).extend(
                _number_range_schema(
                    DEFAULT_AUTO_SHUTOFF_DURATION_MIN, DEFAULT_AUTO_SHUTOFF_DURATION_MAX
                )
            ),
            cv.Optional(CONF_AUTO_SHUTOFF_COUNTDOWN): sensor.sensor_schema(
                unit_of_measurement=UNIT_SECOND,
                icon=ICON_TIMER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DURATION,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            # DISABLED restore mode, deliberately: any other mode makes the
            # switch apply a remembered state at boot, which would actuate
            # the heater or pump before the status register has reported
            # what the device is actually doing. State comes from the
            # device, never from what was last commanded.
            cv.Optional(CONF_HEATER): switch.switch_schema(
                VolcanoHeaterSwitch,
                icon=ICON_RADIATOR,
                device_class=DEVICE_CLASS_SWITCH,
                default_restore_mode="DISABLED",
            ),
            cv.Optional(CONF_PUMP): switch.switch_schema(
                VolcanoPumpSwitch,
                icon=ICON_FAN,
                device_class=DEVICE_CLASS_SWITCH,
                default_restore_mode="DISABLED",
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
        num = await number.new_number(
            target_temperature_config,
            min_value=target_temperature_config[CONF_MIN_VALUE],
            max_value=target_temperature_config[CONF_MAX_VALUE],
            step=target_temperature_config[CONF_STEP],
        )
        await cg.register_parented(num, var)
        cg.add(var.set_target_temperature_number(num))

    if duration_config := config.get(CONF_AUTO_SHUTOFF_DURATION):
        num = await number.new_number(
            duration_config,
            min_value=duration_config[CONF_MIN_VALUE],
            max_value=duration_config[CONF_MAX_VALUE],
            step=duration_config[CONF_STEP],
        )
        await cg.register_parented(num, var)
        cg.add(var.set_auto_shutoff_duration_number(num))

    if countdown_config := config.get(CONF_AUTO_SHUTOFF_COUNTDOWN):
        sens = await sensor.new_sensor(countdown_config)
        cg.add(var.set_auto_shutoff_countdown_sensor(sens))

    if heater_config := config.get(CONF_HEATER):
        sw = await switch.new_switch(heater_config)
        await cg.register_parented(sw, var)
        cg.add(var.set_heater_switch(sw))

    if pump_config := config.get(CONF_PUMP):
        sw = await switch.new_switch(pump_config)
        await cg.register_parented(sw, var)
        cg.add(var.set_pump_switch(sw))
