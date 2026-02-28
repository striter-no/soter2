#ifndef DEVCORE_LOGGING_H

typedef enum {
    LOG_DEBUG   = 0,
    LOG_INFO    = 1,
    LOG_WARNING = 2,
    LOG_ERROR   = 3,
    LOG_FATAL   = 4
} logger_type;

typedef struct logger slogger;
slogger logger_init(int output_fd);
void logger_stop(slogger *logger);

void logger_set_level(slogger *l, logger_type level);
void logger_set_colors(slogger *l, int enable);
void logger_set_timestamp(slogger *l, int enable);

void slog(slogger *l, logger_type t, const char *format, ...);

#endif
#define DEVCORE_LOGGING_H