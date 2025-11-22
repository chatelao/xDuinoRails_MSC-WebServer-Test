#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <string>
#include <utility>
#include <vector>
#include <map>
#include <ArduinoJson.h>

// Forward declaration for LwIP struct
struct fs_file;

class WebServer {
public:
    static void init();

    // The fs_open_custom function calls this to handle requests
    static int handleCustomOpen(struct fs_file *file, const char *name);

    // Called by fs_close_custom to cleanup
    static void handleCustomClose(struct fs_file *file);

    // Helper to set file data from string
    static int setFileData(struct fs_file *file, const std::string& data);

    // Public handlers
    static std::string apiStatus();
    static std::string apiEcho();

private:
    // JSON helper
    static std::string serializeJson(JsonDocument &doc);

    // Response buffers for open files
    static std::map<struct fs_file*, std::string> responseBuffers;
};

#endif // WEBSERVER_H
