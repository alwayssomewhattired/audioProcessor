#include "FFTProcessor.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include "utils.h"

FFTProcessor::FFTProcessor(int chunkSize, int sampleRate)
	: m_chunkSize(chunkSize), m_sampleRate(sampleRate)
{
	m_fftSize = chunkSize / 2 + 1;
	m_realInput = fftw_alloc_real(chunkSize);
	m_complexOutput = fftw_alloc_complex(m_fftSize);
	m_plan = fftw_plan_dft_r2c_1d(chunkSize, m_realInput, m_complexOutput, FFTW_MEASURE);
}

FFTProcessor::~FFTProcessor() {
	if (m_plan) {
		fftw_destroy_plan(m_plan);
	}
	if (m_realInput) {
		fftw_free(m_realInput);
	}
	if (m_complexOutput) {
		fftw_free(m_complexOutput);
	}
}

void FFTProcessor::compute(const std::vector<double>& audioData, double targetFrequency, bool resetSamples) {
	m_magnitudeChunks.clear();
	if (resetSamples) {
		m_sampleStorage.clear();
	}

	int controlNoteBin = static_cast<int>(targetFrequency * m_chunkSize / m_sampleRate);
	int n = audioData.size();
	int numChunks = (n + m_chunkSize - 1) / m_chunkSize;

	for (int chunk = 0; chunk < numChunks; ++chunk) {
		//		THIS CONSOLE OUT SLOWS DOWN. BE CAREFUL
		//std::cout << "Chunk " << chunk << ": controlNote = " << targetFrequency << ", m_fftSize = " << m_fftSize << "\n";

		std::fill(m_realInput, m_realInput + m_chunkSize, 0);
		int start = chunk * m_chunkSize;
		int end = std::min(start + m_chunkSize, n);
		std::copy(audioData.begin() + start, audioData.begin() + end, m_realInput);

		fftw_execute(m_plan);

		std::vector<double> magnitudes(m_fftSize);
		for (int i = 0; i < m_fftSize; ++i) {
			magnitudes[i] = std::sqrt(m_complexOutput[i][0] * m_complexOutput[i][0] +
				m_complexOutput[i][1] * m_complexOutput[i][1]);
			//		THIS CONSOLE OUT SLOWS DOWN. BE CAREFUL
			//std::cout << "Magnitudes Values: " << magnitudes[i] << std::endl;
		}

		m_magnitudeChunks.push_back(std::move(magnitudes));
		//		THIS CONSOLE OUT SLOWS DOWN. BE CAREFUL
		//std::cout << "controlNote: " << controlNote << ", m_fftSize: " << m_fftSize << std::endl;

		if (controlNoteBin >= 0 && controlNoteBin < m_fftSize) {
			double controlMagnitude = m_magnitudeChunks.back()[controlNoteBin];
			if (isProminentPeak(m_magnitudeChunks.back(), controlMagnitude)) {
				std::cout << "Prominent peak found!" << std::endl;
				storeChunkIfProminent(audioData, chunk, controlMagnitude);
			}
			}
			else {
				std::cerr << "Warning: controlNote index (" << controlNoteBin
					<< ") is out of range [0, " << m_fftSize - 1 << "]" << std::endl;
		}
	}
}

//bool FFTProcessor::isProminentPeak(const std::vector<double>& vec, double value) {
//	if (value == 0) return false;
//	for (double v : vec) {
//		if (value < v) return false;
//	}
//	return true;
//}

// PROMINENCE STRENGTH DEMO
bool FFTProcessor::isProminentPeak(const std::vector<double>& vec, double value, double ratio) {
	if (value == 0) return false;

	double maxVal = *std::max_element(vec.begin(), vec.end());

	// Check if value is within ratio% of the peak
	return value >= ratio * maxVal;
}


void FFTProcessor::storeChunkIfProminent(const std::vector<double>& samples, int counter, double magnitude) {
	int start = counter * m_chunkSize;
	int end = std::min(start + m_chunkSize, static_cast<int>(samples.size()));
	for (int i = start; i < end; ++i) {
		double val = clamp(samples[i], -1.0, 1.0);
		m_sampleStorage.push_back(val);
	}
}

const std::vector<std::vector<double>>& FFTProcessor::getMagnitudes() const {
	return m_magnitudeChunks;
}

const std::vector<double>& FFTProcessor::getSampleStorage() const {
	return m_sampleStorage;
}