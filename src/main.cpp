#include <Arduino_JSON.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

#include "queue.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN    2
#endif

#define REMOTE_LED_PIN 2
#define LOG_ENABLE     false

#ifndef WIFI_SSID
#define WIFI_SSID "abcdefgh"
#endif

#ifndef WIFI_PASSWD
#define WIFI_PASSWD "12345678"
#endif

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
    TX_DATA_STATUS             = 'A',
    TX_DATA_MAIN_OUTPUT_TOGGLE = 'B',
    TX_DATA_TIMER_TOGGLE       = 'C',
    TX_DATA_TIMER_SET_VALUES   = 'D',
    TX_DATA_TEMP_STATUS        = 'E',
    TX_DATA_TEMP_SET           = 'F'
} uart_tx_data_e;

typedef enum {
    RX_DATA_STATUS_OK             = 'A',
    RX_DATA_MAIN_OUTPUT_TOGGLE_OK = 'B',
    RX_DATA_TIMER_TOGGLE_OK       = 'C',
    RX_DATA_TIMER_SET_VALUES_OK   = 'D',
    RX_DATA_TEMP_STATUS_OK        = 'E',
    RX_DATA_TEMP_SET_OK           = 'F'
} uart_rx_data_e;

typedef enum {
    QUERY_STATUS = 0,
    QUERY_MAIN_OUTPUT_TOGGLE,
    QUERY_TIMER_TOGGLE,
    QUERY_TIMER_SET_VALUES,
    QUERY_TEMP_SET
} query_type_e;

typedef struct {
    uint8_t enabled;
    uint8_t from_hour;
    uint8_t from_minute;
    uint8_t to_hour;
    uint8_t to_minute;
} timer_param_t;

ESP8266WebServer server(80);
WebSocketsServer web_socket = WebSocketsServer(81);
uint8_t remote_devices, main_output_enabled;
time_t system_time;
timer_param_t timer, new_timer;
JSONVar data, timer_data;
queue_t uart_queue_tx, uart_queue_rx;
int16_t temp_now, temp_now_new, temp_set_point, temp_set_point_new;

void setup_wifi();
void setup_websocket();
void setup_webserver();
void setup_fs();

void websocket_event_handler(uint8_t num, WStype_t type, uint8_t *payload, size_t len);
uint16_t webserver_get_file(String path, String &return_page);
String webserver_file_content_type(String path);
void webserver_file_handler();
void webserver_handle_root();

void update_data();
void update_timer_data();

void setup_wifi()
{
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);

    LOG_INFO("Connecting to WiFi");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(200);
        LOG(".");
    }
    LOG("\n");

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

void webserver_update_all_clients_checkbox()
{
    JSONVar output_data;
    String output_data_as_json;

    output_data["type"] = "cb";
    output_data["main-output-enabled"] = String(main_output_enabled);
    output_data["timer"]["enabled"] = String(timer.enabled);

    output_data_as_json = JSON.stringify(output_data);
    web_socket.broadcastTXT(output_data_as_json.c_str());
}

void webserver_update_all_clients_timer_values()
{
    JSONVar output_data;
    String output_data_as_json;

    update_timer_data();
    output_data["type"] = "timer";
    output_data["timer"] = timer_data;

    // web_socket.broadcastTXT(JSON.stringify(output_data).c_str());
    output_data_as_json = JSON.stringify(output_data);
    web_socket.broadcastTXT(output_data_as_json.c_str());
}

void webserver_update_all_clients_temp_now_value()
{
    JSONVar output_data;
    String output_data_as_json;

    output_data["type"] = "temp-now-values";
    output_data["temp-now"] = temp_now;

    // web_socket.broadcastTXT(JSON.stringify(output_data).c_str());
    output_data_as_json = JSON.stringify(output_data);
    web_socket.broadcastTXT(output_data_as_json.c_str());
}


void webserver_update_all_clients_temp_values()
{
    JSONVar output_data;
    String output_data_as_json;

    output_data["type"] = "temp-values";
    output_data["temp-now"] = temp_now;
    output_data["temp-set"] = temp_set_point;

    // web_socket.broadcastTXT(JSON.stringify(output_data).c_str());
    output_data_as_json = JSON.stringify(output_data);
    web_socket.broadcastTXT(output_data_as_json.c_str());
}

void websocket_event_handler(uint8_t num, WStype_t type, uint8_t *payload, size_t len)
{
    IPAddress ip;
    query_type_e query_type;

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

            query_type = (query_type_e)(*payload - '0');
            LOG_INFO("Received query_type '%s'\n", payload);

            switch(query_type)
            {
                case QUERY_STATUS:
                    LOG_INFO("Enqueueing 'TX_DATA_STATUS'\n");
                    queue_enqueue(&uart_queue_tx, TX_DATA_STATUS);
                break;

                case QUERY_MAIN_OUTPUT_TOGGLE:
                    LOG_INFO("Enqueueing 'TX_DATA_MAIN_OUTPUT_TOGGLE'\n");
                    queue_enqueue(&uart_queue_tx, TX_DATA_MAIN_OUTPUT_TOGGLE);
                break;

                case QUERY_TIMER_TOGGLE:
                    LOG_INFO("Enqueueing 'TX_DATA_TIMER_TOGGLE'\n");
                    queue_enqueue(&uart_queue_tx, TX_DATA_TIMER_TOGGLE);
                break;

                case QUERY_TIMER_SET_VALUES:
                    payload++; // skip query type
                    timer_data = JSON.parse((char *)payload);

                    if(JSON.typeof(timer_data) == "undefined")
                    {
                        LOG_ERROR("QUERY_TIMER_SET_VALUES: Parsing payload failed!");
                        break;
                    }

                    new_timer.from_hour   = (uint8_t)String(timer_data["from"]["hour"]).toInt();
                    new_timer.from_minute = (uint8_t)String(timer_data["from"]["minute"]).toInt();
                    new_timer.to_hour     = (uint8_t)String(timer_data["to"]["hour"]).toInt();
                    new_timer.to_minute   = (uint8_t)String(timer_data["to"]["minute"]).toInt();

                    LOG_INFO("Enqueueing 'TX_DATA_TIMER_SET_VALUES'\n");
                    LOG_INFO("With values: %02d %02d  %02d %02d\n",
                                new_timer.from_hour, new_timer.from_minute,
                                new_timer.to_hour, new_timer.to_minute);

                    queue_enqueue(&uart_queue_tx, TX_DATA_TIMER_SET_VALUES);
                    queue_enqueue(&uart_queue_tx, new_timer.from_hour);
                    queue_enqueue(&uart_queue_tx, new_timer.from_minute);
                    queue_enqueue(&uart_queue_tx, new_timer.to_hour);
                    queue_enqueue(&uart_queue_tx, new_timer.to_minute);

                break;

                case QUERY_TEMP_SET: {
                    payload++; // skip query type
                    JSONVar temp_data;
                    temp_data = JSON.parse((char *)payload);
                    temp_set_point_new = (uint8_t)String(temp_data["temp_set_point_new"]).toInt();
                    queue_enqueue(&uart_queue_tx, TX_DATA_TEMP_SET);
                    queue_enqueue(&uart_queue_tx, (uint8_t)temp_set_point_new);
                }
                break;

                default:
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

// TODO: This should return server response codes, i.e. 200, 404, etc
uint16_t webserver_get_file(String path, String &return_page)
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
    uint16_t response_code;
    response_code = webserver_get_file(path, requested_page) ? 404 : 200;
    server.send(response_code, webserver_file_content_type(path), requested_page);
}

void webserver_handle_root()
{
    String index_page;
    uint16_t response_code = 200;
    if (webserver_get_file("index.html", index_page)) {
        response_code = 404;
    }
    server.send(response_code, "text/html", index_page.c_str());
}

String left_pad(uint8_t n) {
    return n < 10 ? "0" + String(n) : String(n);
}

void update_timer_data() {
    timer_data["enabled"]        = String(timer.enabled);
    timer_data["from"]["hour"]   = left_pad(timer.from_hour);
    timer_data["from"]["minute"] = left_pad(timer.from_minute);
    timer_data["to"]["hour"]     = left_pad(timer.to_hour);
    timer_data["to"]["minute"]   = left_pad(timer.to_minute);
}

void update_data() {
    data["main-output-enabled"] = String(main_output_enabled);
    data["system-ip-addr"]      = WiFi.localIP().toString();
    data["system-time"]         = (uint32_t)system_time;
    data["wifi-ssid"]           = WIFI_SSID;
    data["wifi-rssi"]           = WiFi.RSSI();
    update_timer_data();
    data["timer"] = timer_data;
    data["temp-now"] = temp_now;
    data["temp-set"] = temp_set_point;
}

void uart_rx_handler(void)
{
    String data_as_json;
    uint8_t buffer_aux[4];

    /*
    uart_rx_data_e rx_data_type = (uart_rx_data_e)queue_dequeue(&uart_queue_rx);
    */

    uint8_t rx_data_type = queue_peek(&uart_queue_rx);

    if ((rx_data_type == RX_DATA_STATUS_OK) && (queue_count(&uart_queue_rx) < 10))
    {
        // Wait for additional data to arrive
        return;
    }

    if ((rx_data_type == RX_DATA_TEMP_STATUS_OK) && (queue_count(&uart_queue_rx) < 2))
    {
        // Wait for additional data to arrive
        return;
    }

    queue_dequeue(&uart_queue_rx);

    switch(rx_data_type)
    {
        case RX_DATA_STATUS_OK:

            main_output_enabled = queue_dequeue(&uart_queue_rx);

            // LSB first
            buffer_aux[0] = queue_dequeue(&uart_queue_rx);
            buffer_aux[1] = queue_dequeue(&uart_queue_rx);
            buffer_aux[2] = queue_dequeue(&uart_queue_rx);
            buffer_aux[3] = queue_dequeue(&uart_queue_rx);

            system_time = ((uint32_t)buffer_aux[3] << 24) |
                          ((uint32_t)buffer_aux[2] << 16) |
                          ((uint32_t)buffer_aux[1] << 8)  |
                          ((uint32_t)buffer_aux[0]);

            timer.enabled     = queue_dequeue(&uart_queue_rx);
            timer.from_hour   = queue_dequeue(&uart_queue_rx);
            timer.from_minute = queue_dequeue(&uart_queue_rx);
            timer.to_hour     = queue_dequeue(&uart_queue_rx);
            timer.to_minute   = queue_dequeue(&uart_queue_rx);

            temp_now       = queue_dequeue(&uart_queue_rx);
            temp_set_point = queue_dequeue(&uart_queue_rx);

            update_data();
            data["type"] = "all";

            // TODO: Use sendTXT instead of braodcastTXT; resolve socket numb
            // web_socket.sendTXT(num, data_as_json);
            data_as_json = JSON.stringify(data);
            web_socket.broadcastTXT(data_as_json.c_str());
            LOG_INFO("%s\n", data_as_json.c_str());

        break;
        case RX_DATA_MAIN_OUTPUT_TOGGLE_OK:

            main_output_enabled = !main_output_enabled;
            webserver_update_all_clients_checkbox();

        break;

        case RX_DATA_TIMER_TOGGLE_OK:

            timer.enabled = !timer.enabled;
            webserver_update_all_clients_checkbox();

        break;

        case RX_DATA_TIMER_SET_VALUES_OK:

            timer.from_hour   = new_timer.from_hour;
            timer.from_minute = new_timer.from_minute;
            timer.to_hour     = new_timer.to_hour;
            timer.to_minute   = new_timer.to_minute;
            webserver_update_all_clients_timer_values();

        break;

        case RX_DATA_TEMP_SET_OK:

            temp_set_point = temp_set_point_new;
            webserver_update_all_clients_temp_values();

        break;

        case RX_DATA_TEMP_STATUS_OK:

            temp_now_new = queue_dequeue(&uart_queue_rx);
            temp_set_point = queue_dequeue(&uart_queue_rx); // discard value

            if (temp_now_new != temp_now)
            {
                temp_now = temp_now_new;
                webserver_update_all_clients_temp_now_value();
            }

        break;

        default:
            queue_dequeue(&uart_queue_rx);
        break;
    }
}

void setup()
{
    Serial.begin(115200);

    delay(2000);
    LOG_INFO("Booting");
    for(uint8_t i = 10; i > 0; i--) {
        LOG(".");
        delay(150);
    }
    LOG("\n");

    pinMode(REMOTE_LED_PIN, OUTPUT);
    digitalWrite(REMOTE_LED_PIN, LOW);

    remote_devices = 0;

    timer.enabled = 0;
    timer.from_hour = 12;
    timer.from_minute = 0;
    timer.to_hour = 0;
    timer.to_minute = 0;

    main_output_enabled = 0;

    temp_now = 0;
    temp_now_new = 0;
    temp_set_point = 0;
    temp_set_point_new = 0;

    time(&system_time); // read the current time

    setup_wifi();
    setup_fs();
    setup_websocket();
    setup_webserver();

    LOG_INFO("Setup done\n");
    digitalWrite(REMOTE_LED_PIN, HIGH);

    queue_init(&uart_queue_tx);
    queue_init(&uart_queue_rx);
}

void loop()
{
    while (Serial.available() > 0)
    {
        int16_t byte = Serial.read();

        if (byte >= 0)
        {
            queue_enqueue(&uart_queue_rx, (uint8_t)byte);
        }
    }

    if (!queue_is_empty(&uart_queue_rx))
    {
        uart_rx_handler();
    }

    while (!queue_is_empty(&uart_queue_tx))
    {
        uint8_t value = queue_dequeue(&uart_queue_tx);
        LOG_INFO("Sending byte 0x%02X...\n", value);
        Serial.write(value);
        LOG("\n");
    }

    web_socket.loop();
    server.handleClient();
}
