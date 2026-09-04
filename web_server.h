#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "config.h"
#include <WebServer.h>

extern WebServer server;

void initWebServer();
void handleWebServer();
String getMinimalHTML();

#endif