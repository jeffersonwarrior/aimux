#include <crow.h>
#include <iostream>

int main() {
    crow::SimpleApp app;

    // Test routes
    CROW_ROUTE(app, "/")([](const crow::request&, crow::response& res) {
        res.write("Hello from Crow framework!");
        res.end();
    });

    CROW_ROUTE(app, "/health")([]() {
        crow::json::wvalue response;
        response["status"] = "healthy";
        response["framework"] = "crow";
        return response;
    });

    // WebSocket preparation route (will be used in Task 2.3)
    CROW_WEBSOCKET_ROUTE(app, "/ws")
        .onopen([](crow::websocket::connection& conn) {
            std::cout << "WebSocket connection opened" << std::endl;
        })
        .onclose([](crow::websocket::connection& conn) {
            std::cout << "WebSocket connection closed" << std::endl;
        })
        .onmessage([](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
            std::cout << "Received: " << data << std::endl;
            conn.send_text("Echo: " + data);
        });

    std::cout << "Starting Crow-based server on 127.0.0.1:8080" << std::endl;
    app.bindaddr("127.0.0.1").port(8080).multithreaded().run();

    return 0;
}