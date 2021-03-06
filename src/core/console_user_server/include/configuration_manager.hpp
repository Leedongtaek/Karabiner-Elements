#pragma once

#include "constants.hpp"
#include "core_configuration.hpp"
#include "file_monitor.hpp"
#include "filesystem.hpp"
#include "grabber_client.hpp"
#include <CoreServices/CoreServices.h>
#include <memory>

class configuration_manager final {
public:
  configuration_manager(const configuration_manager&) = delete;

  configuration_manager(spdlog::logger& logger,
                        grabber_client& grabber_client) : logger_(logger),
                                                          grabber_client_(grabber_client),
                                                          need_to_check_for_updates_(true) {
    filesystem::create_directory_with_intermediate_directories(constants::get_user_configuration_directory(), 0700);

    auto core_configuration_file_path = constants::get_core_configuration_file_path();

    std::vector<std::pair<std::string, std::vector<std::string>>> targets = {
        {constants::get_user_configuration_directory(), {core_configuration_file_path}},
    };
    file_monitor_ = std::make_unique<file_monitor>(logger_, targets,
                                                   std::bind(&configuration_manager::core_configuration_file_updated_callback, this, std::placeholders::_1));

    core_configuration_file_updated_callback(core_configuration_file_path);
  }

  ~configuration_manager(void) {
    file_monitor_ = nullptr;
  }

private:
  void core_configuration_file_updated_callback(const std::string& file_path) {
    auto new_ptr = std::make_unique<core_configuration>(logger_, file_path);
    // skip if karabiner.json is broken.
    if (core_configuration_ && !new_ptr->is_loaded()) {
      return;
    }

    core_configuration_ = std::move(new_ptr);
    logger_.info("core_configuration_ was loaded.");

    // ----------------------------------------
    // Send configuration to grabber.

    grabber_client_.core_configuration_updated();

    grabber_client_.clear_simple_modifications();
    for (const auto& pair : core_configuration_->get_current_profile_simple_modifications()) {
      grabber_client_.add_simple_modification(pair.first, pair.second);
    }

    grabber_client_.clear_fn_function_keys();
    for (const auto& pair : core_configuration_->get_current_profile_fn_function_keys()) {
      grabber_client_.add_fn_function_key(pair.first, pair.second);
    }

    grabber_client_.virtual_hid_keyboard_configuration_updated(core_configuration_->get_current_profile_virtual_hid_keyboard());

    grabber_client_.clear_devices();
    for (const auto& pair : core_configuration_->get_current_profile_devices()) {
      grabber_client_.add_device(pair.first, pair.second);
    }
    grabber_client_.complete_devices();

    // ----------------------------------------
    // Check for updates
    if (need_to_check_for_updates_) {
      need_to_check_for_updates_ = false;
      if (core_configuration_->get_global_check_for_updates_on_startup()) {
        logger_.info("Check for updates...");
        system("open '/Library/Application Support/org.pqrs/Karabiner-Elements/updater/Karabiner-Elements.app'");
      }
    }
  }

  spdlog::logger& logger_;
  grabber_client& grabber_client_;

  std::unique_ptr<file_monitor> file_monitor_;

  std::unique_ptr<core_configuration> core_configuration_;

  bool need_to_check_for_updates_;
};
