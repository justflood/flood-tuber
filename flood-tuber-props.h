#pragma once

#include <obs-module.h>
#include <string>
#include <vector>

struct flood_tuber_data;

obs_properties_t *flood_tuber_properties(void *data);
void              flood_tuber_defaults(obs_data_t *settings);

// Set an avatar; returns false if the folder cannot be found
bool apply_avatar_to_settings(obs_data_t *settings, const char *avatar_name, bool as_default);

// Shared Avatar Changes: Upload Button, Cycle, Slots
bool switch_to_avatar(struct flood_tuber_data *tuber, const char *name);

// List of avatars: built-in + custom folder
void flood_tuber_list_avatars(obs_data_t *settings, std::vector<std::string> &out);
