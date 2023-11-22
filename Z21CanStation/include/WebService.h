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

    void begin(AutoConnectConfig &autoConnectConfig, std::function<void(void)> deleteLocoConfigFkt);
private:
    static WebService *m_instance;
    WebService();
    static void handleNotFound(void);
    static String postUpload(AutoConnectAux &aux, PageArgument &args);
    String getContentType(const String &filename);

    std::function<void(void)> m_deleteLocoConfigFkt;

    WebServer m_WebServer;
    AutoConnect m_AutoConnect;

    AutoConnectAux m_auxConfig;
    AutoConnectCheckbox m_deleteLocoConfig;
    AutoConnectSubmit m_configButton;

    AutoConnectAux m_auxConfigStatus;
    AutoConnectText m_readingStatus;
};