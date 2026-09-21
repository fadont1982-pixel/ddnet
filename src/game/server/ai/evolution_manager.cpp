#include "evolution_manager.h"

#include <game/server/gamecontext.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/mapitems.h>
#include <engine/server.h>
#include <engine/shared/config.h>

#include <algorithm>
#include <cmath>
#include <queue>
#include <utility>

CEvolutionManager::CEvolutionManager()
{
	std::random_device Rd;
	m_Rng.seed(Rd());
}

void CEvolutionManager::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
}

void CEvolutionManager::ComputeBfsDistanceMap(int SpawnTileX, int SpawnTileY)
{
	if(!m_pGameServer || !m_pGameServer->Collision())
		return;

	CCollision *pCol = m_pGameServer->Collision();
	m_BfsWidth = pCol->GetWidth();
	m_BfsHeight = pCol->GetHeight();
	if(m_BfsWidth <= 0 || m_BfsHeight <= 0)
		return;

	int TotalTiles = m_BfsWidth * m_BfsHeight;
	m_vBfsDistance.assign(TotalTiles, -1);

	SpawnTileX = std::clamp(SpawnTileX, 0, m_BfsWidth - 1);
	SpawnTileY = std::clamp(SpawnTileY, 0, m_BfsHeight - 1);

	std::queue<std::pair<int, int>> Q;
	m_vBfsDistance[SpawnTileY * m_BfsWidth + SpawnTileX] = 0;
	Q.push({SpawnTileX, SpawnTileY});

	const int aDx[4] = {1, -1, 0, 0};
	const int aDy[4] = {0, 0, 1, -1};

	while(!Q.empty())
	{
		auto Cur = Q.front();
		Q.pop();
		int cx = Cur.first;
		int cy = Cur.second;
		int CurDist = m_vBfsDistance[cy * m_BfsWidth + cx];

		for(int i = 0; i < 4; i++)
		{
			int nx = cx + aDx[i];
			int ny = cy + aDy[i];

			if(nx >= 0 && nx < m_BfsWidth && ny >= 0 && ny < m_BfsHeight)
			{
				int Index = ny * m_BfsWidth + nx;
				if(m_vBfsDistance[Index] == -1)
				{
					int Tile = pCol->GetCollisionAt((float)nx * 32.0f + 16.0f, (float)ny * 32.0f + 16.0f);
					if(Tile != TILE_SOLID && Tile != TILE_FREEZE && Tile != TILE_DFREEZE && Tile != TILE_DEATH)
					{
						m_vBfsDistance[Index] = CurDist + 1;
						Q.push({nx, ny});
					}
				}
			}
		}
	}

	m_BfsComputed = true;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "[AI Navigation] Computed 2D BFS distance map (%dx%d) from spawn (%d, %d)",
		m_BfsWidth, m_BfsHeight, SpawnTileX, SpawnTileY);
	m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ai", aBuf);
}

int CEvolutionManager::GetBfsDistance(vec2 Pos) const
{
	if(!m_BfsComputed || m_BfsWidth <= 0 || m_BfsHeight <= 0)
		return -1;

	int BestDist = -1;
	const vec2 aOffsets[5] = {
		vec2(0.0f, 0.0f),
		vec2(0.0f, -14.0f),
		vec2(0.0f, 14.0f),
		vec2(-14.0f, 0.0f),
		vec2(14.0f, 0.0f)
	};

	for(int i = 0; i < 5; i++)
	{
		vec2 P = Pos + aOffsets[i];
		int tx = std::clamp(round_to_int(P.x) / 32, 0, m_BfsWidth - 1);
		int ty = std::clamp(round_to_int(P.y) / 32, 0, m_BfsHeight - 1);
		int d = m_vBfsDistance[ty * m_BfsWidth + tx];
		if(d > BestDist)
			BestDist = d;
	}

	return BestDist;
}

vec2 CEvolutionManager::GetBfsProgressDirection(vec2 Pos) const
{
	if(!m_BfsComputed || m_BfsWidth <= 0 || m_BfsHeight <= 0)
		return vec2(1.0f, 0.0f);

	int tx = std::clamp(round_to_int(Pos.x) / 32, 0, m_BfsWidth - 1);
	int ty = std::clamp(round_to_int(Pos.y) / 32, 0, m_BfsHeight - 1);
	int CurDist = GetBfsDistance(Pos);

	int BestDist = CurDist;
	vec2 BestDir = vec2(0.0f, 0.0f);

	const int aDx[8] = {1, -1, 0, 0, 1, -1, 1, -1};
	const int aDy[8] = {0, 0, 1, -1, 1, 1, -1, -1};

	for(int i = 0; i < 8; i++)
	{
		int nx = tx + aDx[i];
		int ny = ty + aDy[i];
		if(nx >= 0 && nx < m_BfsWidth && ny >= 0 && ny < m_BfsHeight)
		{
			int d = m_vBfsDistance[ny * m_BfsWidth + nx];
			if(d > BestDist)
			{
				BestDist = d;
				BestDir = vec2((float)aDx[i], (float)aDy[i]);
			}
		}
	}

	if(length(BestDir) > 0.001f)
		return normalize(BestDir);

	return vec2(1.0f, 0.0f);
}

void CEvolutionManager::StartEvolution(int PopulationSize)
{
	if(!m_pGameServer || !m_pGameServer->Server())
		return;

	// Drop previous bot clients if any
	StopEvolution();

	PopulationSize = std::clamp(PopulationSize, 2, 64);
	m_Stats.m_TotalBots = PopulationSize;
	m_Stats.m_Generation = 1;
	m_Stats.m_Active = true;
	m_Stats.m_CurrentGenTicks = 0;
	m_Stats.m_BestFitnessCurrent = 0.0f;
	m_Stats.m_BestFitnessAllTime = 0.0f;
	m_Stats.m_MaxDistanceX = 0.0f;
	m_Stats.m_CheckpointsReached = 0;
	m_Stats.m_BestBotIdLastGen = -1;
	m_Stats.m_StagnationGenerations = 0;
	m_Stats.m_LastGenBestProgress = 0.0f;
	m_Stats.m_LastStuckCentroid = vec2(0.0f, 0.0f);
	m_BfsComputed = false;

	m_vAgents.clear();

	int CurrentSlot = 1;
	for(int i = 0; i < PopulationSize; i++)
	{
		while(CurrentSlot < m_pGameServer->Server()->MaxClients() &&
		      m_pGameServer->Server()->ClientIngame(CurrentSlot))
		{
			CurrentSlot++;
		}
		if(CurrentSlot >= m_pGameServer->Server()->MaxClients())
			break;

		SBotAgent Agent;
		Agent.m_ClientId = CurrentSlot;
		Agent.m_Net.InitRandom(m_Rng);
		Agent.m_IsAlive = true;
		m_vAgents.push_back(Agent);

		char aName[32];
		str_format(aName, sizeof(aName), "Bot %d", Agent.m_ClientId);
		m_pGameServer->Server()->InitBotClient(Agent.m_ClientId, aName);
		m_pGameServer->Server()->SetClientClan(Agent.m_ClientId, "AI-Ghost");

		CurrentSlot++;
	}

	m_Stats.m_TotalBots = (int)m_vAgents.size();

	// Disable collision and hooking between players so the 100 bots do not collide on start
	m_pGameServer->GlobalTuning()->m_PlayerCollision = 0;
	m_pGameServer->GlobalTuning()->m_PlayerHooking = 0;

	// Spawn or respawn all agents
	RespawnAllBots();
}

void CEvolutionManager::StopEvolution()
{
	if(!m_Stats.m_Active && m_vAgents.empty())
		return;

	m_Stats.m_Active = false;
	if(m_pGameServer && m_pGameServer->Server())
	{
		for(auto &Agent : m_vAgents)
		{
			m_pGameServer->Server()->DropBotClient(Agent.m_ClientId);
		}
	}
	m_vAgents.clear();
}

void CEvolutionManager::ResetGeneration()
{
	if(!m_Stats.m_Active)
		return;

	EvaluateAndEvolve();
	RespawnAllBots();
}

bool CEvolutionManager::IsBot(int ClientId) const
{
	if(!m_Stats.m_Active)
		return false;

	for(const auto &Agent : m_vAgents)
	{
		if(Agent.m_ClientId == ClientId)
			return true;
	}
	return false;
}

void CEvolutionManager::OnTick()
{
	if(!m_Stats.m_Active || !m_pGameServer)
		return;

	m_Stats.m_CurrentGenTicks++;

	int Alive = 0;
	float BestX = -999999.0f;
	int CurrentLeaderId = -1;

	for(auto &Agent : m_vAgents)
	{
		if(!Agent.m_IsAlive)
			continue;

		CPlayer *pPlayer = m_pGameServer->m_apPlayers[Agent.m_ClientId];
		if(!pPlayer)
		{
			Agent.m_IsAlive = false;
			continue;
		}

		CCharacter *pChar = pPlayer->GetCharacter();
		if(!pChar || !pChar->IsAlive())
		{
			// Give bots a 50-tick grace period after generation starts to spawn before counting them as dead
			if(m_Stats.m_CurrentGenTicks > 50)
			{
				Agent.m_IsAlive = false;
			}
			continue;
		}

		Alive++;
		Agent.m_TicksAlive++;

		vec2 Pos = pChar->GetPos();
		Agent.m_DeathPos = Pos; // Continuously update latest position

		if(Agent.m_TicksAlive == 1)
		{
			Agent.m_SpawnX = Pos.x;
			Agent.m_SpawnY = Pos.y;
			Agent.m_MaxX = Pos.x;
			Agent.m_LastWatchdogPos = Pos;

			if(!m_BfsComputed)
			{
				ComputeBfsDistanceMap(round_to_int(Pos.x) / 32, round_to_int(Pos.y) / 32);
			}
		}

		if(Pos.x > Agent.m_MaxX)
			Agent.m_MaxX = Pos.x;

		if(Pos.x > BestX)
		{
			BestX = Pos.x;
			CurrentLeaderId = Agent.m_ClientId;
		}

		// Watchdog 1: Freeze check (only after 50 ticks of life)
		if(Agent.m_TicksAlive > 50)
		{
			if(pChar->m_Core.m_IsInFreeze || pChar->m_FreezeTime > 0 || pChar->m_Core.m_DeepFrozen)
			{
				Agent.m_FreezeTicks++;
				float Speed = length(pChar->Core()->m_Vel);
				if(Speed < 0.2f || Agent.m_FreezeTicks > 150)
				{
					Agent.m_DeathPos = Pos;
					pChar->Die(Agent.m_ClientId, WEAPON_WORLD, false);
					Agent.m_IsAlive = false;
					continue;
				}
			}
			else
			{
				Agent.m_FreezeTicks = 0;
			}

			// Watchdog 2: Standing still / Stuck check (only after 100 ticks)
			if(Agent.m_TicksAlive > 100)
			{
				if(distance(Pos, Agent.m_LastWatchdogPos) < 32.0f)
				{
					Agent.m_StuckTicks++;
					int MaxStuckTicks = 200 + (Agent.m_ClientId % 50);
					if(Agent.m_StuckTicks >= MaxStuckTicks)
					{
						Agent.m_DiedStuck = true;
						Agent.m_DeathPos = Pos;
						pChar->Die(Agent.m_ClientId, WEAPON_WORLD, false);
						Agent.m_IsAlive = false;
						continue;
					}
				}
				else
				{
					Agent.m_LastWatchdogPos = Pos;
					Agent.m_StuckTicks = 0;
				}
			}
		}
	}

	m_Stats.m_AliveCount = Alive;
	m_Stats.m_BestBotIdCurrent = CurrentLeaderId;
	if(BestX > m_Stats.m_MaxDistanceX)
		m_Stats.m_MaxDistanceX = BestX;

	// End of generation condition: must have passed at least 50 ticks of the generation!
	if(m_Stats.m_CurrentGenTicks > 50 && (Alive == 0 || m_Stats.m_CurrentGenTicks >= MAX_GEN_TICKS))
	{
		EvaluateAndEvolve();
		RespawnAllBots();
	}
}

void CEvolutionManager::OnCharacterPreTick(CCharacter *pChar)
{
	if(!m_Stats.m_Active || !pChar || !pChar->GetPlayer())
		return;

	int ClientId = pChar->GetPlayer()->GetCid();
	SBotAgent *pAgent = nullptr;
	for(auto &Agent : m_vAgents)
	{
		if(Agent.m_ClientId == ClientId)
		{
			pAgent = &Agent;
			break;
		}
	}

	if(!pAgent || !pAgent->m_IsAlive)
		return;

	// Collect 25 inputs for the neural network
	float aInputs[CNeuralNet::INPUT_SIZE];
	vec2 Pos = pChar->GetPos();

	// 1. 16 LIDAR collision distance rays (radial, 360 deg)
	constexpr float MAX_RAY_DIST = 400.0f;
	for(int i = 0; i < 16; i++)
	{
		float Angle = i * (2.0f * 3.1415926535f / 16.0f);
		vec2 Dir = vec2(std::cos(Angle), std::sin(Angle));
		vec2 Target = Pos + Dir * MAX_RAY_DIST;
		vec2 Col, BeforeCol;
		int Hit = m_pGameServer->Collision()->IntersectLine(Pos, Target, &Col, &BeforeCol);
		if(Hit)
		{
			aInputs[i] = distance(Pos, Col) / MAX_RAY_DIST;
		}
		else
		{
			aInputs[i] = 1.0f;
		}
	}

	// 2. 4 directional freeze detector rays (Right, Right-Up, Right-Down, Up)
	const vec2 aFreezeDirs[4] = {
		vec2(1.0f, 0.0f),
		vec2(0.707f, -0.707f),
		vec2(0.707f, 0.707f),
		vec2(0.0f, -1.0f)
	};
	for(int i = 0; i < 4; i++)
	{
		aInputs[16 + i] = 0.0f;
		for(float Step = 32.0f; Step <= 256.0f; Step += 32.0f)
		{
			vec2 CheckPt = Pos + aFreezeDirs[i] * Step;
			int Tile = m_pGameServer->Collision()->GetCollisionAt(CheckPt.x, CheckPt.y);
			if(Tile == TILE_FREEZE || Tile == TILE_DFREEZE)
			{
				aInputs[16 + i] = 1.0f - (Step / 256.0f);
				break;
			}
		}
	}

	// 3. Physical status
	aInputs[20] = std::clamp(pChar->Core()->m_Vel.x / 20.0f, -2.0f, 2.0f);
	aInputs[21] = std::clamp(pChar->Core()->m_Vel.y / 20.0f, -2.0f, 2.0f);
	aInputs[22] = (pChar->Core()->m_Jumped >= 2) ? 0.0f : 1.0f;
	aInputs[23] = pChar->IsGrounded() ? 1.0f : 0.0f;
	aInputs[24] = (pChar->m_Core.m_IsInFreeze || pChar->m_FreezeTime > 0 || pChar->m_Core.m_DeepFrozen) ? 1.0f : 0.0f;

	// 4. Directional BFS navigation guidance vector (inputs 25 & 26)
	vec2 NavDir = GetBfsProgressDirection(Pos);
	aInputs[25] = NavDir.x;
	aInputs[26] = NavDir.y;

	// Track BFS progress and unique visited tiles
	int BfsDist = GetBfsDistance(Pos);
	if(BfsDist > pAgent->m_MaxBfsDistance)
		pAgent->m_MaxBfsDistance = BfsDist;

	if(m_BfsWidth > 0 && m_BfsHeight > 0)
	{
		int Tx = std::clamp(round_to_int(Pos.x) / 32, 0, m_BfsWidth - 1);
		int Ty = std::clamp(round_to_int(Pos.y) / 32, 0, m_BfsHeight - 1);
		int TileIndex = Ty * m_BfsWidth + Tx;
		if(TileIndex >= 0 && TileIndex < (int)pAgent->m_vVisitedMap.size())
		{
			if(!pAgent->m_vVisitedMap[TileIndex])
			{
				pAgent->m_vVisitedMap[TileIndex] = 1;
				pAgent->m_VisitedTilesCount++;
			}
		}
	}

	// Forward pass
	CNeuralNet::SOutput Out = pAgent->m_Net.Forward(aInputs);

	// Apply decisions directly to character input
	pChar->m_Input.m_Direction = (Out.m_MoveRight ? 1 : 0) - (Out.m_MoveLeft ? 1 : 0);
	pChar->m_Input.m_Jump = Out.m_Jump ? 1 : 0;
	pChar->m_Input.m_Hook = Out.m_Hook ? 1 : 0;
	pChar->m_Input.m_TargetX = round_to_int(std::cos(Out.m_AimAngle) * 200.0f);
	pChar->m_Input.m_TargetY = round_to_int(std::sin(Out.m_AimAngle) * 200.0f);

	// Track wall hugging (pushing into a solid obstacle without moving)
	if(pChar->m_Input.m_Direction != 0 && std::fabs(pChar->Core()->m_Vel.x) < 0.2f && pChar->IsGrounded())
	{
		pAgent->m_WallHugTicks++;
	}
}

void CEvolutionManager::OnBotDeath(int ClientId)
{
	for(auto &Agent : m_vAgents)
	{
		if(Agent.m_ClientId == ClientId)
		{
			Agent.m_IsAlive = false;
			CPlayer *pPlayer = m_pGameServer->m_apPlayers[ClientId];
			if(pPlayer && pPlayer->GetCharacter())
			{
				Agent.m_DeathPos = pPlayer->GetCharacter()->GetPos();
			}
			break;
		}
	}
}

void CEvolutionManager::OnBotReachCheckpoint(int ClientId, int Checkpoint)
{
	for(auto &Agent : m_vAgents)
	{
		if(Agent.m_ClientId == ClientId)
		{
			if(Checkpoint != Agent.m_LastCheckpoint)
			{
				Agent.m_LastCheckpoint = Checkpoint;
				Agent.m_Checkpoints++;
				if(Agent.m_Checkpoints > m_Stats.m_CheckpointsReached)
					m_Stats.m_CheckpointsReached = Agent.m_Checkpoints;
			}
			break;
		}
	}
}

void CEvolutionManager::EvaluateAndEvolve()
{
	if(m_vAgents.empty())
		return;

	// 1. Calculate fitness for each agent
	for(auto &Agent : m_vAgents)
	{
		// Progress is primarily BFS distance along the level (32 points per tile = 1 point per pixel)
		// Plus exploration bonus for visiting unique tiles
		float PathProgress = (float)Agent.m_MaxBfsDistance * 32.0f;
		float XProgress = std::max(0.0f, Agent.m_MaxX - Agent.m_SpawnX);
		float BaseProgress = std::max(PathProgress, XProgress * 0.5f);

		Agent.m_Fitness = BaseProgress + (Agent.m_VisitedTilesCount * 4.0f) + (Agent.m_Checkpoints * 150.0f) - (Agent.m_TicksAlive * 0.05f);

		// Direct stuck watchdog penalty
		if(Agent.m_DiedStuck)
		{
			Agent.m_Fitness -= 300.0f + (Agent.m_WallHugTicks * 0.25f);
		}
	}

	// 2. Multi-generation stagnation & stuck analysis
	float CurrentBestProgress = 0.0f;
	vec2 CurrentLeaderPos = vec2(0.0f, 0.0f);
	for(const auto &Agent : m_vAgents)
	{
		float Prog = (float)Agent.m_MaxBfsDistance * 32.0f;
		if(Prog > CurrentBestProgress)
		{
			CurrentBestProgress = Prog;
			CurrentLeaderPos = (Agent.m_DeathPos.x != 0.0f || Agent.m_DeathPos.y != 0.0f) ? Agent.m_DeathPos : vec2(Agent.m_MaxX, Agent.m_SpawnY);
		}
	}

	// Check if progress hasn't improved over last generation
	bool Stagnated = (CurrentBestProgress <= m_Stats.m_LastGenBestProgress + 16.0f);
	if(Stagnated)
	{
		m_Stats.m_StagnationGenerations++;
	}
	else
	{
		m_Stats.m_StagnationGenerations = 0;
		m_Stats.m_LastGenBestProgress = CurrentBestProgress;
		m_Stats.m_LastStuckCentroid = CurrentLeaderPos;
	}

	// Multi-generation stuck penalty ("уменьшать награду или же давать минус вообще")
	if(m_Stats.m_StagnationGenerations >= 2)
	{
		float StagnationMult = 1.0f + (float)(m_Stats.m_StagnationGenerations - 1) * 0.8f;
		float StagnationPenalty = 500.0f * StagnationMult;

		for(auto &Agent : m_vAgents)
		{
			bool InStuckZone = (distance(Agent.m_DeathPos, m_Stats.m_LastStuckCentroid) < 150.0f);
			if(Agent.m_DiedStuck || InStuckZone)
			{
				Agent.m_Fitness -= StagnationPenalty;
				// NOTE: Fitness is deliberately allowed to become negative as requested,
				// pushing stuck bots to the very bottom of the pool.
			}
		}

		char aStuckMsg[160];
		str_format(aStuckMsg, sizeof(aStuckMsg),
			"[AI Watchdog] STAGNATION! Bots stuck at (%.0f, %.0f) for %d gens! Applied -%.0f penalty & hyper-mutation!",
			m_Stats.m_LastStuckCentroid.x, m_Stats.m_LastStuckCentroid.y,
			m_Stats.m_StagnationGenerations, StagnationPenalty);
		m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ai", aStuckMsg);
		m_pGameServer->SendChat(-1, TEAM_ALL, aStuckMsg);
	}
	else
	{
		m_Stats.m_LastStuckCentroid = CurrentLeaderPos;
	}

	// 3. Sort descending by fitness (highest fitness first, negative stuck bots at bottom)
	std::sort(m_vAgents.begin(), m_vAgents.end(), [](const SBotAgent &A, const SBotAgent &B) {
		return A.m_Fitness > B.m_Fitness;
	});

	// 4. Record leader stats
	m_Stats.m_BestFitnessCurrent = m_vAgents[0].m_Fitness;
	m_Stats.m_BestBotIdLastGen = m_vAgents[0].m_ClientId;

	if(m_vAgents[0].m_Fitness > m_Stats.m_BestFitnessAllTime)
	{
		m_Stats.m_BestFitnessAllTime = m_vAgents[0].m_Fitness;
		m_AllTimeBestNet = m_vAgents[0].m_Net;
		m_HasAllTimeBest = true;
	}

	// 5. Adaptive reproduction & local optimum breakout
	int TotalAgents = (int)m_vAgents.size();
	int EliteCount = std::min(3, TotalAgents);
	int RandomImmigrants = 0;
	float MutationRate = 0.10f;
	float MutationStrength = 0.15f;

	if(m_Stats.m_StagnationGenerations >= 4)
	{
		// Critical stagnation: Break out aggressively!
		EliteCount = 0; // No elites preserved unchanged!
		RandomImmigrants = TotalAgents * 4 / 10; // 40% fresh brains
		MutationRate = 0.40f;
		MutationStrength = 0.35f;
	}
	else if(m_Stats.m_StagnationGenerations >= 2)
	{
		// Moderate stagnation: shake up the population
		EliteCount = 1; // Only 1 elite
		RandomImmigrants = TotalAgents / 4; // 25% fresh brains
		MutationRate = 0.25f;
		MutationStrength = 0.25f;
	}

	std::vector<CNeuralNet> vNewNets;
	vNewNets.reserve(TotalAgents);

	// A. Elites
	for(int i = 0; i < EliteCount; i++)
	{
		CNeuralNet EliteNet = m_vAgents[i].m_Net;
		if(m_Stats.m_StagnationGenerations >= 2)
		{
			// Even the single elite gets a tiny mutation jitter so it doesn't repeat identical stuck inputs
			EliteNet.Mutate(m_Rng, 0.05f, 0.05f);
		}
		vNewNets.push_back(EliteNet);
	}

	// B. Random Immigrants (explore entirely new paths)
	for(int i = 0; i < RandomImmigrants; i++)
	{
		CNeuralNet FreshNet;
		FreshNet.InitRandom(m_Rng);
		vNewNets.push_back(FreshNet);
	}

	// C. Offspring from top performers
	int ParentPoolSize = std::max(1, std::min(8, TotalAgents));
	std::uniform_int_distribution<int> ParentDist(0, ParentPoolSize - 1);

	while((int)vNewNets.size() < TotalAgents)
	{
		int ParentIdx = ParentDist(m_Rng);
		CNeuralNet ChildNet = m_vAgents[ParentIdx].m_Net;
		ChildNet.Mutate(m_Rng, MutationRate, MutationStrength);
		vNewNets.push_back(ChildNet);
	}

	// Apply new neural nets back to agents
	for(size_t i = 0; i < m_vAgents.size(); i++)
	{
		m_vAgents[i].m_Net = vNewNets[i];
	}

	m_Stats.m_Generation++;
	m_Stats.m_CurrentGenTicks = 0;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "[AI Gen #%d Complete] Leader: Bot #%d | Fitness: %.0f | Max Dist: %.0f",
		m_Stats.m_Generation - 1, m_Stats.m_BestBotIdLastGen, m_Stats.m_BestFitnessCurrent, m_Stats.m_MaxDistanceX);
	m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ai", aBuf);
	m_pGameServer->SendChat(-1, TEAM_ALL, aBuf);
	m_pGameServer->SendBroadcast(aBuf, -1, true);
}

void CEvolutionManager::RespawnAllBots()
{
	if(!m_pGameServer || !m_pGameServer->Server())
		return;

	int MapTotalTiles = (m_BfsWidth > 0 && m_BfsHeight > 0) ? (m_BfsWidth * m_BfsHeight) : 0;

	for(auto &Agent : m_vAgents)
	{
		Agent.m_IsAlive = true;
		Agent.m_TicksAlive = 0;
		Agent.m_StuckTicks = 0;
		Agent.m_FreezeTicks = 0;
		Agent.m_WallHugTicks = 0;
		Agent.m_DiedStuck = false;
		Agent.m_Checkpoints = 0;
		Agent.m_LastCheckpoint = -1;
		Agent.m_SpawnX = 0.0f;
		Agent.m_SpawnY = 0.0f;
		Agent.m_MaxX = 0.0f;
		Agent.m_DeathPos = vec2(0.0f, 0.0f);
		Agent.m_MaxBfsDistance = 0;
		Agent.m_VisitedTilesCount = 0;
		Agent.m_LastWatchdogPos = vec2(0.0f, 0.0f);

		if(MapTotalTiles > 0)
		{
			Agent.m_vVisitedMap.assign(MapTotalTiles, 0);
		}
		else
		{
			Agent.m_vVisitedMap.clear();
		}

		// Mark clan for client rendering: Leader vs Ghost
		bool IsLeader = (Agent.m_ClientId == m_Stats.m_BestBotIdLastGen);
		m_pGameServer->Server()->SetClientClan(Agent.m_ClientId, IsLeader ? "AI-Leader" : "AI-Ghost");

		char aName[32];
		str_format(aName, sizeof(aName), IsLeader ? "[LEADER] Bot %d" : "Bot %d", Agent.m_ClientId);
		m_pGameServer->Server()->SetClientName(Agent.m_ClientId, aName);

		CPlayer *pPlayer = m_pGameServer->m_apPlayers[Agent.m_ClientId];
		if(pPlayer)
		{
			pPlayer->ResetForAiRespawn();
		}
	}
	m_Stats.m_CurrentGenTicks = 0;
}

bool CEvolutionManager::SaveBest(const char *pFilename)
{
	if(m_HasAllTimeBest)
		return m_AllTimeBestNet.Save(pFilename);
	else if(!m_vAgents.empty())
		return m_vAgents[0].m_Net.Save(pFilename);
	return false;
}

bool CEvolutionManager::LoadBest(const char *pFilename)
{
	CNeuralNet LoadedNet;
	if(!LoadedNet.Load(pFilename))
		return false;

	m_AllTimeBestNet = LoadedNet;
	m_HasAllTimeBest = true;

	// Populate top 5 with loaded net
	for(size_t i = 0; i < std::min((size_t)5, m_vAgents.size()); i++)
	{
		m_vAgents[i].m_Net = LoadedNet;
	}

	return true;
}
