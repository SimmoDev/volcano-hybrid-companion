import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import CONF_ID

# Volcano component (ADR-0002, ADR-0003, ADR-0007).
#
# This is the first working increment: a read-only BLE communication layer
# that connects via the configured ble_client, resolves the status/flags
# register by UUID, subscribes, reads its initial value, decodes heater and
# pump state, and logs them. No writes to any characteristic exist yet --
# see volcano.h / volcano.cpp for the TODO markers showing where the
# remaining protocol coverage and the Volcano abstraction layer belong.

CODEOWNERS = ["@SimmoDev"]
DEPENDENCIES = ["ble_client"]

volcano_ns = cg.esphome_ns.namespace("volcano")
VolcanoComponent = volcano_ns.class_(
    "VolcanoComponent", cg.Component, ble_client.BLEClientNode
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(VolcanoComponent),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
