import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

# Volcano component scaffold (ADR-0002, ADR-0003).
#
# This component currently registers only an empty ESPHome Component.
# No BLE communication, Volcano state model, or domain commands exist
# yet -- see the TODO markers in volcano.h / volcano.cpp for where
# that work belongs.

CODEOWNERS = ["@SimmoDev"]

volcano_ns = cg.esphome_ns.namespace("volcano")
VolcanoComponent = volcano_ns.class_("VolcanoComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(VolcanoComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
