
#include "WebSocketClient.h"
#include <iostream>
#include <sstream>
#include <json/json.h>
#include <boost/asio/ssl/context.hpp>

WebSocketClient::WebSocketClient()
    : connected(false), task_done(false), targetFrequency(400),
    c(), hdl_global(), mtx(), cv(), condition_met(false)
{
    c.init_asio();
    c.set_tls_init_handler([this](connection_hdl hdl) { return on_tls_init(hdl); });
    c.set_message_handler([this](connection_hdl hdl, client::message_ptr msg) { on_message(hdl, msg); });
    c.set_open_handler([this](connection_hdl hdl) { on_open(hdl); });
    c.set_fail_handler([this](connection_hdl hdl) { on_fail(hdl); });
}

WebSocketClient::~WebSocketClient() {
    if (websocket_thread.joinable()) {
        websocket_thread.join();
    }
}

bool WebSocketClient::connect(const std::string& uri) {
    websocketpp::lib::error_code ec;
    auto con = c.get_connection(uri, ec);

    if (ec) {
        std::cerr << "Connection failed: " << ec.message() << std::endl;
        return false;
    }

    c.connect(con);

    websocket_thread = std::thread([this]() { c.run(); });

    return true;
}

void WebSocketClient::wait_for_connection() {
    while (!connected) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void WebSocketClient::wait_for_condition() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [this] { return condition_met; });
}

void WebSocketClient::reset_condition() {
    std::lock_guard<std::mutex> lock(mtx);
    condition_met = false;
}

bool WebSocketClient::send_message(const std::string& message) {
    if (!connected) {
        std::cerr << "Cannot send message: not connected." << std::endl;
        return false;
    }
    websocketpp::lib::error_code ec;
    c.send(hdl_global, message, websocketpp::frame::opcode::text, ec);
    if (ec) {
        std::cerr << "Send failed: " << ec.message() << std::endl;
        return false;
    }
    task_done = true;
    return true;
}

void WebSocketClient::stop() {
    task_done = true;
    c.stop();
    if (websocket_thread.joinable()) {
        websocket_thread.join();
    }
}

std::shared_ptr<boost::asio::ssl::context> WebSocketClient::on_tls_init(connection_hdl) {
    auto ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);
    try {
        ctx->set_options(boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::no_sslv3 |
            boost::asio::ssl::context::single_dh_use);
        ctx->set_verify_mode(boost::asio::ssl::verify_none);
    }
    catch (const std::exception& e) {
        std::cerr << "Error in TLS context initialization: " << e.what() << std::endl;
    }
    return ctx;
}

void WebSocketClient::on_message(connection_hdl, client::message_ptr msg) {
    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errs;
    std::istringstream s(msg->get_payload());

    if (Json::parseFromStream(reader, s, &root, &errs)) {
        std::string received_message = root.get("message", "").asString();
        std::cout << "Message: " << received_message << std::endl;

        if (received_message == "source_upload") {
            std::cout << "Source has been uploaded!" << std::endl;
            {
                std::lock_guard<std::mutex> lock(mtx);
                condition_met = true;
            }
            cv.notify_one();
        }
        else if (received_message == "poverty_stricken") {
            std::cout << "Controls have been received" << std::endl;
            std::string noteControl = root.get("note", "").asString();
            double noteNum = std::stoi(noteControl);
            std::cout << "note: " << noteNum << std::endl;
            targetFrequency = noteNum;
        }
    }
    else {
        std::cerr << "Failed to parse JSON: " << errs << std::endl;
    }
}

void WebSocketClient::on_open(connection_hdl hdl) {
    std::cout << "Connected to server!" << std::endl;
    hdl_global = hdl;
    connected = true;
}

void WebSocketClient::on_fail(connection_hdl) {
    std::cout << "Connection failed!" << std::endl;
}
