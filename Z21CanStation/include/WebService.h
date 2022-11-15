#pragma once

#include <WebServer.h>
#include <AutoConnect.h>
#include <SPIFFS.h>
#include <string>
#include <memory>

//#define DEBUG

// class is designed as a singelton
class WebService
{
public:
    static WebService *getInstance();
    virtual ~WebService();

    void cyclic();

    void begin(AutoConnectConfig &autoConnectConfig);
private:
    static WebService *m_instance;
    WebService();
    static void handleNotFound(void);
    static String postUpload(AutoConnectAux &aux, PageArgument &args);
    String getContentType(const String &filename);

    WebServer m_WebServer;
    AutoConnect m_AutoConnect;
};