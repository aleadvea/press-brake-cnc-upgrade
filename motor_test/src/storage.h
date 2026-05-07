#pragma once

#include "ui_strings.h"

bool storage_init();
void storage_load_all();
void storage_save_materials();
void storage_save_programs();
void storage_save_settings();

// Language persistence
void storage_load_language();
void storage_save_language();
