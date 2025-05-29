#pragma once
#include <string>
#include <vector>
#include <aws/core/Aws.h>

class AudioFileParse {
public:
	AudioFileParse(const std::string& region = "us-east-2");
	~AudioFileParse();

	std::vector<double> readAudioFileAsMono(const std::string& filename);
	bool downloadAudioFileFromS3(const Aws::String& bucketName, const Aws::String& objectKey);

	void applyHanningWindow();

	const std::vector<double>& getAudioData() const;

	size_t size() const;

private:
	Aws::SDKOptions m_options;
	std::string m_region;
	bool m_sdkInitialized;
	std::vector<double> m_audioData;
};