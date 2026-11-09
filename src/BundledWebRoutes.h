#pragma once
class AsyncWebServer;
// Registration readiness only; complete postboot health has separate gates.
bool registerBundledWebRoutes(AsyncWebServer &server);
bool bundledWebRoutesReady();
