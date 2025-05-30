
#define NOMINMAX

#define _USE_MATH_DEFINES


#include <iostream>
#include <vector>
#include <sndfile.h>
#include <fftw3.h>
#include <fstream>
#include <cmath>

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
#include <cstdlib>
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


	// Generate UUID string (e.g. for filenames)
static std::string generateUUID() {
	boost::uuids::random_generator gen;
	boost::uuids::uuid uuid = gen();
	return boost::uuids::to_string(uuid) + ".wav";
}



/////////////GLOBALS/////////////////

// Product length in samples
int g_productDurationSamples = 96000;

////////////////////////////////////


int main()
{
	/////////////////////////////////////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////

	std::cout <<
		"-------------------------------WELCOME--TO--MY--SOFTWARE-------------------------------"
		<< std::endl;

	std::string uri = "";

	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////

	WebSocketClient ws;
	ws.connect(uri);

	const Aws::String bucketName = "firstdemoby";
	const Aws::String objectKey = "fetch-test.mp3";


	const char* outputName = "output.wav";
	int sampleRate = 44100;
	int channels = 1;
	int format = SF_FORMAT_WAV | SF_FORMAT_PCM_32; // 32 bit pcm wav

	SNDFILE* infiniteFile;
	SF_INFO sf_info;
	sf_info.samplerate = sampleRate;
	sf_info.channels = channels;
	sf_info.format = format;
	int chunk_size = 8192; // Good size for note-like values

	ws.wait_for_connection();

	FFTProcessor fftProcessor(chunk_size, sampleRate);


	while (fftProcessor.getSampleStorage().size() < g_productDurationSamples) {

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
		parser.downloadAudioFileFromS3(bucketName, objectKey);
		parser.readAudioFileAsMono("temp_audio.mp3");
		int n = parser.size();
		std::cout << n << std::endl;

		int num_chunks = (n + chunk_size - 1) / chunk_size; // calculate all the number of chunks

		parser.applyHanningWindow();


		fftProcessor.compute(parser.getAudioData(), ws.get_control_note(), false);

		const auto& chunks = fftProcessor.getMagnitudes();
		const std::vector<double>& audio_copy = parser.getAudioData();

		std::cout << "sampleStorage: " << fftProcessor.getSampleStorage().size() << std::endl;

		if (fftProcessor.getSampleStorage().size() >= g_productDurationSamples) {
			// open wav file for writing
			infiniteFile = sf_open(outputName, SFM_WRITE, &sf_info);
			if (!infiniteFile) {
				std::cerr << "error opening file: " << outputName << std::endl;
				return 1;
			}

			// write data for writing wav file
			sf_count_t framesWritten = sf_writef_double(infiniteFile, fftProcessor.getSampleStorage().data(), fftProcessor.getSampleStorage().size());
			if (framesWritten != fftProcessor.getSampleStorage().size()) {
				std::cerr << "Error writing data: " << sf_strerror(infiniteFile) << std::endl;
			}

			sf_close(infiniteFile);

			// open the file to upload to s3
			std::ifstream file_stream(outputName, std::ios::in | std::ios::binary);
			if (!file_stream) {
				std::cerr << "Error: Could not open " << outputName << " for s3 upload" << std::endl;
				return 1;
			}

			// Initialize the AWS SDK
			Aws::SDKOptions options;
			Aws::InitAPI(options);

			// Create S3 client
			Aws::Client::ClientConfiguration config;
			config.region = "us-east-2";
			Aws::S3::S3Client s3_client(config);

			// productKey (UUID)
			std::string productKey = generateUUID();

			// Create a putObjectRequest
			Aws::S3::Model::PutObjectRequest object_request;
			Aws::String tag_string = "expire="; // Key with empty value
			object_request.SetBucket(bucketName);
			object_request.SetKey(productKey);
			object_request.SetTagging(tag_string);

			// Attatch the file stream to the putObject Request
			auto input_data = Aws::MakeShared<Aws::IOStream>("AllocTag", file_stream.rdbuf());
			object_request.SetBody(input_data);

			// Perform the file upload to S3
			auto put_object_outcome = s3_client.PutObject(object_request);

			if (put_object_outcome.IsSuccess()) {
				std::cout << "Product successfully uploaded to S3!" << std::endl;
				Json::Value message;
				message["action"] = "sendMessage";
				message["body"] = "finish_now";

				// Convert the JSON object to a string
				Json::StreamWriterBuilder writer;
				std::string message_str = Json::writeString(writer, message);
				std::cout << message_str << std::endl;

				ws.send_message(message_str);
				ws.wait_for_condition();
			}
			else {
				std::cerr << "Error uploading product: " << put_object_outcome.GetError().GetMessage() << std::endl;
			}

			Aws::ShutdownAPI(options);

		}

		ws.reset_condition();

	}

	std::cout << "FINISHED" << std::endl;
}