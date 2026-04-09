#include <Arduino.h>
#include <Arduino_JSON.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN    2
#endif

#define REMOTE_LED_PIN 2
#define WIFI_SSID      "abcdefgh"
#define WIFI_PASSWD    "12345678"
#define LOG_ENABLE     true

#if true == LOG_ENABLE
#define LOG(fmt, ...) do {             \
        Serial.printf(fmt, ##__VA_ARGS__); \
        Serial.flush();                    \
    } while (0)
#else
#define LOG(fmt, ...)
#endif

#define LOG_INFO(fmt, ...) LOG("[INFO] " fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG("[ERROR] " fmt, ##__VA_ARGS__)

typedef enum {
    QUERY_STATUS
} query_t;

ESP8266WebServer server(80);
WebSocketsServer web_socket = WebSocketsServer(81);
static uint8_t remote_devices;
JSONVar data;

void setup_wifi();
void setup_websocket();
void setup_webserver();
void setup_fs();

void websocket_event_handler(uint8_t num, WStype_t type, uint8_t *payload, size_t len);
int webserver_get_file(String path, String &return_page);
String webserver_file_content_type(String path);
void webserver_file_handler();
void webserver_handle_root();

void update_data();

void setup_wifi()
{
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);

    LOG_INFO("Connecting to WiFi");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(200);
        LOG(".");
    }

    LOG_INFO("Connected to '%s', IP address %d.%d.%d.%d\n", WIFI_SSID,
             WiFi.localIP()[0],
             WiFi.localIP()[1],
             WiFi.localIP()[2],
             WiFi.localIP()[3]);
}

void setup_websocket()
{
    web_socket.begin();
    web_socket.onEvent(websocket_event_handler);
}

void setup_webserver()
{
    LOG_INFO("Loading server response from file 'index.html'\n");

    server.on("/", webserver_handle_root);
    server.onNotFound(webserver_file_handler);

    server.begin();
}

void setup_fs()
{
    if (LittleFS.begin() == 0)
    {
        LOG_ERROR("Error couldn't begin filesystem!\n");
    }

    FSInfo fs_info;
    LittleFS.info(fs_info);
    LOG_INFO("LittleFS started %d bytes used\n", fs_info.usedBytes);
}

void websocket_event_handler(uint8_t num, WStype_t type, uint8_t *payload, size_t len)
{
    String data_as_json;
    IPAddress ip;
    uint8_t query_type;

    switch(type)
    {
        case WStype_DISCONNECTED:

            LOG_INFO("%d: Disconnected\n", num);
            remote_devices -= 1;
            digitalWrite(REMOTE_LED_PIN, remote_devices ? LOW : HIGH);

        break;

        case WStype_CONNECTED:

            ip = web_socket.remoteIP(num);
            LOG_INFO("%u: Connected from %d.%d.%d.%d, URL '%s'\n",
                    num, ip[0], ip[1], ip[2], ip[3], payload);
            remote_devices += 1;
            digitalWrite(REMOTE_LED_PIN, remote_devices ? LOW : HIGH);

        break;

        case WStype_TEXT:

            query_type = *payload - '0';
            switch(query_type)
            {
                case QUERY_STATUS:
                    update_data();
                    data["type"] = "all";
                    data_as_json = JSON.stringify(data);
                    web_socket.sendTXT(num, data_as_json);
                break;

                default:
                    LOG_INFO(" %s\n", payload);
                break;
            }

            break;

        case WStype_BIN:

            LOG_INFO("%u: get binary length: %u\n", num, len);
            hexdump(payload, len);

        break;

        case WStype_ERROR:
        default: break;
    }
}

int webserver_get_file(String path, String &return_page)
{
    if (LittleFS.exists(path))
    {
        LOG_INFO("Serving file '%s'\n", path.c_str());
        File file = LittleFS.open(path.c_str(), "r");
        while (file.available())
        {
            return_page += (char)file.read();
        }
        file.close();
    }
    else
    {
        LOG_INFO("'%s' File Not Found\n", path.c_str());
        return_page = R"==(<!DOCTYPE html>
        <html>
          <head>
              <title>ERROR 404: File Not found!!</title>
              <meta name="viewport" content="width=device-width, initial-scale=1.0">
          </head>
          <body>
              <h>ERROR 404: File Not Found!</h1>
              <p>file '/index.html' not found</p>
          </body>
        </html>)==";
        return 1;
    }
    return 0;
}

String webserver_file_content_type(String path)
{
    if (path.endsWith(".html")) return "text/html";
    else if (path.endsWith(".css")) return "text/css";
    else if (path.endsWith(".js")) return "application/javascript";
    else if (path.endsWith(".ico")) return "image/x-icon";
    else if (path.endsWith(".gz")) return "application/x-gzip";
    return "text/plain";
}

void webserver_file_handler()
{
    String path = server.uri();
    String requested_page;
    int response_code;
    response_code = webserver_get_file(path, requested_page) ? 404 : 200;
    server.send(response_code, webserver_file_content_type(path), requested_page);
}

void webserver_handle_root()
{
    String index_page;
    int response_code = 200;
    if (webserver_get_file("index.html", index_page)) {
        response_code = 404;
    }
    server.send(response_code, "text/html", index_page.c_str());
}

void update_data() {
    data["system-ip-addr"] = WiFi.localIP().toString();
    data["wifi-ssid"]      = WIFI_SSID;
    data["wifi-rssi"]      = WiFi.RSSI();
}

void setup()
{
    pinMode(REMOTE_LED_PIN, OUTPUT);
    digitalWrite(REMOTE_LED_PIN, LOW);
    Serial.begin(115200);

    delay(2000);
    LOG_INFO("Booting");
    for(uint8_t i = 10; i > 0; i--) {
        LOG(".");
        delay(150);
    }
    LOG("\n");

    remote_devices = 0;

    setup_wifi();
    setup_fs();
    setup_websocket();
    setup_webserver();

    LOG_INFO("Setup done\n");
    digitalWrite(REMOTE_LED_PIN, HIGH);
}

void loop()
{
    web_socket.loop();
    server.handleClient();
}
