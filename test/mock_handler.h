#ifndef MOCK_HANDLER_H
#define MOCK_HANDLER_H

#include "../src/handler.h"
#include "command_handler.h"

Handler *create_mock_handler();
void destroy_mock_handler(Handler *handler);

CommandHandler *create_mock_command_handler();
void destroy_mock_command_handler(CommandHandler *mock_ch);

#endif // MOCK_HANDLER_H