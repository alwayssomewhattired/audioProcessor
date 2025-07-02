
#define _USE_MATH_DEFINES

#include "AudioFileParse.h"
#include <sndfile.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <iostream>
#include <fstream>
#include <iterator>
#include <cmath>
#include <filesystem>



AudioFileParse::AudioFileParse(const std::string& region)
	: m_region(region), m_sdkInitialized(false) {
	Aws::InitAPI(m_options);
	m_sdkInitialized = true;
}

AudioFileParse::~AudioFileParse() {
	if (m_sdkInitialized) {
		Aws::ShutdownAPI(m_options);
	}
}

const std::vector<double>& AudioFileParse::getAudioData() const {
	return m_audioData;
}

size_t AudioFileParse::size() const {
	return m_audioData.size();
}

std::vector<double> AudioFileParse::readAudioFileAsMono(const std::string& filename) {
	SF_INFO sfInfo;
	SNDFILE* inFile = sf_open(filename.c_str(), SFM_READ, &sfInfo);

	if (!inFile) {
		std::cerr << "Error opening audio file" << filename << std::endl;
		return {};
	}

	size_t numFrames = sfInfo.frames;
	int numChannels = sfInfo.channels;
	std::vector<double> rawData(numFrames * numChannels);

	if (sf_read_double(inFile, rawData.data(), rawData.size()) <= 0) {
		std::cerr << "Error reading audio data from: " << filename << std::endl;
		sf_close(inFile);
		return {};
	}

	sf_close(inFile);

	if (numChannels == 1) {
		return rawData;
	}
	else if (numChannels == 2) {
		m_audioData.resize(numFrames);
		for (size_t i = 0; i < numFrames; ++i) {
			m_audioData[i] = (rawData[2 * i] + rawData[2 * i + 1]) / 2.0;
		}
		return m_audioData;
	}
	else {
		std::cerr << "Unsupported numer of channels: " << numChannels << std::endl;
	}
}


bool AudioFileParse::downloadAudioFileFromS3(const Aws::String& bucketName, const Aws::String& objectKey) {
	Aws::SDKOptions options;
	Aws::InitAPI(options);
	bool success = false;

	{

		// Create S3 client
		Aws::Client::ClientConfiguration config;
		config.region = "us-east-2";
		Aws::S3::S3Client s3_client(config);

		std::cout << "hello" << std::endl;
		// Create request
		Aws::S3::Model::GetObjectRequest request;
		request.SetBucket(bucketName);
		request.SetKey(objectKey);


		// Execute request
		auto outcome = s3_client.GetObject(request);
		if (outcome.IsSuccess()) {
			auto& stream = outcome.GetResult().GetBody();
			std::vector<char> s3Data((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

			std::cout << "Successfully read " << s3Data.size() << " bytes from S3" << std::endl;

			// Save audio to temporary file (libsndfile requires a file)
			std::string tempFile = "temp_audio.mp3";
			std::ofstream outFile(tempFile, std::ios::binary);
			outFile.write(s3Data.data(), s3Data.size());
			success = true;
			outFile.close();

			auto fileSize = std::filesystem::file_size(tempFile);
			std::cout << "Temp file size: " << fileSize << "bytes" << std::endl;
		}
		else {
			std::cerr << "Failed to read file from S3: " << outcome.GetError().GetMessage() << std::endl;
		}
	}
	Aws::ShutdownAPI(options);
	return success;
}

void AudioFileParse::applyHanningWindow() {
	int n = static_cast<int>(m_audioData.size());
	for (int i = 0; i < n; i++) {
		double hannValue = 0.5 * (1 - cos(2 * M_PI * i / (n - 1)));
		m_audioData[i] *= hannValue;
	}
}