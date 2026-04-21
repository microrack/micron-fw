#pragma once

#include <stddef.h>

void logger_init();
void logger_printf(const char* fmt, ...);
bool logger_get_next(char* out_message, size_t out_message_size);

/// Optional: called once after a new line is appended (under `logger` mutex, before return).
void logger_set_output_notify(void (*notify)(void));
