
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
		"-------------------------------------------------WELCOME--TO--MY--SOFTWARE-------------------------------------------------"
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



	WebSocketClient ws;
	ws.connect(uri);

	const char* outputName = "output.wav";

	ws.wait_for_connection();

	FFTProcessor fftProcessor(config.chunkSize, config.sampleRate);


	while (fftProcessor.getSampleStorage().size() < config.productDurationSamples) {

		// Create a JSON message
		Json::Value message;
		message["action"] = "sendMessage";
		message["body"] = "run_function";

		// Convert the JSON object to a string
		Json::StreamWriterBuilder writer;
		std::string message_str = Json::writeString(writer, message);
		std::cout << message_str << std::endl;
		ws.send_message(message_str);

		ws.wait_for_condition(); // Wait until 'source_upload' message is received

		std::cout << "WE GOOD BABY" << std::endl;
		std::cout << "note: " << ws.get_control_note() << std::endl;


		AudioFileParse parser;
		parser.downloadAudioFileFromS3(config.bucketName, config.objectKey);
		parser.readAudioFileAsMono("temp_audio.mp3");
		int n = parser.size();
		std::cout << "Fetched audio size: " << n << std::endl;

		int num_chunks = (n + config.chunkSize - 1) / config.chunkSize; // calculate all the number of chunks

		parser.applyHanningWindow();


		fftProcessor.compute(parser.getAudioData(), ws.get_control_note(), false);

		const auto& chunks = fftProcessor.getMagnitudes();
		const std::vector<double>& audio_copy = parser.getAudioData();

		std::cout << "sampleStorage: " << fftProcessor.getSampleStorage().size() << std::endl;

		if (fftProcessor.getSampleStorage().size() >= config.productDurationSamples) {


			AudioUploader uploader(config.bucketName, config.region);
			// TO DO
			// 0.5. COMMIT SUCKAH!
			// 1. SEND UUID FROM HERE TO WS SERVER 
			// 2. SENDMESSAGE HANDLER FUNCTION FOR UUID TO FRONTEND WEBSOCKET SERVER
			// 3. FRONTEND WEBSOCKET SERVER HANDLER FUNCTION FOR UUID TO FRONTEND SERVER

			// maybe
				// ATTATCH CONNECTION ID TO AUDIO CONTROLS BEFORE SENT TO HERE. ONCE HERE, SAVE THE CONNECTION ID TO BE ATTATCHED TO 
				// SAMPLEDINFINITID MESSAGE AS UPLOADIFREADY ARGUMENTS

			uploader.uploadIfReady(fftProcessor.getSampleStorage(), config.productDurationSamples, outputName, ws);
		}
		ws.reset_condition();

	}

		ws.stop();
	std::cout << "FINISHED" << std::endl;
}