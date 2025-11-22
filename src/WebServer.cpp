#include "WebServer.h"
#include <Arduino.h>
#include <map>
#include <lwip/apps/httpd.h>
#include <lwip/apps/fs.h>
#include <lwip/def.h>
#include <lwip/mem.h>

// Configuration
#define LWIP_HTTPD_POST_MAX_PAYLOAD_LEN 2048

// Globals for POST handling
static std::string http_post_uri;
static char http_post_payload[LWIP_HTTPD_POST_MAX_PAYLOAD_LEN];
static uint16_t http_post_payload_len = 0;

// Static members
std::map<struct fs_file*, std::string> WebServer::responseBuffers;

// HTML Content
static const char *index_html = R"html(
<!DOCTYPE html>
<html>
<head>
    <title>RP2040 RNDIS</title>
    <style>
        body { font-family: sans-serif; text-align: center; padding: 20px; }
        .card { border: 1px solid #ccc; padding: 20px; border-radius: 8px; display: inline-block; }
    </style>
</head>
<body>
    <h1>Hello from RP2040 RNDIS!</h1>
    <div class="card">
        <h2>Device Status</h2>
        <p id="status">Loading...</p>
        <button onclick="fetchStatus()">Refresh</button>
    </div>
    <script>
        function fetchStatus() {
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('status').innerHTML =
                        'Uptime: ' + data.uptime + ' ms<br>' +
                        'Heap Free: ' + data.heap_free + ' bytes';
                })
                .catch(err => {
                    document.getElementById('status').innerText = 'Error fetching status';
                    console.error(err);
                });
        }
        fetchStatus();
        setInterval(fetchStatus, 5000);
    </script>
</body>
</html>
)html";

void WebServer::init() {
    // httpd_init() is called in RndisInterface.cpp
}

std::string WebServer::serializeJson(JsonDocument &doc) {
    std::string output;
    ::serializeJson(doc, output);
    return output;
}

std::string WebServer::apiStatus() {
    JsonDocument doc;
    doc["uptime"] = millis();
    doc["heap_free"] = rp2040.getFreeHeap();
    return serializeJson(doc);
}

std::string WebServer::apiEcho() {
    // Parse POST data
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http_post_payload, http_post_payload_len);

    JsonDocument response;
    if (error) {
        response["error"] = "Invalid JSON";
        response["details"] = error.c_str();
    } else {
        response["success"] = true;
        response["echo"] = doc;
    }
    return serializeJson(response);
}

// Handler mapping
typedef std::string (*HandlerFuncPtr)();
static const std::pair<const char*, HandlerFuncPtr> handlerFuncs[] = {
    { "/api/status", WebServer::apiStatus },
    { "/api/echo", WebServer::apiEcho },
};

int WebServer::setFileData(struct fs_file *file, const std::string& data) {
    // Use map to store buffer for this file handle
    std::string& buffer = responseBuffers[file];
    buffer.clear();
    buffer.reserve(data.length() + 128);

    buffer += "HTTP/1.0 200 OK\r\n";
    buffer += "Content-Type: application/json\r\n";
    buffer += "Access-Control-Allow-Origin: *\r\n";
    buffer += "Content-Length: " + std::to_string(data.length()) + "\r\n";
    buffer += "\r\n";
    buffer += data;

    file->data = buffer.c_str();
    file->len = buffer.length();
    file->index = file->len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED; // We included headers

    // pextension and is_custom_file removed as they might not be in the struct

    return 1;
}

void WebServer::handleCustomClose(struct fs_file *file) {
    responseBuffers.erase(file);
}

int WebServer::handleCustomOpen(struct fs_file *file, const char *name) {
    // Check API handlers
    for (const auto& handler : handlerFuncs) {
        if (strcmp(handler.first, name) == 0) {
            return setFileData(file, handler.second());
        }
    }

    // Serve Index
    if (strcmp(name, "/") == 0 || strcmp(name, "/index.html") == 0) {
        file->data = index_html;
        file->len = strlen(index_html);
        file->index = file->len;
        file->flags = 0; // Let LwIP add default headers (HTTP 200 OK, Content-Type html)
        return 1;
    }

    return 0;
}

// --- Extern C Callbacks for LwIP ---

extern "C" int fs_open_custom(struct fs_file *file, const char *name) {
    return WebServer::handleCustomOpen(file, name);
}

extern "C" void fs_close_custom(struct fs_file *file) {
    WebServer::handleCustomClose(file);
}

extern "C" int fs_read_custom(struct fs_file *file, char *buffer, int count) {
    (void)file;
    (void)buffer;
    (void)count;
    return 0;
}

extern "C" int fs_bytes_left_custom(struct fs_file *file) {
    return file->len - file->index;
}

extern "C" err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
                       uint16_t http_request_len, int content_len, char *response_uri,
                       uint16_t response_uri_len, uint8_t *post_auto_wnd) {
    (void)connection;
    (void)http_request;
    (void)http_request_len;
    (void)content_len;
    (void)response_uri;
    (void)response_uri_len;
    (void)post_auto_wnd;

    if (uri) {
        http_post_uri = uri;
    }
    http_post_payload_len = 0;
    return ERR_OK;
}

extern "C" err_t httpd_post_receive_data(void *connection, struct pbuf *p) {
    (void)connection;
    struct pbuf *q = p;
    while (q != NULL) {
        if (http_post_payload_len + q->len <= LWIP_HTTPD_POST_MAX_PAYLOAD_LEN) {
            memcpy(http_post_payload + http_post_payload_len, q->payload, q->len);
            http_post_payload_len += q->len;
        } else {
            // Overflow
            pbuf_free(p); // Must free the head
            return ERR_BUF;
        }
        q = q->next;
    }
    pbuf_free(p);
    return ERR_OK;
}

extern "C" void httpd_post_finished(void *connection, char *response_uri, uint16_t response_uri_len) {
    (void)connection;
    if (response_uri && !http_post_uri.empty()) {
        strncpy(response_uri, http_post_uri.c_str(), response_uri_len);
    }
}
