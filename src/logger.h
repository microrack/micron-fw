#pragma once

#include <stddef.h>

void logger_init();
void log_printf(const char* fmt, ...);
bool logger_get_next(char* out_message, size_t out_message_size);
