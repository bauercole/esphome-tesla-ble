import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from . import TeslaBLEVehicle, tesla_ble_vehicle_ns

CODEOWNERS = ["@PedroKTFC"]
DEPENDENCIES = ["tesla_ble_vehicle"]

CONF_TESLA_BLE_VEHICLE_ID = "tesla_ble_vehicle_id"

TeslaBLEClimate = tesla_ble_vehicle_ns.class_(
    "TeslaBLEClimate", climate.Climate, cg.Component
)

CONFIG_SCHEMA = (
    climate.climate_schema(TeslaBLEClimate, icon="mdi:air-conditioner")
    .extend(
        {
            cv.GenerateID(CONF_TESLA_BLE_VEHICLE_ID): cv.use_id(TeslaBLEVehicle),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    parent = await cg.get_variable(config[CONF_TESLA_BLE_VEHICLE_ID])
    cg.add(var.set_parent(parent))
    cg.add(parent.set_climate(var))
