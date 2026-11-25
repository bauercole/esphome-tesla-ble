#pragma once

#include <array>
#include <cmath>
#include <car_server.pb.h>
#include <esphome/components/climate/climate.h>
#include <esphome/core/component.h>
#include <vehicle.pb.h>

namespace esphome
{
  namespace tesla_ble_vehicle
  {
    class TeslaBLEVehicle;

    class TeslaBLEClimate : public climate::Climate, public Component
    {
    public:
      static constexpr const char *PRESET_KEEP    = "Keep";
      static constexpr const char *PRESET_DOG     = "Dog";
      static constexpr const char *PRESET_CAMP    = "Camp";
      static constexpr const char *PRESET_DEFROST = "Defrost";

      struct ClimateKeeperMapping
      {
        int state_tag;
        CarServer_HvacClimateKeeperAction_ClimateKeeperAction_E action;
        const char *preset_label;
      };

      static constexpr std::array<ClimateKeeperMapping, 3> CLIMATE_KEEPER_MAP {{
          {CarServer_ClimateState_ClimateKeeperMode_On_tag,    CarServer_HvacClimateKeeperAction_ClimateKeeperAction_E_ClimateKeeperAction_On,   PRESET_KEEP},
          {CarServer_ClimateState_ClimateKeeperMode_Dog_tag,   CarServer_HvacClimateKeeperAction_ClimateKeeperAction_E_ClimateKeeperAction_Dog,  PRESET_DOG},
          {CarServer_ClimateState_ClimateKeeperMode_Party_tag, CarServer_HvacClimateKeeperAction_ClimateKeeperAction_E_ClimateKeeperAction_Camp, PRESET_CAMP},
      }};

      TeslaBLEClimate();

      void dump_config() override;

      void set_parent(TeslaBLEVehicle *parent) { this->parent_ = parent; }

      void update_from_climate_state(const CarServer_ClimateState &climate_state);
      void set_unknown();

    protected:
      climate::ClimateTraits traits() override;
      void control(const climate::ClimateCall &call) override;

      TeslaBLEVehicle *parent_{nullptr};
      bool defrost_active_{false};
      bool climate_keeper_active_{false};
    };

  }  // namespace tesla_ble_vehicle
}  // namespace esphome
