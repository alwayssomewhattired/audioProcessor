#pragma once

// UUIDs
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp> // For converting UUID to a string

template <typename T>
T clamp(T value, T min, T max) {
	if (value < min) return min;
	if (value > max) return max;
	return value;
}

static std::string generateUUID() {
	boost::uuids::random_generator gen;
	boost::uuids::uuid uuid = gen();
	return boost::uuids::to_string(uuid) + ".wav";
}