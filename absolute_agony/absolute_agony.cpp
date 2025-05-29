



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


	// Generate UUID string (e.g. for filenames)
static std::string generateUUID() {
	boost::uuids::random_generator gen;
	boost::uuids::uuid uuid = gen();
	return boost::uuids::to_string(uuid) + ".wav";
}

// Product length in samples
int g_productDurationSamples = 96000;

std::vector<double> sampleStorage;

// create a function for finding and storing prominent frequency samples
bool isGreaterThanAll(std::vector<double>& vec, double value, int counter,
	std::vector<double>& samples, int CHUNK_SIZE) {
	for (double element : vec) {
		if (value < element || value == 0) {
			return false;
		}
	}
	int CHUNK_SLICE_START = counter * CHUNK_SIZE;
	int CHUNK_SLICE_END = CHUNK_SLICE_START + CHUNK_SIZE;
	//sampleStorage.reserve(5295743); // This literally does nothing lol
	//store and normalize samples
	for (int i = CHUNK_SLICE_START; i < CHUNK_SLICE_END; ++i) {
		if (samples[i] < -1) {
			samples[i] = -1;
		}
		else if (samples[i] > 1) {
			samples[i] = 1;
		}
		//sampleStorage.emplace_back(samples[i]); // This might be breaking					I don't know
		sampleStorage.push_back(samples[i]); // This might fix it							which is better...
		//std::cout << "stored samples!!!: " << samples[i] << std::endl;
	}
	return true;
}

int main()
{
	/////////////////////////////////////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////////////////

	std::cout <<
		"---------------------WELCOME--TO--MY--SOFTWARE---------------------"
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

	while (sampleStorage.size() < g_productDurationSamples) {
		std::cout << sampleStorage.size() << std::endl;

		if (sampleStorage.size() >= g_productDurationSamples) {
			Json::Value message;
			message["action"] = "sendMessage";
			message["body"] = "finish_now";

			// Convert the JSON object to a string
			Json::StreamWriterBuilder writer;
			std::string message_str = Json::writeString(writer, message);
			std::cout << message_str << std::endl;

			ws.send_message(message_str);

		}

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

		// allocate memory for the real input and complex output of this chunk
		int fft_size = chunk_size / 2 + 1; // size of complex output for this chunk
		double* real_input = fftw_alloc_real(chunk_size); // real input array
		if (real_input == nullptr) { // handle allocation error
			std::cerr << "Failed to allocate real memory" << std::endl;
			exit(1);
		}
		fftw_complex* complex_output = fftw_alloc_complex(fft_size); // complex output array
		if (complex_output == nullptr) { // handle allocation error
			std::cerr << "Failed to allocated complex memory" << std::endl;
		}

		// create fftw plan for this chunk
		fftw_plan plan = fftw_plan_dft_r2c_1d(chunk_size, real_input, complex_output, FFTW_MEASURE);

		// create vector for magnitude storage
		std::vector<double> magnitudes(chunk_size);

		int counter = -1;

		// loop over each chunk
		for (int chunk = 0; chunk < num_chunks - 1; ++chunk) { // num_chuks is strange
			++counter;
			std::cout << "chunk: " << chunk << std::endl;
			std::cout << "num chunk " << num_chunks << std::endl;
			//std::cout << "counter: " << counter << std::endl;
			// Determine the starting and ending index for the chunk
			int start = chunk * chunk_size;
			int end = std::min<int>(start + chunk_size, n); // added <int> identifier


			// copy the audio source data into the real_input array
			std::fill(real_input, real_input + chunk_size, 0); // Zero out the input
	
			const std::vector<double>& audio = parser.getAudioData();
			std::vector<double> audio_copy = parser.getAudioData();
			std::copy(audio.begin() + start, audio.begin() + end, real_input);

			//execute the fft
			fftw_execute(plan);

			// open file to store analysis
			//if (outputFile.is_open()) {
					// process the complex output
			for (int i = 0; i < fft_size; ++i) {
				//std::cout << "samples: " << i << " " << audio_data[i + chunk_size] << std::endl;
				double freq = static_cast<double>(i) * sampleRate / chunk_size;
				magnitudes[i] = std::sqrt(complex_output[i][0] * complex_output[i][0]
					+ complex_output[i][1] * complex_output[i][1]);
				// push freq/bins/magnitude to text file
			/*	outputFile << "bin " << i << ": Frequency = " << freq << " Hz, magnitude = "
					<< magnitudes[i] << "\n";*/
			}

			isGreaterThanAll(magnitudes, magnitudes[ws.get_control_note()], counter, audio_copy, chunk_size);

		}
		// I wonder if this is supposed to be in the for loop.
		//outputFile.close();

	/*	if (magFile.is_open()) {
			for (double element : sampleStorage) {
				magFile << "sample: " << element << std::endl;
			}
		}
		magFile.close();*/


		if (sampleStorage.size() >= g_productDurationSamples) {
			// open wav file for writing
			infiniteFile = sf_open(outputName, SFM_WRITE, &sf_info);
			if (!infiniteFile) {
				std::cerr << "error opening file: " << outputName << std::endl;
				return 1;
			}

			// write data for writing wav file
			sf_count_t framesWritten = sf_writef_double(infiniteFile, sampleStorage.data(), sampleStorage.size()); // might need 'sizeof' for size
			if (framesWritten != sampleStorage.size()) {
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

		//clean up
		fftw_destroy_plan(plan);
		fftw_free(real_input);
		fftw_free(complex_output);

		ws.reset_condition();

	}

	std::cout << "FINISHED" << std::endl;
}