#pragma once

#include <obs-module.h>

// UI / properties layer — see flood-tuber-props.cpp

obs_properties_t *flood_tuber_properties(void *data);
void              flood_tuber_defaults(obs_data_t *settings);
