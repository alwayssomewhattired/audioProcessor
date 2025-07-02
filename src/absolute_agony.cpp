
#define NOMINMAX

#define _USE_MATH_DEFINES

#include <filesystem>

#include <iostream>
#include <vector>
#include <sndfile.h>
#include <fftw3.h>
#include <fstream>
#include <cmath>
#include <cstdlib>

// Unique Key
#include <iomanip>

// UUIDs
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp> // For converting UUID to a string

// websockets (linux)
#include <websocketpp/client.hpp>

// AWS
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/core/utils/Outcome.h>
#include <aws/core/utils/DateTime.h>

// SSL/TLS
#include <websocketpp/config/asio_client.hpp>

#include <string>
#include <memory>
#include <functional>

#include <websocketpp/config/asio.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/transport/asio/security/tls.hpp>

#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>

// JSON for messaging
#include <json/json.h>


// my stuff
#include "WebSocketClient.h"
#include "AudioFileParse.h"
#include "FFTProcessor.h"
#include "AudioUploader.h"

struct Config {
	std::string region;
	std::string bucketName;
	std::string objectKey;
	std::string outputFile;
	int chunkSize;
	int sampleRate;
	int channels;
	int productDurationSamples;

};


int main()
{


	std::cout <<
"---------------------------------------------------WELCOME--TO--MY--SOFTWARE---------------------------------------------------"
		<< std::endl;


	Config config = {
	"us-east-2",           // region
	"firstdemoby",         // bucketName
	"fetch-test.mp3",      // objectKey
	"output.wav",          // outputFile
	8192,                  // chunkSize
	44100,                 // sampleRate
	1,					   // channels
	96000,				   // productDurationSamples
	};

	const char* uri = std::getenv("MY_WEBSOCKET_URI");
	if (!uri) {
		std::cerr << "MY_WEBSOCKET_URI is not set" << std::endl;
		return 1;
	}

	const char* outputName = "output.wav";

	WebSocketClient ws;
	ws.connect(uri);
	ws.wait_for_connection();
	
	std::string my_control_source = "";
	const char* control_source = std::getenv("MY_CONTROL_SOURCE");
	if (control_source == nullptr) {
		std::cerr << "MY_CONTROL_SOURCE not set." << std::endl;
		return 0;
	}
	std::cout << "CONTROL_SOURCE = " << control_source << std::endl;
	my_control_source = std::string(control_source);

	std::string my_user_id = "";
	const char* user_id = std::getenv("MY_USER_ID");
	if (user_id != nullptr) {
		std::cout << "MY_USER_ID: " << user_id << std::endl;
		my_user_id = std::string(user_id);
		std::cout << "my_user_id: " << my_user_id << std::endl;
	}
	else {
		std::cerr << "MY_USER_ID not set." << std::endl;
	}

	Json::Value message;
	message["action"] = "sendMessage";
	message["body"] = "user_id";
	message["user_id"] = my_user_id;

	Json::StreamWriterBuilder writer;
	std::string message_str = Json::writeString(writer, message);
	std::cout << "Sending to API: " << message_str << std::endl;
	ws.send_message(message_str);

	FFTProcessor fftProcessor(config.chunkSize, config.sampleRate);


	double my_control_note = 0.0;
	const char* control_note = std::getenv("MY_CONTROL_NOTE");
	if (control_note != nullptr) {
		std::cout << "MY_CONTROL_NOTE: " << control_note << std::endl;
		my_control_note = std::atof(control_note);
	}
	else {
		std::cerr << "MY_CONTROL_NOTE not set." << std::endl;
	}
	////////////////////////////////////////													////////////////////////////////////////
	////////////////////////////////////////													////////////////////////////////////////

	//						USER_UPLOAD_PATH

	if (my_control_source == "user_source") {
		std::cout << "user_source path taken!" << std::endl;
		// PUT LOGIC HERE .....
		// SEND "source_ready" TO WEBSOCKET SERVER ...
		// PAUSE EXECUTION UNTIL AUDIO RECEIVED
		// PARSE AUDIO
		// SEND FINISHED AUDIO TO WEBSOCKET SERVER
		// CLEANUP/FINISH
		Json::Value message;
		message["action"] = "sendMessage";
		message["body"] = "source_ready";
		message["user_id"] = my_user_id;
		message["source_ready"] = "source_ready";
		Json::StreamWriterBuilder writer;
		std::string message_str = Json::writeString(writer, message);
		std::cout << "Sending to API: " << message_str << std::endl;
		ws.send_message(message_str);

		ws.wait_for_condition();
		// parse audio
		AudioFileParse parser;
		parser.downloadAudioFileFromS3(config.bucketName, ws.get_audio_source_name());

		parser.readAudioFileAsMono("temp_audio.mp3");
		int n = parser.size();
		std::cout << "User audio size: " << n << std::endl;

		int num_chunks = (n + config.chunkSize - 1) / config.chunkSize;

		parser.applyHanningWindow();

		std::cout << "User audio size after Hanning: " << parser.size() << std::endl;


		fftProcessor.compute(parser.getAudioData(), my_control_note, false);

		const auto& chunks = fftProcessor.getMagnitudes();
		const std::vector<double>& audio_copy = parser.getAudioData();

		std::cout << "sampleStorage: " << fftProcessor.getSampleStorage().size() << std::endl;

		AudioUploader uploader(config.bucketName, config.region);

		uploader.uploadIfReady(fftProcessor.getSampleStorage(), config.productDurationSamples, outputName, ws, my_user_id);

		ws.reset_condition();
		ws.stop();
		std::cout << "FINISHED" << std::endl;
		return 0;
	}


	////////////////////////////////////////													////////////////////////////////////////
	////////////////////////////////////////													////////////////////////////////////////

	//						RANDOM_SOURCE_PATH
	while (fftProcessor.getSampleStorage().size() < config.productDurationSamples) {


		// Create a JSON message.
		Json::Value message;
		message["action"] = "sendMessage";
		message["body"] = "run_function";

		// Convert the JSON object to a string
		Json::StreamWriterBuilder writer;
		std::string message_str = Json::writeString(writer, message);
		std::cout << "Sending to API: " << message_str << std::endl;
		ws.send_message(message_str);

		ws.wait_for_condition(); // Wait until 'source_upload' message is received

		std::cout << "WE GOOD BABY" << std::endl;


		AudioFileParse parser;
		parser.downloadAudioFileFromS3(config.bucketName, ws.get_audio_source_name());
		parser.readAudioFileAsMono("temp_audio.mp3");
		int n = parser.size();
		std::cout << "Fetched audio size: " << n << std::endl;

		int num_chunks = (n + config.chunkSize - 1) / config.chunkSize; // calculate all the number of chunks

		parser.applyHanningWindow();


		fftProcessor.compute(parser.getAudioData(), my_control_note, false);

		const auto& chunks = fftProcessor.getMagnitudes();
		const std::vector<double>& audio_copy = parser.getAudioData();

		std::cout << "sampleStorage: " << fftProcessor.getSampleStorage().size() << std::endl;

		if (fftProcessor.getSampleStorage().size() >= config.productDurationSamples) {

			AudioUploader uploader(config.bucketName, config.region);

			uploader.uploadIfReady(fftProcessor.getSampleStorage(), config.productDurationSamples, outputName, ws, my_user_id);
		}
		ws.reset_condition();

	}

		ws.stop();
	std::cout << "FINISHED" << std::endl;
}