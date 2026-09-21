#ifndef GAME_SERVER_AI_EVOLUTION_MANAGER_H
#define GAME_SERVER_AI_EVOLUTION_MANAGER_H

#include "neural_net.h"
#include <base/vmath.h>
#include <vector>
#include <string>
#include <random>

class CGameContext;
class CCharacter;
class CPlayer;

struct SBotAgent
{
	int m_ClientId = -1;
	CNeuralNet m_Net;
	bool m_IsAlive = false;

	float m_SpawnX = 0.0f;
	float m_SpawnY = 0.0f;
	float m_MaxX = 0.0f;
	float m_Fitness = 0.0f;
	int m_Checkpoints = 0;
	int m_LastCheckpoint = -1;
	int m_TicksAlive = 0;

	// Navigation & Exploration
	int m_MaxBfsDistance = 0;
	int m_VisitedTilesCount = 0;
	std::vector<uint8_t> m_vVisitedMap;

	// Watchdog tracking
	vec2 m_LastWatchdogPos = vec2(0, 0);
	int m_StuckTicks = 0;
	int m_FreezeTicks = 0;
	int m_WallHugTicks = 0;
	bool m_DiedStuck = false;
	vec2 m_DeathPos = vec2(0, 0);
};

struct SEvolutionStats
{
	int m_Generation = 1;
	int m_AliveCount = 0;
	int m_TotalBots = 16;
	int m_CurrentGenTicks = 0;
	float m_BestFitnessCurrent = 0.0f;
	float m_BestFitnessAllTime = 0.0f;
	int m_BestBotIdCurrent = -1;
	int m_BestBotIdLastGen = -1;
	float m_MaxDistanceX = 0.0f;
	int m_CheckpointsReached = 0;
	float m_TimeScale = 1.0f;
	bool m_Active = false;

	// Multi-generation stagnation & stuck analysis
	int m_StagnationGenerations = 0;
	float m_LastGenBestProgress = 0.0f;
	vec2 m_LastStuckCentroid = vec2(0, 0);
};

class CEvolutionManager
{
private:
	CGameContext *m_pGameServer = nullptr;
	std::vector<SBotAgent> m_vAgents;
	SEvolutionStats m_Stats;
	std::mt19937 m_Rng;

	CNeuralNet m_AllTimeBestNet;
	bool m_HasAllTimeBest = false;

	// BFS Navigation Grid
	std::vector<int> m_vBfsDistance;
	int m_BfsWidth = 0;
	int m_BfsHeight = 0;
	bool m_BfsComputed = false;

	static constexpr int MAX_GEN_TICKS = 2000; // 40 seconds at 50 tps
	static constexpr int STUCK_LIMIT_TICKS = 150; // 3 seconds without moving > 32px

	void ComputeBfsDistanceMap(int SpawnTileX, int SpawnTileY);
	int GetBfsDistance(vec2 Pos) const;
	vec2 GetBfsProgressDirection(vec2 Pos) const;

	void EvaluateAndEvolve();
	void RespawnAllBots();

public:
	CEvolutionManager();
	~CEvolutionManager() = default;

	void Init(CGameContext *pGameServer);
	void StartEvolution(int PopulationSize = 16);
	void StopEvolution();
	void ResetGeneration();

	void OnTick();
	void OnCharacterPreTick(CCharacter *pCharacter);
	void OnBotDeath(int ClientId);
	void OnBotReachCheckpoint(int ClientId, int Checkpoint);

	bool IsBot(int ClientId) const;
	bool IsActive() const { return m_Stats.m_Active; }
	int BestBotId() const { return m_Stats.m_BestBotIdLastGen; }
	const SEvolutionStats &Stats() const { return m_Stats; }

	bool SaveBest(const char *pFilename);
	bool LoadBest(const char *pFilename);
};

#endif // GAME_SERVER_AI_EVOLUTION_MANAGER_H
