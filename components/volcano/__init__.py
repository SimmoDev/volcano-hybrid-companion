import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, binary_sensor, number, sensor, switch, text_sensor
from esphome.const import (
    CONF_CURRENT_TEMPERATURE,
    CONF_HEATER,
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_STEP,
    CONF_TARGET_TEMPERATURE,
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_SWITCH,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_BLUETOOTH,
    ICON_BRIGHTNESS_5,
    ICON_CHIP,
    ICON_FAN,
    ICON_POWER,
    ICON_RADIATOR,
    ICON_THERMOMETER,
    ICON_TIMER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_HOUR,
    UNIT_MINUTE,
    UNIT_PERCENT,
    UNIT_SECOND,
)

# Volcano component (ADR-0002, ADR-0003, ADR-0007, ADR-0009).
#
# Three parts, per ADR-0009: VolcanoBleClient (the BLE communication layer:
# connects via the configured ble_client, resolves characteristics by UUID,
# reads/writes and encodes/decodes them), VolcanoDevice (the Volcano
# abstraction layer: the authoritative state model, connection state, and
# requested-versus-confirmed write handling), and VolcanoComponent (this
# component: ESPHome integration only, consuming VolcanoDevice like any
# other control interface). Each value can optionally be exposed as an
# entity configured below -- a sensor for the read-only ones, a number or
# switch for the writable ones, each reporting the device's state as well
# as setting it -- for use on e.g. an ESPHome web_server page. See
# components/volcano/README.md for the full picture.

CODEOWNERS = ["@SimmoDev"]
DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["binary_sensor", "sensor", "number", "switch", "text_sensor"]

CONF_PUMP = "pump"
CONF_CONNECTED = "connected"
CONF_AUTO_SHUTOFF_COUNTDOWN = "auto_shutoff_countdown"
CONF_AUTO_SHUTOFF_DURATION = "auto_shutoff_duration"
CONF_FIRMWARE_VERSION = "firmware_version"
CONF_BLE_FIRMWARE_VERSION = "ble_firmware_version"
CONF_SERIAL_NUMBER = "serial_number"
CONF_POWER_SUPPLY = "power_supply"
CONF_PRODUCT_LINE = "product_line"
CONF_VIBRATION = "vibration"
CONF_DISPLAY_ON_COOLING = "display_on_cooling"
CONF_DISPLAY_UNITS_FAHRENHEIT = "display_units_fahrenheit"
CONF_LED_BRIGHTNESS = "led_brightness"
CONF_HOURS_OF_OPERATION = "hours_of_operation"
CONF_MINUTES_OF_OPERATION = "minutes_of_operation"

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
VolcanoLedBrightnessNumber = volcano_ns.class_(
    "VolcanoLedBrightnessNumber", number.Number, cg.Parented.template(VolcanoComponent)
)
VolcanoVibrationSwitch = volcano_ns.class_(
    "VolcanoVibrationSwitch", switch.Switch, cg.Parented.template(VolcanoComponent)
)
VolcanoDisplayOnCoolingSwitch = volcano_ns.class_(
    "VolcanoDisplayOnCoolingSwitch", switch.Switch, cg.Parented.template(VolcanoComponent)
)
VolcanoDisplayUnitsSwitch = volcano_ns.class_(
    "VolcanoDisplayUnitsSwitch", switch.Switch, cg.Parented.template(VolcanoComponent)
)
VolcanoHeaterSwitch = volcano_ns.class_(
    "VolcanoHeaterSwitch", switch.Switch, cg.Parented.template(VolcanoComponent)
)
VolcanoPumpSwitch = volcano_ns.class_(
    "VolcanoPumpSwitch", switch.Switch, cg.Parented.template(VolcanoComponent)
)

# Defaults for the three writable number entities are the ranges the
# protocol documentation records as confirmed accepted -- 40.0-230.0 degC
# for CMD-001, 1-360 minutes for CMD-003, and 0-100% for CMD-002. They are
# configurable so an example can deliberately span wider to exercise the
# component's own refusal of an out-of-range value; the component enforces
# the confirmed range regardless of what the entity advertises.
DEFAULT_TARGET_TEMPERATURE_MIN = 40.0
DEFAULT_TARGET_TEMPERATURE_MAX = 230.0
DEFAULT_AUTO_SHUTOFF_DURATION_MIN = 1.0
DEFAULT_AUTO_SHUTOFF_DURATION_MAX = 360.0
DEFAULT_LED_BRIGHTNESS_MIN = 0.0
DEFAULT_LED_BRIGHTNESS_MAX = 100.0


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
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TARGET_TEMPERATURE): number.number_schema(
                VolcanoTargetTemperatureNumber,
                unit_of_measurement=UNIT_CELSIUS,
                icon="mdi:thermostat",
                device_class=DEVICE_CLASS_TEMPERATURE,
            ).extend(
                _number_range_schema(
                    DEFAULT_TARGET_TEMPERATURE_MIN, DEFAULT_TARGET_TEMPERATURE_MAX
                )
            ),
            # Settings rather than live control: they configure how the
            # device behaves rather than commanding it, so they default to
            # the config entity category and group away from the controls.
            cv.Optional(CONF_AUTO_SHUTOFF_DURATION): number.number_schema(
                VolcanoAutoShutoffDurationNumber,
                unit_of_measurement=UNIT_MINUTE,
                icon=ICON_TIMER,
                device_class=DEVICE_CLASS_DURATION,
                entity_category=ENTITY_CATEGORY_CONFIG,
            ).extend(
                _number_range_schema(
                    DEFAULT_AUTO_SHUTOFF_DURATION_MIN, DEFAULT_AUTO_SHUTOFF_DURATION_MAX
                )
            ),
            cv.Optional(CONF_LED_BRIGHTNESS): number.number_schema(
                VolcanoLedBrightnessNumber,
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_BRIGHTNESS_5,
                entity_category=ENTITY_CATEGORY_CONFIG,
            ).extend(
                _number_range_schema(
                    DEFAULT_LED_BRIGHTNESS_MIN, DEFAULT_LED_BRIGHTNESS_MAX
                )
            ),
            cv.Optional(CONF_AUTO_SHUTOFF_COUNTDOWN): sensor.sensor_schema(
                unit_of_measurement=UNIT_SECOND,
                icon=ICON_TIMER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DURATION,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            # ADR-0009: whether VolcanoDevice's connection state is READY --
            # the link is up AND the initial read sweep has completed, not
            # merely that the link exists. No entity category: this is what
            # every other entity's own validity depends on, so it groups
            # with the primary state/controls rather than the diagnostics.
            cv.Optional(CONF_CONNECTED): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_CONNECTIVITY,
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
            # The heater-runtime meter (STATE-001/STATE-006), a pair:
            # minutes counts 0-59 and carries into hours. Both advance only
            # while the heater is on, so neither tracks wall-clock time.
            cv.Optional(CONF_HOURS_OF_OPERATION): sensor.sensor_schema(
                unit_of_measurement=UNIT_HOUR,
                icon=ICON_TIMER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DURATION,
                state_class=STATE_CLASS_TOTAL_INCREASING,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_MINUTES_OF_OPERATION): sensor.sensor_schema(
                unit_of_measurement=UNIT_MINUTE,
                icon=ICON_TIMER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DURATION,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            # A setting rather than an actuator, so it groups with the
            # other settings rather than with Heat and Air. Same DISABLED
            # restore mode and same no-optimistic-publish rule as those:
            # the state shown is the register's, not what was requested.
            cv.Optional(CONF_VIBRATION): switch.switch_schema(
                VolcanoVibrationSwitch,
                icon="mdi:vibrate",
                entity_category=ENTITY_CATEGORY_CONFIG,
                default_restore_mode="DISABLED",
            ),
            cv.Optional(CONF_DISPLAY_ON_COOLING): switch.switch_schema(
                VolcanoDisplayOnCoolingSwitch,
                icon="mdi:snowflake",
                entity_category=ENTITY_CATEGORY_CONFIG,
                default_restore_mode="DISABLED",
            ),
            cv.Optional(CONF_DISPLAY_UNITS_FAHRENHEIT): switch.switch_schema(
                VolcanoDisplayUnitsSwitch,
                icon="mdi:temperature-fahrenheit",
                entity_category=ENTITY_CATEGORY_CONFIG,
                default_restore_mode="DISABLED",
            ),
            # Device information: fixed per unit, read once per connection,
            # never written. Diagnostic rather than primary state, so they
            # do not sit alongside the controls by default.
            cv.Optional(CONF_FIRMWARE_VERSION): text_sensor.text_sensor_schema(
                icon=ICON_CHIP,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_BLE_FIRMWARE_VERSION): text_sensor.text_sensor_schema(
                icon=ICON_BLUETOOTH,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_SERIAL_NUMBER): text_sensor.text_sensor_schema(
                icon="mdi:identifier",
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            # CHAR-024 and CHAR-025 carry Confirmed values whose *meaning* is
            # only Probable (see docs/protocol/characteristics.md), which is
            # what these two key names encode. The strings themselves are
            # exactly what the device returns.
            cv.Optional(CONF_POWER_SUPPLY): text_sensor.text_sensor_schema(
                icon=ICON_POWER,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_PRODUCT_LINE): text_sensor.text_sensor_schema(
                icon="mdi:tag-outline",
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
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

    if led_config := config.get(CONF_LED_BRIGHTNESS):
        num = await number.new_number(
            led_config,
            min_value=led_config[CONF_MIN_VALUE],
            max_value=led_config[CONF_MAX_VALUE],
            step=led_config[CONF_STEP],
        )
        await cg.register_parented(num, var)
        cg.add(var.set_led_brightness_number(num))

    if countdown_config := config.get(CONF_AUTO_SHUTOFF_COUNTDOWN):
        sens = await sensor.new_sensor(countdown_config)
        cg.add(var.set_auto_shutoff_countdown_sensor(sens))

    if connected_config := config.get(CONF_CONNECTED):
        cg.add(var.set_connected_binary_sensor(await binary_sensor.new_binary_sensor(connected_config)))

    if heater_config := config.get(CONF_HEATER):
        sw = await switch.new_switch(heater_config)
        await cg.register_parented(sw, var)
        cg.add(var.set_heater_switch(sw))

    if pump_config := config.get(CONF_PUMP):
        sw = await switch.new_switch(pump_config)
        await cg.register_parented(sw, var)
        cg.add(var.set_pump_switch(sw))

    if hours_config := config.get(CONF_HOURS_OF_OPERATION):
        cg.add(var.set_hours_sensor(await sensor.new_sensor(hours_config)))

    if minutes_config := config.get(CONF_MINUTES_OF_OPERATION):
        cg.add(var.set_minutes_sensor(await sensor.new_sensor(minutes_config)))

    if vibration_config := config.get(CONF_VIBRATION):
        sw = await switch.new_switch(vibration_config)
        await cg.register_parented(sw, var)
        cg.add(var.set_vibration_switch(sw))

    if units_config := config.get(CONF_DISPLAY_UNITS_FAHRENHEIT):
        sw = await switch.new_switch(units_config)
        await cg.register_parented(sw, var)
        cg.add(var.set_display_units_switch(sw))

    if display_config := config.get(CONF_DISPLAY_ON_COOLING):
        sw = await switch.new_switch(display_config)
        await cg.register_parented(sw, var)
        cg.add(var.set_display_on_cooling_switch(sw))

    for key, setter in (
        (CONF_FIRMWARE_VERSION, var.set_firmware_version_text_sensor),
        (CONF_BLE_FIRMWARE_VERSION, var.set_ble_firmware_version_text_sensor),
        (CONF_SERIAL_NUMBER, var.set_serial_number_text_sensor),
        (CONF_POWER_SUPPLY, var.set_power_supply_text_sensor),
        (CONF_PRODUCT_LINE, var.set_product_line_text_sensor),
    ):
        if entry := config.get(key):
            cg.add(setter(await text_sensor.new_text_sensor(entry)))
