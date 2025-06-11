#pragma once


#include <boost/asio/ssl/context.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>


#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>

#include <json/json.h>

class WebSocketClient {
public:
	using client = websocketpp::client < websocketpp::config::asio_tls_client>;
	using connection_hdl = websocketpp::connection_hdl;

	WebSocketClient();
	~WebSocketClient();

	bool connect(const std::string& uri);
	void wait_for_connection();
	void wait_for_condition();
	void reset_condition();
	bool send_message(const std::string& message);
	void stop();

private:
	std::shared_ptr<boost::asio::ssl::context> on_tls_init(connection_hdl);
	void on_message(connection_hdl, client::message_ptr);
	void on_open(connection_hdl);
	void on_fail(connection_hdl);

	std::atomic<bool> connected;
	std::atomic<bool> task_done;

	client c;
	connection_hdl hdl_global;

	std::mutex mtx;
	std::condition_variable cv;
	bool condition_met;

	std::thread websocket_thread;
};