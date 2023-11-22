#include "WebService.h"
#include <map>
#include <ESPmDNS.h>

WebService *WebService::m_instance{nullptr};

WebService::WebService()
    : m_AutoConnect(m_WebServer),
    m_auxConfig("/", "Config"),
      m_deleteLocoConfig("deleteLocoConfig", "deleteLocoConfig", "Delete internal memory for z21 loco config", false),
    m_configButton("configButton", "Run", "/configstatus"),
    m_auxConfigStatus("/configstatus", "Config Status"),
      m_readingStatus("readingStatus", "readingStatus", "Done")
{
}

WebService *WebService::getInstance()
{
    if (nullptr == m_instance)
    {
        m_instance = new WebService();
    }
    return m_instance;
};

WebService::~WebService()
{
}

void WebService::cyclic()
{
    m_AutoConnect.handleClient();
}

void WebService::begin(AutoConnectConfig &autoConnectConfig, std::function<void(void)> deleteLocoConfigFkt)
{
m_deleteLocoConfigFkt = deleteLocoConfigFkt;

m_auxConfigStatus.on([this](AutoConnectAux &aux, PageArgument &arg)
                            {
                                if (m_AutoConnect.where() != "/configstatus") 
                                {
                                    if (m_WebServer.hasArg("deleteLocoConfig"))
                                    {
                                        Serial.println("deleting z21 loco config");
                                        m_deleteLocoConfigFkt();
                                    }
                                }                            
                            return String(); });

    m_AutoConnect.config(autoConnectConfig);

    m_AutoConnect.onNotFound(WebService::handleNotFound);

    m_auxConfig.add({m_deleteLocoConfig, m_configButton});
    m_auxConfigStatus.add({m_readingStatus});

    m_AutoConnect.join(m_auxConfig);
    m_AutoConnect.join(m_auxConfigStatus);

    m_AutoConnect.begin();

    if (MDNS.begin("z21"))
    {
        MDNS.addService("http", "tcp", 80);
    }
}

void WebService::handleNotFound(void)
{
    String message = "File Not Found\n";
    message += "URI: ";
    message += m_instance->m_WebServer.uri();
    message += "\nMethod: ";
    message += (m_instance->m_WebServer.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += m_instance->m_WebServer.args();
    message += "\n";
    for (uint8_t i = 0; i < m_instance->m_WebServer.args(); i++)
    {
        message += " " + m_instance->m_WebServer.argName(i) + ": " + m_instance->m_WebServer.arg(i) + "\n";
    }
#ifdef DEBUG
    Serial.print(message);
#endif
    m_instance->m_WebServer.send(404, "text/plain", message);
}

String WebService::getContentType(const String &filename)
{
    const static std::map<const String, const String> contentTypes{
        {".cs2", "text/plain"},
        {".txt", "text/plain"},
        {".htm", "text/html"},
        {".html", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".png", "image/png"},
        {".gif", "image/gif"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".ico", "image/x-icon"},
        {".svg", "image/svg+xml"},
        {".xml", "text/xml"},
        {".pdf", "application/x-pdf"},
        {".zip", "application/x-zip"},
        {".gz", "application/x-gzip"},
        {".z21", "application/octet-stream"}};
    for (auto iter = contentTypes.begin(); iter != contentTypes.end(); ++iter)
    {
        if (filename.endsWith(iter->first))
        {
            return iter->second;
        }
    }
    return "text/plain";
}