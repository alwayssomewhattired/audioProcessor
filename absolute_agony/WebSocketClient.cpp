#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

#include <boost/asio/ssl/context.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <json/json.h>

#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>

class WebSocketClient {
public:
	using client = websocketpp::client<websocketpp::config::asio_tls_client>;
	using connection_hdl = websocketpp::connection_hdl;

	WebSocketClient()
		: connected(false), task_done(false), controlNote(20),
		c(), hdl_global(), mtx(), cv(), condition_met(false)
	{
		// Initialize ASIO
		c.init_asio();

		// Register handlers
		c.set_tls_init_handler([this](connection_hdl hdl) { return on_tls_init(hdl); });
		c.set_message_handler([this](connection_hdl hdl, client::message_ptr msg) { on_message(hdl, msg); });
		c.set_open_handler([this](connection_hdl hdl) { on_open(hdl); });
		c.set_fail_handler([this](connection_hdl hdl) { on_fail(hdl); });
	}

	~WebSocketClient() {
		if (websocket_thread.joinable()) {
			websocket_thread.join();
		}
	}

	// Connect to server and start running the client loop on a seperate thread
	bool connnect(const std::string& uri) {
		websocketpp::lib::error_code ec;
		auto con = c.get_connection(uri, ec);
		if (ec) {
			std::cerr << "Connection failed: " << ec.message() << std::endl;
			return false;
		}

		c.connect(con);

		websocket_thread = std::thread([this]() {
			c.run();
			});

		return true;
	}

	// Wait until connection is established
	void wait_for_connection() {
		while (!connected) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	// Wait for condition (e.g. source_upload message)
	void wait_for_condition() {
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait(lock, [this] { return condition_met; });
	}

	// Accessor for controlNote
	int get_control_note() const {
		return controlNote;
	}

	// Generate UUID string (e.g. for filenames)
	static std::string generateUUID() {
		boost::uuids::random_generator gen;
		boost::uuids::uuid uuid = gen();
		return boost::uuids::to_string(uuid) + ".wav";
	}

	// Mark task done and stop the client loop
	void stop() {
		task_done = true;
		c.stop();
		if (websocket_thread.joinable()) {
			websocket_thread.join();
		}
	}

private:

	std::shared_ptr<boost::asio::ssl::context> on_tls_init(connection_hdl) {
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

	void on_message(connection_hdl, client::message_ptr msg) {
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
				int noteNum = std::stoi(noteControl);
				std::cout << "note: " << noteNum << std::endl;
				controlNote = noteNum;
			}
		}
		else {
			std::cerr << "Failed to parse JSON: " << errs << std::endl;
		}
	}

	void on_open(connection_hdl hdl) {
		std::cout << "Connected to server!" << std::endl;
		hdl_global = hdl;
		connected = true;
	}

	void on_fail(connection_hdl) {
		std::cout << "Cnnection failed!" << std::endl;
	}

	std::atomic<bool> connected;
	std::atomic<bool> task_done;

	client c;
	connection_hdl hdl_global;

	std::mutex mtx;
	std::condition_variable cv;
	bool condition_met;

	int controlNote;

	std::thread websocket_thread;
};
