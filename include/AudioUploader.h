#pragma once

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <sndfile.h>
#include <fstream>
#include <iostream>
#include <json/json.h>

#include "WebSocketClient.h"

class AudioUploader {
public:
	AudioUploader(const std::string& bucket, const std::string& region);
	~AudioUploader();

	bool uploadIfReady(const std::vector<double>& samples, size_t sampleThreshold,
		const std::string& outputName, WebSocketClient& ws, const std::string userId);


private:
	Aws::SDKOptions options;
	Aws::Client::ClientConfiguration config;
	std::unique_ptr<Aws::S3::S3Client> s3_client;
	std::string bucketName;
	std::string region;

	bool writeWavFile(const std::vector<double>& samples, const std::string& filename);
	void AudioIdMessage(WebSocketClient& ws, const std::string sampledinfiniteId, const std::string userId);
	void notifyWebSocket(WebSocketClient& ws);
};