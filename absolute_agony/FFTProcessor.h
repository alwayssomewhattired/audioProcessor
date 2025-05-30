#pragma once

#include <vector>
#include <fftw3.h>

class FFTProcessor {
public:
	FFTProcessor(int chunkSize, int sampleRate);
	~FFTProcessor();

	void compute(const std::vector<double>& audioData, int controlNote, bool resetSamples = true);
	const std::vector<std::vector<double>>& getMagnitudes() const;
	const std::vector<double>& getSampleStorage() const;

private:
	bool isProminentPeak(const std::vector<double>& vec, double value);
	void storeChunkIfProminent(const std::vector<double>& samples, int counter, double magnitude);

	int m_chunkSize;
	int m_sampleRate;
	int m_fftSize;

	double* m_realInput;
	fftw_complex* m_complexOutput;
	fftw_plan m_plan;

	std::vector<std::vector<double>> m_magnitudeChunks;
	std::vector<double> m_sampleStorage;
};
