#pragma once
// code adapted from
// https://github.com/Infinideastudio/NEWorld/blob/0.5.0/NEWorld.Game/Universe/World/TerrainGen/Noise.h
// which is a project that I have worked on before

#include <cstdlib>

static constexpr double NoiseScaleX = 64;
static constexpr double NoiseScaleZ = 64;

class Noise {
	static constexpr double Base(int x, int y) {
		auto xx = x + y * 13258953287;
		xx = (xx >> 13) ^ xx;
		return ((xx * (xx * xx * 15731 + 789221) + 1376312589) & 0x7fffffff) / 16777216.0;
	}

	static constexpr double SmoothedNoise(int x, int y) {
		const auto corners = (Base(x - 1, y - 1) + Base(x + 1, y - 1) + Base(x - 1, y + 1) +
			Base(x + 1, y + 1)) / 8.0;
		const auto sides = (Base(x - 1, y) + Base(x + 1, y) + Base(x, y - 1) + Base(x, y + 1)) / 4.0;
		const auto center = Base(x, y);
		return corners + sides + center;
	}

	static constexpr double Interpolate(double a, double b, double x) { return a * (1.0 - x) + b * x; }

	static double InterpolatedNoise(double x, double y) {
		const auto int_X = static_cast<int>(floor(x));
		const auto fractional_X = x - int_X;
		const auto int_Y = static_cast<int>(floor(y));
		const auto fractional_Y = y - int_Y;
		const auto v1 = Base(int_X, int_Y);
		const auto v2 = Base(int_X + 1, int_Y);
		const auto v3 = Base(int_X, int_Y + 1);
		const auto v4 = Base(int_X + 1, int_Y + 1);
		const auto i1 = Interpolate(v1, v2, fractional_X);
		const auto i2 = Interpolate(v3, v4, fractional_X);
		return Interpolate(i1, i2, fractional_Y);
	}

	static double PerlinNoise2D(double x, double y) {
		double total = 0, frequency = 1, amplitude = 1;
		for (auto i = 0; i <= 4; i++) {
			total += InterpolatedNoise(x * frequency, y * frequency) * amplitude;
			frequency *= 2;
			amplitude /= 2.0;
		}
		return total;
	}
public:
	explicit Noise() {
		srand(time(nullptr));
		offsetX = rand();
		offsetZ	= rand();
	}

	int operator() (int x, int z) {
		return static_cast<int>(
			PerlinNoise2D(
				x / NoiseScaleX + 0.125 + offsetX,
				z / NoiseScaleZ + 0.125 + offsetZ
			)) >> 2;
	}
private:
	int offsetX, offsetZ;
};

