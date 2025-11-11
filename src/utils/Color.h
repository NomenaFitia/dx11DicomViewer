#pragma once
#include <random>
#include <array>

struct Color3f { float r{ 1 }, g{ 1 }, b{ 1 }; };

/*
namespace ColorUtil {
	inline Color3f RandomPastel(uint32_t seed)
	{
		std::mt19937 rng(seed);
		std::uniform_real_distribution<float> u(0.4f, 0.95f);
		float r = u(rng), g = u(rng), b = u(rng);
		// Normaliser pour éviter les composantes trop proches
		float maxc = std::max({ r,g,b });
		float k = 0.9f / maxc;
		return { r * k, g * k, b * k };
	}
}
*/