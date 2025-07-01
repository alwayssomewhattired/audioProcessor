#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <sndfile.h>
#include <fstream>
#include <iostream>
#include <json/json.h>
#include <cstdlib>

#include "AudioUploader.h"
#include "WebSocketClient.h"
#include "utils.h"

#include <thread>
#include <chrono>

AudioUploader::AudioUploader(const std::string& bucket, const std::string& region)
: bucketName(bucket) {
	options = Aws::SDKOptions{};
	Aws::InitAPI(options);
	config.region = region;
	s3_client = std::make_unique<Aws::S3::S3Client>(config);
}

AudioUploader::~AudioUploader() {
	Aws::ShutdownAPI(options);
}

bool AudioUploader::uploadIfReady(const std::vector<double>& samples, size_t sampleThreshold, const std::string& outputName, WebSocketClient& ws, const std::string userId) {
	if (samples.size() < sampleThreshold) {
		std::cout << "sampledinfinite did not meet required length" << std::endl;
		return false;
	}

	if (!writeWavFile(samples, outputName)) {
		std::cout << "Could not write to WavFile" << std::endl;
		return false;
	}

	auto input_data = Aws::MakeShared<Aws::FStream>("AllocTag", outputName, std::ios_base::in | std::ios_base::binary);
	if (!input_data->good()) {
		std::cerr << "Error: Could not open " << outputName << " for S3 upload" << std::endl;
		return false;
	}

	input_data->seekg(0, std::ios::end);
	std::streampos fileSize = input_data->tellg();
	input_data->seekg(0, std::ios::beg);
	std::cout << "Uploading file size: " << fileSize << " bytes" << std::endl;


	std::string productKey = generateUUID();

	std::cout << "sampledinfinite: " << productKey << std::endl;

	Aws::S3::Model::PutObjectRequest object_request;
	object_request.SetBucket(bucketName);
	object_request.SetKey(productKey);
	object_request.SetTagging("expire=");
	object_request.SetBody(input_data);

	auto outcome = s3_client->PutObject(object_request);
	if (outcome.IsSuccess()) {
		std::cout << "Product successfully uploaded to S3!" << std::endl;
		std::cout << "in uploadifready: " << userId << std::endl;
		AudioIdMessage(ws, productKey, userId);
		std::cout << "after AudioIdMessage" << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));  // sleep 2000 ms
		notifyWebSocket(ws);
		std::cout << "After notifyWebSocket" << std::endl;
		input_data.reset();
		return true;
	}
	else {
		std::cerr << "Error uploading product: " << outcome.GetError().GetMessage() << std::endl;
		return false;
	}
}

bool AudioUploader::writeWavFile(const std::vector<double>& samples, const std::string& filename) {
	SF_INFO sf_info = { 0 };
	sf_info.channels = 1;
	sf_info.samplerate = 44100;
	sf_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;


	SNDFILE* file = sf_open(filename.c_str(), SFM_WRITE, &sf_info);
	if (!file) {
		std::cerr << "Error opening file: " << filename << std::endl;
		return false;
	}

	sf_count_t framesWritten = sf_writef_double(file, samples.data(), samples.size());
	if (framesWritten != static_cast<sf_count_t>(samples.size())) {
		std::cerr << "Error writing data: " << sf_strerror(file) << std::endl;
	}

	sf_close(file);
	return true;
}

void AudioUploader::AudioIdMessage(WebSocketClient& ws, std::string sampledinfiniteId, std::string userId) {
	std::cout << "Inside AudioIdMessage" << std::endl;
	Json::Value message;
	message["action"] = "sendMessage";
	message["body"] = "sampledinfinite";
	message["sampledinfiniteId"] = sampledinfiniteId;
	message["userId"] = userId;
	std::cout << "in audioidmessage: " << userId << std::endl;
	std::cout << "in uploadifready(message): " << message["userId"] << std::endl;

	Json::StreamWriterBuilder writer;
	std::string message_str = Json::writeString(writer, message);
	std::cout << message_str << std::endl;

	ws.send_message(message_str);
}

void AudioUploader::notifyWebSocket(WebSocketClient& ws) {
	std::cout << "inside notifyWebSocket" << std::endl;
	Json::Value message;
	message["action"] = "sendMessage";
	message["body"] = "finish_now";

	Json::StreamWriterBuilder writer;
	std::string message_str = Json::writeString(writer, message);
	std::cout << message_str << std::endl;

	ws.send_message(message_str);
	ws.wait_for_condition();
}