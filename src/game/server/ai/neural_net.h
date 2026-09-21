#ifndef GAME_SERVER_AI_NEURAL_NET_H
#define GAME_SERVER_AI_NEURAL_NET_H

#include <base/vmath.h>
#include <cmath>
#include <vector>
#include <random>
#include <string>
#include <fstream>
#include <algorithm>

class CNeuralNet
{
public:
	static constexpr int INPUT_SIZE = 27;
	static constexpr int HIDDEN1_SIZE = 32;
	static constexpr int HIDDEN2_SIZE = 16;
	static constexpr int OUTPUT_SIZE = 5;

	// Layer 1: Inputs -> Hidden1
	std::vector<float> m_vW1; // INPUT_SIZE * HIDDEN1_SIZE
	std::vector<float> m_vB1; // HIDDEN1_SIZE

	// Layer 2: Hidden1 -> Hidden2
	std::vector<float> m_vW2; // HIDDEN1_SIZE * HIDDEN2_SIZE
	std::vector<float> m_vB2; // HIDDEN2_SIZE

	// Layer 3: Hidden2 -> Output
	std::vector<float> m_vW3; // HIDDEN2_SIZE * OUTPUT_SIZE
	std::vector<float> m_vB3; // OUTPUT_SIZE

	struct SOutput
	{
		bool m_MoveLeft;
		bool m_MoveRight;
		bool m_Jump;
		bool m_Hook;
		float m_AimAngle; // In radians [-pi, pi]
	};

	CNeuralNet()
	{
		m_vW1.resize(INPUT_SIZE * HIDDEN1_SIZE, 0.0f);
		m_vB1.resize(HIDDEN1_SIZE, 0.0f);

		m_vW2.resize(HIDDEN1_SIZE * HIDDEN2_SIZE, 0.0f);
		m_vB2.resize(HIDDEN2_SIZE, 0.0f);

		m_vW3.resize(HIDDEN2_SIZE * OUTPUT_SIZE, 0.0f);
		m_vB3.resize(OUTPUT_SIZE, 0.0f);
	}

	void InitRandom(std::mt19937 &Rng)
	{
		// Xavier / He initialization
		float StdDev1 = std::sqrt(2.0f / INPUT_SIZE);
		std::normal_distribution<float> Dist1(0.0f, StdDev1);
		for(auto &W : m_vW1) W = Dist1(Rng);
		for(auto &B : m_vB1) B = 0.0f;

		float StdDev2 = std::sqrt(2.0f / HIDDEN1_SIZE);
		std::normal_distribution<float> Dist2(0.0f, StdDev2);
		for(auto &W : m_vW2) W = Dist2(Rng);
		for(auto &B : m_vB2) B = 0.0f;

		float StdDev3 = std::sqrt(2.0f / HIDDEN2_SIZE);
		std::normal_distribution<float> Dist3(0.0f, StdDev3);
		for(auto &W : m_vW3) W = Dist3(Rng);
		for(auto &B : m_vB3) B = 0.0f;
	}

	SOutput Forward(const float *pInputs) const
	{
		float aH1[HIDDEN1_SIZE];
		for(int j = 0; j < HIDDEN1_SIZE; j++)
		{
			float Sum = m_vB1[j];
			for(int i = 0; i < INPUT_SIZE; i++)
			{
				Sum += pInputs[i] * m_vW1[i * HIDDEN1_SIZE + j];
			}
			aH1[j] = std::tanh(Sum);
		}

		float aH2[HIDDEN2_SIZE];
		for(int j = 0; j < HIDDEN2_SIZE; j++)
		{
			float Sum = m_vB2[j];
			for(int i = 0; i < HIDDEN1_SIZE; i++)
			{
				Sum += aH1[i] * m_vW2[i * HIDDEN2_SIZE + j];
			}
			aH2[j] = std::tanh(Sum);
		}

		float aOut[OUTPUT_SIZE];
		for(int j = 0; j < OUTPUT_SIZE; j++)
		{
			float Sum = m_vB3[j];
			for(int i = 0; i < HIDDEN2_SIZE; i++)
			{
				Sum += aH2[i] * m_vW3[i * OUTPUT_SIZE + j];
			}
			aOut[j] = std::tanh(Sum);
		}

		SOutput Out;
		// tanh output is in [-1, 1]
		Out.m_MoveLeft = aOut[0] > 0.0f;
		Out.m_MoveRight = aOut[1] > 0.0f;
		Out.m_Jump = aOut[2] > 0.0f;
		Out.m_Hook = aOut[3] > 0.0f;
		Out.m_AimAngle = aOut[4] * 3.1415926535f; // Map to [-pi, pi]

		return Out;
	}

	void Mutate(std::mt19937 &Rng, float MutationRate = 0.10f, float MutationStrength = 0.15f)
	{
		std::uniform_real_distribution<float> UniDist(0.0f, 1.0f);
		std::normal_distribution<float> NormDist(0.0f, MutationStrength);

		auto MutateVec = [&](std::vector<float> &Vec) {
			for(auto &Val : Vec)
			{
				if(UniDist(Rng) < MutationRate)
				{
					Val += NormDist(Rng);
					// Clamp weights to avoid explosion
					Val = std::clamp(Val, -5.0f, 5.0f);
				}
			}
		};

		MutateVec(m_vW1);
		MutateVec(m_vB1);
		MutateVec(m_vW2);
		MutateVec(m_vB2);
		MutateVec(m_vW3);
		MutateVec(m_vB3);
	}

	bool Save(const char *pFilename) const
	{
		std::ofstream File(pFilename, std::ios::binary);
		if(!File.is_open())
			return false;

		int Magic = 0x44444149; // "DDAI"
		int Version = 1;
		File.write(reinterpret_cast<const char *>(&Magic), sizeof(Magic));
		File.write(reinterpret_cast<const char *>(&Version), sizeof(Version));

		auto WriteVec = [&](const std::vector<float> &Vec) {
			size_t Size = Vec.size();
			File.write(reinterpret_cast<const char *>(&Size), sizeof(Size));
			File.write(reinterpret_cast<const char *>(Vec.data()), Size * sizeof(float));
		};

		WriteVec(m_vW1);
		WriteVec(m_vB1);
		WriteVec(m_vW2);
		WriteVec(m_vB2);
		WriteVec(m_vW3);
		WriteVec(m_vB3);

		return File.good();
	}

	bool Load(const char *pFilename)
	{
		std::ifstream File(pFilename, std::ios::binary);
		if(!File.is_open())
			return false;

		int Magic = 0;
		int Version = 0;
		File.read(reinterpret_cast<char *>(&Magic), sizeof(Magic));
		File.read(reinterpret_cast<char *>(&Version), sizeof(Version));

		if(Magic != 0x44444149 || Version != 1)
			return false;

		auto ReadVec = [&](std::vector<float> &Vec) -> bool {
			size_t Size = 0;
			File.read(reinterpret_cast<char *>(&Size), sizeof(Size));
			if(Size != Vec.size()) return false;
			File.read(reinterpret_cast<char *>(Vec.data()), Size * sizeof(float));
			return true;
		};

		if(!ReadVec(m_vW1) || !ReadVec(m_vB1) ||
		   !ReadVec(m_vW2) || !ReadVec(m_vB2) ||
		   !ReadVec(m_vW3) || !ReadVec(m_vB3))
		{
			return false;
		}

		return File.good();
	}
};

#endif // GAME_SERVER_AI_NEURAL_NET_H
