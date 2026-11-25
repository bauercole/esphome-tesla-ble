#include "tesla_ble_climate.h"
#include "tesla_ble_vehicle.h"

#include <esphome/core/log.h>

namespace esphome
{
  namespace tesla_ble_vehicle
  {
    TeslaBLEClimate::TeslaBLEClimate()
    {
      this->set_supported_custom_presets({PRESET_KEEP, PRESET_DOG, PRESET_CAMP, PRESET_DEFROST});
      this->current_temperature = NAN;
      this->target_temperature = NAN;
      this->mode = climate::CLIMATE_MODE_OFF;
      this->set_preset_(climate::CLIMATE_PRESET_NONE);
    }

    void TeslaBLEClimate::dump_config()
    {
      LOG_CLIMATE("", "Tesla BLE Climate", this);
      this->dump_traits_(TAG);
    }

    climate::ClimateTraits TeslaBLEClimate::traits()
    {
      auto traits = climate::ClimateTraits();
      traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
      traits.set_supported_modes({
          climate::CLIMATE_MODE_OFF,
          climate::CLIMATE_MODE_HEAT_COOL,
      });
      traits.set_supported_presets({
          climate::CLIMATE_PRESET_NONE,
      });
      traits.set_visual_min_temperature(15.0f);
      traits.set_visual_max_temperature(28.0f);
      traits.set_visual_target_temperature_step(0.5f);
      traits.set_visual_current_temperature_step(0.1f);
      return traits;
    }

    void TeslaBLEClimate::update_from_climate_state(const CarServer_ClimateState &climate_state)
    {
      if (climate_state.which_optional_inside_temp_celsius)
      {
        this->current_temperature = climate_state.optional_inside_temp_celsius.inside_temp_celsius;
      }
      if (climate_state.which_optional_driver_temp_setting)
      {
        this->target_temperature = climate_state.optional_driver_temp_setting.driver_temp_setting;
      }
      if (climate_state.which_optional_is_climate_on)
      {
        this->mode = climate_state.optional_is_climate_on.is_climate_on
                         ? climate::CLIMATE_MODE_HEAT_COOL
                         : climate::CLIMATE_MODE_OFF;
      }

      if (climate_state.has_defrost_mode)
      {
        this->defrost_active_ = (climate_state.defrost_mode.which_type != CarServer_ClimateState_DefrostMode_Off_tag);
      }

      const char *keeper_preset = nullptr;
      if (climate_state.has_climate_keeper_mode)
      {
        for (const auto &entry : CLIMATE_KEEPER_MAP)
        {
          if (entry.state_tag == climate_state.climate_keeper_mode.which_type)
          {
            keeper_preset = entry.preset_label;
            break;
          }
        }
        this->climate_keeper_active_ = (keeper_preset != nullptr);
      }

      if (this->defrost_active_)
      {
        this->set_custom_preset_(PRESET_DEFROST);
      }
      else if (keeper_preset != nullptr)
      {
        this->set_custom_preset_(keeper_preset);
      }
      else if (!this->climate_keeper_active_ &&
               (climate_state.has_climate_keeper_mode || climate_state.has_defrost_mode))
      {
        this->set_preset_(climate::CLIMATE_PRESET_NONE);
      }

      this->publish_state();
    }

    void TeslaBLEClimate::set_unknown()
    {
      this->current_temperature = NAN;
      this->target_temperature = NAN;
      this->mode = climate::CLIMATE_MODE_OFF;
      this->defrost_active_ = false;
      this->climate_keeper_active_ = false;
      this->set_preset_(climate::CLIMATE_PRESET_NONE);
      this->publish_state();
    }

    void TeslaBLEClimate::control(const climate::ClimateCall &call)
    {
      if (this->parent_ == nullptr)
      {
        ESP_LOGW(TAG, "No parent TeslaBLEVehicle configured, ignoring climate call");
        return;
      }

      const bool turning_off = call.get_mode().has_value() && (*call.get_mode() == climate::CLIMATE_MODE_OFF);
      const bool turning_on = call.get_mode().has_value() && (*call.get_mode() == climate::CLIMATE_MODE_HEAT_COOL);

      if (turning_off)
      {
        if (this->defrost_active_)
        {
          this->parent_->sendCarServerVehicleActionMessage(BLE_CarServer_VehicleAction::DEFROST_CAR, 0);
          this->defrost_active_ = false;
        }
        if (this->climate_keeper_active_)
        {
          this->parent_->sendCarServerVehicleActionMessage(
              BLE_CarServer_VehicleAction::SET_CLIMATE_KEEPER,
              CarServer_HvacClimateKeeperAction_ClimateKeeperAction_E_ClimateKeeperAction_Off);
          this->climate_keeper_active_ = false;
        }
        this->parent_->sendCarServerVehicleActionMessage(BLE_CarServer_VehicleAction::SET_HVAC_SWITCH, 0);
        this->mode = climate::CLIMATE_MODE_OFF;
        this->set_preset_(climate::CLIMATE_PRESET_NONE);

        if (call.get_target_temperature().has_value())
        {
          float target = *call.get_target_temperature();
          this->parent_->setClimateTemp(target);
          this->target_temperature = target;
        }

        this->publish_state();
        return;
      }

      const bool want_none = call.get_preset().has_value() && (*call.get_preset() == climate::CLIMATE_PRESET_NONE);
      const bool want_custom = call.has_custom_preset();
      const StringRef custom_preset = call.get_custom_preset();
      const bool want_defrost = want_custom && (custom_preset == PRESET_DEFROST);

      const ClimateKeeperMapping *requested_keeper = nullptr;
      if (want_custom)
      {
        for (const auto &entry : CLIMATE_KEEPER_MAP)
        {
          if (custom_preset == entry.preset_label)
          {
            requested_keeper = &entry;
            break;
          }
        }
      }
      const bool want_keeper = (requested_keeper != nullptr);

      bool hvac_on_sent = false;
      if (turning_on && !want_defrost)
      {
        this->parent_->sendCarServerVehicleActionMessage(BLE_CarServer_VehicleAction::SET_HVAC_SWITCH, 1);
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        hvac_on_sent = true;
      }

      if (want_keeper && !hvac_on_sent && (this->mode == climate::CLIMATE_MODE_OFF))
      {
        // Climate keeper requires HVAC to be running
        this->parent_->sendCarServerVehicleActionMessage(BLE_CarServer_VehicleAction::SET_HVAC_SWITCH, 1);
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        hvac_on_sent = true;
      }

      if (call.get_target_temperature().has_value())
      {
        float target = *call.get_target_temperature();
        this->parent_->setClimateTemp(target);
        this->target_temperature = target;
      }

      if (want_none)
      {
        if (this->defrost_active_)
        {
          this->parent_->sendCarServerVehicleActionMessage(BLE_CarServer_VehicleAction::DEFROST_CAR, 0);
          this->defrost_active_ = false;
        }
        if (this->climate_keeper_active_)
        {
          this->parent_->sendCarServerVehicleActionMessage(
              BLE_CarServer_VehicleAction::SET_CLIMATE_KEEPER,
              CarServer_HvacClimateKeeperAction_ClimateKeeperAction_E_ClimateKeeperAction_Off);
          this->climate_keeper_active_ = false;
        }
        this->set_preset_(climate::CLIMATE_PRESET_NONE);
      }
      else if (want_defrost)
      {
        if (this->climate_keeper_active_)
        {
          this->parent_->sendCarServerVehicleActionMessage(
              BLE_CarServer_VehicleAction::SET_CLIMATE_KEEPER,
              CarServer_HvacClimateKeeperAction_ClimateKeeperAction_E_ClimateKeeperAction_Off);
          this->climate_keeper_active_ = false;
        }
        this->parent_->sendCarServerVehicleActionMessage(BLE_CarServer_VehicleAction::DEFROST_CAR, 1);
        this->defrost_active_ = true;
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        this->set_custom_preset_(PRESET_DEFROST);
      }
      else if (want_keeper)
      {
        if (this->defrost_active_)
        {
          this->parent_->sendCarServerVehicleActionMessage(BLE_CarServer_VehicleAction::DEFROST_CAR, 0);
          this->defrost_active_ = false;
        }

        this->parent_->sendCarServerVehicleActionMessage(
            BLE_CarServer_VehicleAction::SET_CLIMATE_KEEPER, requested_keeper->action);
        this->climate_keeper_active_ = true;
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        this->set_custom_preset_(requested_keeper->preset_label);
      }

      this->publish_state();
    }

  }  // namespace tesla_ble_vehicle
}  // namespace esphome
