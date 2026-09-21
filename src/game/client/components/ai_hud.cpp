#include "ai_hud.h"

#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <generated/protocol.h>

#include <algorithm>
#include <cmath>

void CAiHud::OnReset()
{
	m_MouseUnlocked = false;
}

void CAiHud::UpdateMousePos()
{
	vec2 Pos = Input()->NativeMousePos();
	float WindowW = Graphics()->WindowWidth();
	float WindowH = Graphics()->WindowHeight();
	if(WindowW > 0.0f && WindowH > 0.0f)
	{
		float ScreenH = 300.0f;
		float ScreenW = ScreenH * Graphics()->ScreenAspect();
		m_MousePos.x = (Pos.x / WindowW) * ScreenW;
		m_MousePos.y = (Pos.y / WindowH) * ScreenH;
	}
}

bool CAiHud::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(m_MouseUnlocked || g_Config.m_ClAiTurbo)
	{
		UpdateMousePos();
		return true;
	}
	return false;
}

void CAiHud::SendAiCommand(const char *pCmd)
{
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "rcon %s", pCmd);
	Console()->ExecuteLine(aBuf, IConsole::CLIENT_ID_UNSPECIFIED);

	str_format(aBuf, sizeof(aBuf), "say /%s", pCmd);
	Console()->ExecuteLine(aBuf, IConsole::CLIENT_ID_UNSPECIFIED);
}

void CAiHud::AdjustSpeed(int Delta)
{
	static const int s_Speeds[] = {1, 2, 5, 10, 20, 50, 100};
	int Cur = g_Config.m_SvAiTimescale;
	int CurIdx = 0;
	for(int i = 0; i < (int)std::size(s_Speeds); i++)
	{
		if(s_Speeds[i] <= Cur)
			CurIdx = i;
	}
	int NewIdx = std::clamp(CurIdx + Delta, 0, (int)std::size(s_Speeds) - 1);
	int NewSpeed = s_Speeds[NewIdx];
	g_Config.m_SvAiTimescale = NewSpeed;

	char aCmd[64];
	str_format(aCmd, sizeof(aCmd), "ai_timescale %d", NewSpeed);
	SendAiCommand(aCmd);
}

void CAiHud::ToggleTurbo()
{
	int NewTurbo = g_Config.m_ClAiTurbo ^ 1;
	g_Config.m_ClAiTurbo = NewTurbo;
	g_Config.m_SvAiTurbo = NewTurbo;

	char aCmd[64];
	str_format(aCmd, sizeof(aCmd), "ai_turbo %d", NewTurbo);
	SendAiCommand(aCmd);
}

bool CAiHud::HandleClick(vec2 MousePos)
{
	if(g_Config.m_ClAiTurbo)
	{
		if(m_BtnTurboResume.Inside(MousePos))
		{
			ToggleTurbo();
			return true;
		}
		if(m_BtnSpeedDown.Inside(MousePos))
		{
			AdjustSpeed(-1);
			return true;
		}
		if(m_BtnSpeedUp.Inside(MousePos))
		{
			AdjustSpeed(+1);
			return true;
		}
		if(m_BtnClose.Inside(MousePos))
		{
			g_Config.m_ClAiHud = 0;
			return true;
		}
		return false;
	}

	if(!g_Config.m_ClAiHud)
	{
		if(m_BtnMinPill.Inside(MousePos))
		{
			g_Config.m_ClAiHud = 1;
			return true;
		}
		return false;
	}

	if(m_BtnClose.Inside(MousePos))
	{
		g_Config.m_ClAiHud = 0;
		if(m_MouseUnlocked)
		{
			m_MouseUnlocked = false;
			Input()->MouseModeRelative();
		}
		return true;
	}
	if(m_BtnSpeedDown.Inside(MousePos))
	{
		AdjustSpeed(-1);
		return true;
	}
	if(m_BtnSpeedUp.Inside(MousePos))
	{
		AdjustSpeed(+1);
		return true;
	}
	if(m_BtnTurbo.Inside(MousePos))
	{
		ToggleTurbo();
		return true;
	}
	if(m_BtnCam.Inside(MousePos))
	{
		g_Config.m_ClAiCamFollow ^= 1;
		return true;
	}
	if(m_BtnRestart.Inside(MousePos))
	{
		SendAiCommand("ai_reset");
		return true;
	}
	return false;
}

bool CAiHud::OnInput(const IInput::CEvent &Event)
{
	if(Event.m_Flags & IInput::FLAG_PRESS)
	{
		// 1. Close/toggle HUD
		if(Event.m_Key == KEY_F3 || Event.m_Key == KEY_F4 || Event.m_Key == KEY_H)
		{
			g_Config.m_ClAiHud ^= 1;
			if(!g_Config.m_ClAiHud && m_MouseUnlocked)
			{
				m_MouseUnlocked = false;
				Input()->MouseModeRelative();
			}
			return true;
		}

		// 2. Camera follow toggle
		if(Event.m_Key == KEY_F5 || (Event.m_Key == KEY_C && (m_MouseUnlocked || g_Config.m_ClAiTurbo)))
		{
			g_Config.m_ClAiCamFollow ^= 1;
			return true;
		}

		// 3. Turbo toggle
		if(Event.m_Key == KEY_T)
		{
			ToggleTurbo();
			return true;
		}

		// 4. Timescale adjust
		if(Event.m_Key == KEY_LEFTBRACKET || Event.m_Key == KEY_MINUS || Event.m_Key == KEY_KP_MINUS)
		{
			AdjustSpeed(-1);
			return true;
		}
		if(Event.m_Key == KEY_RIGHTBRACKET || Event.m_Key == KEY_EQUALS || Event.m_Key == KEY_KP_PLUS)
		{
			AdjustSpeed(+1);
			return true;
		}

		// 5. Free/lock mouse cursor for clicking (Left Alt or F2)
		if(Event.m_Key == KEY_LALT || Event.m_Key == KEY_RALT || Event.m_Key == KEY_F2)
		{
			m_MouseUnlocked = !m_MouseUnlocked;
			if(m_MouseUnlocked)
				Input()->MouseModeAbsolute();
			else
				Input()->MouseModeRelative();
			return true;
		}

		// 6. Escape to cancel mouse cursor
		if(Event.m_Key == KEY_ESCAPE && m_MouseUnlocked)
		{
			m_MouseUnlocked = false;
			Input()->MouseModeRelative();
			return true;
		}

		// 7. Mouse click
		if(Event.m_Key == KEY_MOUSE_1 && (m_MouseUnlocked || g_Config.m_ClAiTurbo))
		{
			UpdateMousePos();
			if(HandleClick(m_MousePos))
				return true;
			return true; // Eat click when mouse is unlocked
		}
	}
	return false;
}

void CAiHud::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_BROADCAST)
	{
		const CNetMsg_Sv_Broadcast *pMsg = (const CNetMsg_Sv_Broadcast *)pRawMsg;
		const char *pMatch = str_find(pMsg->m_pMessage, "[AI Gen #");
		if(pMatch)
		{
			int Gen = 0;
			float Fitness = 0.0f;
			float MaxDist = 0.0f;
			if(sscanf(pMatch, "[AI Gen #%d Complete] Leader: Bot #%*d | Fitness: %f | Max Dist: %f", &Gen, &Fitness, &MaxDist) >= 1)
			{
				m_CurrentGen = Gen + 2;
				if(MaxDist > m_AllTimeMaxX)
					m_AllTimeMaxX = MaxDist;

				if(m_HistoryCount < MAX_HISTORY)
				{
					m_aHistoryDist[m_HistoryCount++] = MaxDist;
				}
				else
				{
					for(int i = 0; i < MAX_HISTORY - 1; i++)
						m_aHistoryDist[i] = m_aHistoryDist[i + 1];
					m_aHistoryDist[MAX_HISTORY - 1] = MaxDist;
				}
			}
		}
	}
	else if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		const CNetMsg_Sv_Chat *pMsg = (const CNetMsg_Sv_Chat *)pRawMsg;
		const char *pText = pMsg->m_pMessage;
		const char *pMatch = str_find(pText, "[AI Gen #");
		if(pMatch)
		{
			int Gen = 0;
			float Fitness = 0.0f;
			float MaxDist = 0.0f;
			if(sscanf(pMatch, "[AI Gen #%d Complete] Leader: Bot #%*d | Fitness: %f | Max Dist: %f", &Gen, &Fitness, &MaxDist) >= 1)
			{
				m_CurrentGen = Gen + 2;
				if(MaxDist > m_AllTimeMaxX)
					m_AllTimeMaxX = MaxDist;
			}
		}
		int ScaleVal = 0;
		if(sscanf(pText, "AI Timescale set to %dx", &ScaleVal) == 1)
		{
			g_Config.m_SvAiTimescale = ScaleVal;
		}
		if(str_find(pText, "AI Turbo mode: ENABLED"))
		{
			g_Config.m_ClAiTurbo = 1;
			g_Config.m_SvAiTurbo = 1;
		}
		else if(str_find(pText, "AI Turbo mode: DISABLED"))
		{
			g_Config.m_ClAiTurbo = 0;
			g_Config.m_SvAiTurbo = 0;
		}
	}
}

void CAiHud::RenderTurboDashboard(int AliveBots, int TotalBots, float LeadX, const char *pLeaderName)
{
	const float ScreenH = 300.0f;
	const float ScreenW = ScreenH * Graphics()->ScreenAspect();
	Graphics()->MapScreenToSize(ScreenW, ScreenH);

	// Fullscreen backdrop: dark carbon slate
	Graphics()->DrawRect(0, 0, ScreenW, ScreenH, ColorRGBA(0.04f, 0.06f, 0.09f, 0.98f), IGraphics::CORNER_NONE, 0.0f);

	// Outer matrix grid frame
	const float BoxW = 260.0f;
	const float BoxH = 200.0f;
	const float BoxX = (ScreenW - BoxW) / 2.0f;
	const float BoxY = (ScreenH - BoxH) / 2.0f;

	Graphics()->DrawRect(BoxX - 1.0f, BoxY - 1.0f, BoxW + 2.0f, BoxH + 2.0f, ColorRGBA(0.15f, 0.85f, 1.0f, 0.5f), IGraphics::CORNER_ALL, 8.0f);
	Graphics()->DrawRect4(BoxX, BoxY, BoxW, BoxH,
		ColorRGBA(0.06f, 0.10f, 0.16f, 0.96f),
		ColorRGBA(0.06f, 0.10f, 0.16f, 0.96f),
		ColorRGBA(0.03f, 0.05f, 0.09f, 0.98f),
		ColorRGBA(0.03f, 0.05f, 0.09f, 0.98f),
		IGraphics::CORNER_ALL, 7.0f);

	// Neon top bar
	Graphics()->DrawRect(BoxX + 20.0f, BoxY, BoxW - 40.0f, 2.0f, ColorRGBA(0.2f, 0.9f, 1.0f, 1.0f), IGraphics::CORNER_ALL, 1.0f);

	TextRender()->TextOutlineColor(ColorRGBA(0.0f, 0.0f, 0.0f, 0.9f));

	// Title
	TextRender()->TextColor(ColorRGBA(0.25f, 0.90f, 1.0f, 1.0f));
	TextRender()->Text(BoxX + 16.0f, BoxY + 10.0f, 7.0f, "⚡ DDNET-AI HYPER-TRAINING MATRIX ⚡");

	TextRender()->TextColor(ColorRGBA(1.0f, 0.75f, 0.2f, 0.9f));
	TextRender()->Text(BoxX + 16.0f, BoxY + 22.0f, 4.0f, "[HEADLESS MODE ACTIVE - UNTHROTTLED SIMULATION]");

	// Generation Card
	Graphics()->DrawRect(BoxX + 16.0f, BoxY + 34.0f, BoxW - 32.0f, 32.0f, ColorRGBA(0.10f, 0.16f, 0.24f, 0.8f), IGraphics::CORNER_ALL, 4.0f);
	char aGenBuf[64];
	str_format(aGenBuf, sizeof(aGenBuf), "GENERATION #%d", m_CurrentGen);
	TextRender()->TextColor(ColorRGBA(0.35f, 1.0f, 0.65f, 1.0f));
	TextRender()->Text(BoxX + 24.0f, BoxY + 40.0f, 7.5f, aGenBuf);

	char aPopBuf[64];
	str_format(aPopBuf, sizeof(aPopBuf), "Active Population: %d / %d", AliveBots, TotalBots);
	TextRender()->TextColor(ColorRGBA(0.85f, 0.92f, 1.0f, 1.0f));
	TextRender()->Text(BoxX + 24.0f, BoxY + 52.0f, 4.2f, aPopBuf);

	// Stats cards
	const float StatY = BoxY + 72.0f;
	// Lead bot card
	Graphics()->DrawRect(BoxX + 16.0f, StatY, 110.0f, 34.0f, ColorRGBA(0.08f, 0.12f, 0.18f, 0.8f), IGraphics::CORNER_ALL, 4.0f);
	TextRender()->TextColor(ColorRGBA(1.0f, 0.84f, 0.2f, 1.0f));
	TextRender()->Text(BoxX + 22.0f, StatY + 5.0f, 4.0f, "LEADER BOT");
	char aLeadBuf[64];
	str_format(aLeadBuf, sizeof(aLeadBuf), "%s", pLeaderName);
	TextRender()->TextColor(ColorRGBA(0.9f, 0.95f, 1.0f, 1.0f));
	TextRender()->Text(BoxX + 22.0f, StatY + 12.0f, 3.8f, aLeadBuf);
	str_format(aLeadBuf, sizeof(aLeadBuf), "Distance: %.0f px", LeadX > 0 ? LeadX : 0.0f);
	TextRender()->TextColor(ColorRGBA(0.6f, 0.8f, 1.0f, 1.0f));
	TextRender()->Text(BoxX + 22.0f, StatY + 20.0f, 3.6f, aLeadBuf);

	// Record card
	Graphics()->DrawRect(BoxX + 134.0f, StatY, 110.0f, 34.0f, ColorRGBA(0.08f, 0.12f, 0.18f, 0.8f), IGraphics::CORNER_ALL, 4.0f);
	TextRender()->TextColor(ColorRGBA(0.3f, 0.9f, 0.5f, 1.0f));
	TextRender()->Text(BoxX + 140.0f, StatY + 5.0f, 4.0f, "ALL-TIME RECORD");
	char aRecBuf[64];
	str_format(aRecBuf, sizeof(aRecBuf), "%.0f px", m_AllTimeMaxX);
	TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	TextRender()->Text(BoxX + 140.0f, StatY + 14.0f, 5.5f, aRecBuf);

	// Progress chart
	const float TurboChartY = BoxY + 112.0f;
	const float TurboChartW = BoxW - 32.0f;
	const float TurboChartH = 34.0f;
	Graphics()->DrawRect(BoxX + 16.0f, TurboChartY, TurboChartW, TurboChartH, ColorRGBA(0.05f, 0.08f, 0.12f, 0.85f), IGraphics::CORNER_ALL, 4.0f);
	TextRender()->TextColor(ColorRGBA(0.45f, 0.65f, 0.85f, 0.85f));
	TextRender()->Text(BoxX + 22.0f, TurboChartY + 3.0f, 3.5f, "FITNESS EVOLUTION TRAJECTORY");

	float MaxGraph = std::max(m_AllTimeMaxX * 1.1f, 800.0f);
	if(m_HistoryCount > 0)
	{
		float StepW = (TurboChartW - 16.0f) / MAX_HISTORY;
		for(int h = 0; h < m_HistoryCount; h++)
		{
			float BarVal = std::clamp(m_aHistoryDist[h] / MaxGraph, 0.08f, 1.0f);
			float BarX = BoxX + 22.0f + h * StepW;
			float BarHeight = 20.0f * BarVal;
			Graphics()->DrawRect(BarX, TurboChartY + TurboChartH - 4.0f - BarHeight, StepW - 1.5f, BarHeight, ColorRGBA(0.2f, 0.85f, 1.0f, 0.85f), IGraphics::CORNER_NONE, 0.0f);
		}
	}

	// Bottom action buttons
	const float BtnY = BoxY + 154.0f;
	m_BtnTurboResume = {BoxX + 16.0f, BtnY, 140.0f, 18.0f};
	bool ResumeHover = m_BtnTurboResume.Inside(m_MousePos);
	ColorRGBA ResumeBg = ResumeHover ? ColorRGBA(0.2f, 0.65f, 0.45f, 0.95f) : ColorRGBA(0.12f, 0.45f, 0.30f, 0.85f);
	Graphics()->DrawRect(m_BtnTurboResume.x, m_BtnTurboResume.y, m_BtnTurboResume.w, m_BtnTurboResume.h, ResumeBg, IGraphics::CORNER_ALL, 4.0f);
	TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	TextRender()->Text(m_BtnTurboResume.x + 8.0f, m_BtnTurboResume.y + 4.5f, 4.4f, "👁 RESUME VISUAL (T)");

	// Speed down and up
	m_BtnSpeedDown = {BoxX + 162.0f, BtnY, 18.0f, 18.0f};
	bool DownHover = m_BtnSpeedDown.Inside(m_MousePos);
	Graphics()->DrawRect(m_BtnSpeedDown.x, m_BtnSpeedDown.y, m_BtnSpeedDown.w, m_BtnSpeedDown.h, DownHover ? ColorRGBA(0.25f, 0.35f, 0.5f, 0.9f) : ColorRGBA(0.15f, 0.22f, 0.32f, 0.8f), IGraphics::CORNER_ALL, 3.0f);
	TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	TextRender()->Text(m_BtnSpeedDown.x + 5.5f, m_BtnSpeedDown.y + 4.5f, 5.0f, "-");

	char aSpdText[32];
	str_format(aSpdText, sizeof(aSpdText), "%dx", g_Config.m_SvAiTimescale);
	Graphics()->DrawRect(BoxX + 184.0f, BtnY, 28.0f, 18.0f, ColorRGBA(0.10f, 0.16f, 0.25f, 0.8f), IGraphics::CORNER_ALL, 3.0f);
	TextRender()->TextColor(ColorRGBA(0.35f, 0.9f, 1.0f, 1.0f));
	TextRender()->Text(BoxX + 188.0f, BtnY + 5.0f, 4.2f, aSpdText);

	m_BtnSpeedUp = {BoxX + 216.0f, BtnY, 18.0f, 18.0f};
	bool UpHover = m_BtnSpeedUp.Inside(m_MousePos);
	Graphics()->DrawRect(m_BtnSpeedUp.x, m_BtnSpeedUp.y, m_BtnSpeedUp.w, m_BtnSpeedUp.h, UpHover ? ColorRGBA(0.25f, 0.35f, 0.5f, 0.9f) : ColorRGBA(0.15f, 0.22f, 0.32f, 0.8f), IGraphics::CORNER_ALL, 3.0f);
	TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	TextRender()->Text(m_BtnSpeedUp.x + 4.5f, m_BtnSpeedUp.y + 4.5f, 5.0f, "+");

	m_BtnClose = {BoxX + BoxW - 22.0f, BoxY + 8.0f, 14.0f, 12.0f};
	bool CloseHover = m_BtnClose.Inside(m_MousePos);
	Graphics()->DrawRect(m_BtnClose.x, m_BtnClose.y, m_BtnClose.w, m_BtnClose.h, CloseHover ? ColorRGBA(0.9f, 0.2f, 0.2f, 0.95f) : ColorRGBA(0.4f, 0.1f, 0.1f, 0.7f), IGraphics::CORNER_ALL, 3.0f);
	TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	TextRender()->Text(m_BtnClose.x + 4.0f, m_BtnClose.y + 2.0f, 4.5f, "X");

	// Instructions footer
	TextRender()->TextColor(ColorRGBA(0.55f, 0.65f, 0.75f, 0.8f));
	TextRender()->Text(BoxX + 16.0f, BoxY + BoxH - 12.0f, 3.4f, "Press [T] to resume visual rendering  |  Click buttons with mouse");

	// Draw pointer cursor in turbo mode
	RenderTools()->RenderCursor(Ui()->MousePos(), 24.0f);
}

void CAiHud::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	int TotalAiBots = 0;
	int AliveAiBots = 0;
	float LeadX = -999999.0f;
	int LeaderId = -1;
	char aLeaderName[64] = "None";

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!GameClient()->m_aClients[i].m_Active)
			continue;

		const char *pName = GameClient()->m_aClients[i].m_aName;
		const char *pClan = GameClient()->m_aClients[i].m_aClan;

		bool IsGhost = (str_comp(pClan, "AI-Ghost") == 0 || str_find(pName, "Bot ") != nullptr);
		bool IsLead = (str_comp(pClan, "AI-Leader") == 0 || str_find(pName, "[LEADER]") != nullptr);

		if(IsGhost || IsLead)
		{
			TotalAiBots++;
			if(GameClient()->m_Snap.m_aCharacters[i].m_Active)
			{
				AliveAiBots++;
				vec2 Pos = GameClient()->m_aClients[i].m_RenderPos;
				if(Pos.x > LeadX)
				{
					LeadX = Pos.x;
					LeaderId = i;
					str_copy(aLeaderName, pName);
				}
			}
		}
	}

	if(LeadX > m_AllTimeMaxX)
		m_AllTimeMaxX = LeadX;

	// Always update mouse pos
	UpdateMousePos();

	// If Turbo mode is active, render full-screen turbo matrix
	if(g_Config.m_ClAiTurbo)
	{
		RenderTurboDashboard(AliveAiBots, TotalAiBots, LeadX, aLeaderName);
		return;
	}

	const float ScreenH = 300.0f;
	const float ScreenW = ScreenH * Graphics()->ScreenAspect();
	Graphics()->MapScreenToSize(ScreenW, ScreenH);

	// If HUD is closed, render compact floating pill so user can easily reopen it
	if(!g_Config.m_ClAiHud)
	{
		m_BtnMinPill = {ScreenW / 2.0f - 55.0f, 4.0f, 110.0f, 12.0f};
		bool MinHover = m_BtnMinPill.Inside(m_MousePos);
		ColorRGBA PillBg = MinHover ? ColorRGBA(0.12f, 0.40f, 0.65f, 0.95f) : ColorRGBA(0.06f, 0.12f, 0.20f, 0.85f);
		Graphics()->DrawRect(m_BtnMinPill.x - 1.0f, m_BtnMinPill.y - 1.0f, m_BtnMinPill.w + 2.0f, m_BtnMinPill.h + 2.0f, ColorRGBA(0.2f, 0.8f, 1.0f, 0.5f), IGraphics::CORNER_ALL, 4.0f);
		Graphics()->DrawRect(m_BtnMinPill.x, m_BtnMinPill.y, m_BtnMinPill.w, m_BtnMinPill.h, PillBg, IGraphics::CORNER_ALL, 3.0f);

		char aMinText[64];
		str_format(aMinText, sizeof(aMinText), "⚡ AI CORE: GEN #%d [H to open]", m_CurrentGen);
		TextRender()->TextColor(ColorRGBA(0.35f, 0.9f, 1.0f, 1.0f));
		TextRender()->Text(m_BtnMinPill.x + 6.0f, m_BtnMinPill.y + 2.5f, 3.4f, aMinText);

		if(m_MouseUnlocked)
			RenderTools()->RenderCursor(Ui()->MousePos(), 24.0f);
		return;
	}

	const float PanelX = 8.0f;
	const float PanelY = 14.0f;
	const float PanelW = 158.0f;
	const float PanelH = TotalAiBots > 0 ? 138.0f : 56.0f;

	// 1. Panel outer border & frosted background
	Graphics()->DrawRect(PanelX - 1.0f, PanelY - 1.0f, PanelW + 2.0f, PanelH + 2.0f, ColorRGBA(0.12f, 0.55f, 0.90f, 0.45f), IGraphics::CORNER_ALL, 6.0f);
	Graphics()->DrawRect4(PanelX, PanelY, PanelW, PanelH,
		ColorRGBA(0.04f, 0.07f, 0.12f, 0.94f),
		ColorRGBA(0.04f, 0.07f, 0.12f, 0.94f),
		ColorRGBA(0.02f, 0.03f, 0.06f, 0.97f),
		ColorRGBA(0.02f, 0.03f, 0.06f, 0.97f),
		IGraphics::CORNER_ALL, 5.0f);

	// 2. Top neon highlight strip
	Graphics()->DrawRect(PanelX + 12.0f, PanelY, PanelW - 24.0f, 1.5f, ColorRGBA(0.2f, 0.85f, 1.0f, 0.95f), IGraphics::CORNER_ALL, 1.0f);

	TextRender()->TextOutlineColor(ColorRGBA(0.0f, 0.0f, 0.0f, 0.8f));

	if(TotalAiBots == 0)
	{
		// Standby card
		TextRender()->TextColor(ColorRGBA(0.25f, 0.85f, 1.0f, 1.0f));
		TextRender()->Text(PanelX + 8.0f, PanelY + 6.0f, 4.2f, "AI CORE [STANDBY]");

		m_BtnClose = {PanelX + PanelW - 14.0f, PanelY + 4.0f, 10.0f, 9.0f};
		bool CloseHover = m_BtnClose.Inside(m_MousePos);
		Graphics()->DrawRect(m_BtnClose.x, m_BtnClose.y, m_BtnClose.w, m_BtnClose.h, CloseHover ? ColorRGBA(0.9f, 0.2f, 0.2f, 0.9f) : ColorRGBA(0.4f, 0.1f, 0.1f, 0.7f), IGraphics::CORNER_ALL, 2.0f);
		TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
		TextRender()->Text(m_BtnClose.x + 2.5f, m_BtnClose.y + 1.2f, 3.8f, "X");

		TextRender()->TextColor(ColorRGBA(0.85f, 0.9f, 0.95f, 0.9f));
		TextRender()->Text(PanelX + 8.0f, PanelY + 16.0f, 3.5f, "Waiting for population to spawn...");

		TextRender()->TextColor(ColorRGBA(1.0f, 0.82f, 0.15f, 1.0f));
		TextRender()->Text(PanelX + 8.0f, PanelY + 26.0f, 3.8f, "Press F1 and type: ai_start 16");

		TextRender()->TextColor(ColorRGBA(0.5f, 0.6f, 0.7f, 0.8f));
		TextRender()->Text(PanelX + 8.0f, PanelY + 40.0f, 3.2f, "[H] Close   [T] Turbo   [Alt] Mouse");
		return;
	}

	// Active AI Header
	char aGenText[64];
	str_format(aGenText, sizeof(aGenText), "GEN #%d", m_CurrentGen);
	TextRender()->TextColor(ColorRGBA(0.25f, 0.85f, 1.0f, 1.0f));
	TextRender()->Text(PanelX + 8.0f, PanelY + 6.0f, 4.2f, "AI CORE");

	// Header badge pill
	Graphics()->DrawRect(PanelX + PanelW - 54.0f, PanelY + 5.0f, 36.0f, 8.5f, ColorRGBA(0.1f, 0.55f, 0.95f, 0.35f), IGraphics::CORNER_ALL, 3.0f);
	Graphics()->DrawRect(PanelX + PanelW - 54.0f, PanelY + 5.0f, 36.0f, 8.5f, ColorRGBA(0.2f, 0.85f, 1.0f, 0.8f), IGraphics::CORNER_ALL, 3.0f);
	TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	TextRender()->Text(PanelX + PanelW - 51.0f, PanelY + 6.2f, 3.4f, aGenText);

	// Close Button [X]
	m_BtnClose = {PanelX + PanelW - 14.0f, PanelY + 4.5f, 10.0f, 9.5f};
	bool CloseHover = m_BtnClose.Inside(m_MousePos);
	Graphics()->DrawRect(m_BtnClose.x, m_BtnClose.y, m_BtnClose.w, m_BtnClose.h, CloseHover ? ColorRGBA(0.95f, 0.2f, 0.2f, 0.95f) : ColorRGBA(0.35f, 0.1f, 0.1f, 0.75f), IGraphics::CORNER_ALL, 2.0f);
	TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	TextRender()->Text(m_BtnClose.x + 2.5f, m_BtnClose.y + 1.2f, 3.8f, "X");

	// Active Population bar
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Alive Bots: %d / %d", AliveAiBots, TotalAiBots);
	TextRender()->TextColor(ColorRGBA(0.9f, 0.95f, 1.0f, 1.0f));
	TextRender()->Text(PanelX + 8.0f, PanelY + 17.0f, 3.8f, aBuf);

	const float BarW = PanelW - 16.0f;
	const float BarH = 4.0f;
	const float BarY = PanelY + 24.0f;
	Graphics()->DrawRect(PanelX + 8.0f, BarY, BarW, BarH, ColorRGBA(0.1f, 0.15f, 0.22f, 0.9f), IGraphics::CORNER_ALL, 2.0f);

	float AliveRatio = TotalAiBots > 0 ? (float)AliveAiBots / TotalAiBots : 0.0f;
	AliveRatio = std::clamp(AliveRatio, 0.0f, 1.0f);

	ColorRGBA BarColor = AliveRatio > 0.4f ? ColorRGBA(0.2f, 0.9f, 0.45f, 1.0f) :
	                     AliveRatio > 0.15f ? ColorRGBA(1.0f, 0.75f, 0.1f, 1.0f) :
	                                         ColorRGBA(0.95f, 0.25f, 0.2f, 1.0f);
	if(AliveRatio > 0.01f)
		Graphics()->DrawRect(PanelX + 8.0f, BarY, BarW * AliveRatio, BarH, BarColor, IGraphics::CORNER_ALL, 2.0f);

	// Leader spotlight box
	const float LeaderBoxY = PanelY + 31.0f;
	const float LeaderBoxH = 22.0f;
	Graphics()->DrawRect(PanelX + 8.0f, LeaderBoxY, BarW, LeaderBoxH, ColorRGBA(0.08f, 0.12f, 0.18f, 0.75f), IGraphics::CORNER_ALL, 3.0f);
	Graphics()->DrawRect(PanelX + 8.0f, LeaderBoxY, 2.0f, LeaderBoxH, ColorRGBA(1.0f, 0.84f, 0.0f, 1.0f), IGraphics::CORNER_L, 1.0f);

	if(LeaderId != -1)
		str_format(aBuf, sizeof(aBuf), "Lead: %s", aLeaderName);
	else
		str_copy(aBuf, "Lead: Searching...");
	TextRender()->TextColor(ColorRGBA(1.0f, 0.84f, 0.15f, 1.0f));
	TextRender()->Text(PanelX + 13.0f, LeaderBoxY + 3.0f, 3.8f, aBuf);

	if(LeaderId != -1)
		str_format(aBuf, sizeof(aBuf), "Distance: %.0f px  |  Record: %.0f px", LeadX, m_AllTimeMaxX);
	else
		str_format(aBuf, sizeof(aBuf), "Distance: -- px  |  Record: %.0f px", m_AllTimeMaxX);
	TextRender()->TextColor(ColorRGBA(0.75f, 0.85f, 0.95f, 1.0f));
	TextRender()->Text(PanelX + 13.0f, LeaderBoxY + 11.5f, 3.3f, aBuf);

	// Control Buttons Row 1: Speed Controls & Camera Follow
	const float Row1Y = PanelY + 56.0f;

	// Speed Down [-]
	m_BtnSpeedDown = {PanelX + 8.0f, Row1Y, 13.0f, 12.0f};
	bool SpdDnHover = m_BtnSpeedDown.Inside(m_MousePos);
	Graphics()->DrawRect(m_BtnSpeedDown.x, m_BtnSpeedDown.y, m_BtnSpeedDown.w, m_BtnSpeedDown.h, SpdDnHover ? ColorRGBA(0.25f, 0.4f, 0.6f, 0.9f) : ColorRGBA(0.12f, 0.20f, 0.32f, 0.85f), IGraphics::CORNER_ALL, 3.0f);
	TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	TextRender()->Text(m_BtnSpeedDown.x + 3.8f, m_BtnSpeedDown.y + 2.0f, 4.2f, "-");

	// Speed Display pill
	Graphics()->DrawRect(PanelX + 23.0f, Row1Y, 34.0f, 12.0f, ColorRGBA(0.08f, 0.14f, 0.22f, 0.85f), IGraphics::CORNER_ALL, 2.0f);
	str_format(aBuf, sizeof(aBuf), "%dx", g_Config.m_SvAiTimescale);
	TextRender()->TextColor(ColorRGBA(0.35f, 0.90f, 1.0f, 1.0f));
	TextRender()->Text(PanelX + 28.0f, Row1Y + 2.5f, 3.6f, aBuf);

	// Speed Up [+]
	m_BtnSpeedUp = {PanelX + 59.0f, Row1Y, 13.0f, 12.0f};
	bool SpdUpHover = m_BtnSpeedUp.Inside(m_MousePos);
	Graphics()->DrawRect(m_BtnSpeedUp.x, m_BtnSpeedUp.y, m_BtnSpeedUp.w, m_BtnSpeedUp.h, SpdUpHover ? ColorRGBA(0.25f, 0.4f, 0.6f, 0.9f) : ColorRGBA(0.12f, 0.20f, 0.32f, 0.85f), IGraphics::CORNER_ALL, 3.0f);
	TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	TextRender()->Text(m_BtnSpeedUp.x + 3.2f, m_BtnSpeedUp.y + 2.0f, 4.2f, "+");

	// Camera pill [CAM TRACK]
	m_BtnCam = {PanelX + 76.0f, Row1Y, 74.0f, 12.0f};
	bool CamHover = m_BtnCam.Inside(m_MousePos);
	bool CamFollow = g_Config.m_ClAiCamFollow != 0;
	ColorRGBA CamBg = CamFollow ? (CamHover ? ColorRGBA(0.18f, 0.55f, 0.32f, 0.95f) : ColorRGBA(0.12f, 0.40f, 0.22f, 0.85f)) :
	                              (CamHover ? ColorRGBA(0.30f, 0.30f, 0.35f, 0.95f) : ColorRGBA(0.18f, 0.18f, 0.22f, 0.85f));
	Graphics()->DrawRect(m_BtnCam.x, m_BtnCam.y, m_BtnCam.w, m_BtnCam.h, CamBg, IGraphics::CORNER_ALL, 3.0f);
	str_format(aBuf, sizeof(aBuf), "CAM: %s", CamFollow ? "TRACK" : "FREE");
	TextRender()->TextColor(CamFollow ? ColorRGBA(0.4f, 1.0f, 0.55f, 1.0f) : ColorRGBA(0.75f, 0.75f, 0.75f, 1.0f));
	TextRender()->Text(m_BtnCam.x + 8.0f, m_BtnCam.y + 2.5f, 3.4f, aBuf);

	// Control Buttons Row 2: Turbo Mode & Reset Generation
	const float Row2Y = PanelY + 71.0f;

	// Turbo Button [⚡ TURBO TRAIN]
	m_BtnTurbo = {PanelX + 8.0f, Row2Y, 82.0f, 12.0f};
	bool TurboHover = m_BtnTurbo.Inside(m_MousePos);
	ColorRGBA TurboBg = TurboHover ? ColorRGBA(0.85f, 0.55f, 0.1f, 0.95f) : ColorRGBA(0.65f, 0.38f, 0.05f, 0.85f);
	Graphics()->DrawRect(m_BtnTurbo.x, m_BtnTurbo.y, m_BtnTurbo.w, m_BtnTurbo.h, TurboBg, IGraphics::CORNER_ALL, 3.0f);
	TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	TextRender()->Text(m_BtnTurbo.x + 6.0f, m_BtnTurbo.y + 2.5f, 3.4f, "⚡ TURBO (T)");

	// Reset Button [↺ RESET]
	m_BtnRestart = {PanelX + 94.0f, Row2Y, 56.0f, 12.0f};
	bool RestartHover = m_BtnRestart.Inside(m_MousePos);
	ColorRGBA RestartBg = RestartHover ? ColorRGBA(0.4f, 0.2f, 0.5f, 0.95f) : ColorRGBA(0.25f, 0.12f, 0.32f, 0.85f);
	Graphics()->DrawRect(m_BtnRestart.x, m_BtnRestart.y, m_BtnRestart.w, m_BtnRestart.h, RestartBg, IGraphics::CORNER_ALL, 3.0f);
	TextRender()->TextColor(ColorRGBA(0.9f, 0.8f, 1.0f, 1.0f));
	TextRender()->Text(m_BtnRestart.x + 7.0f, m_BtnRestart.y + 2.5f, 3.4f, "↺ RESET");

	// Mini Progress Sparkline Chart
	const float ChartY = PanelY + 86.0f;
	const float ChartH = 26.0f;
	Graphics()->DrawRect(PanelX + 8.0f, ChartY, BarW, ChartH, ColorRGBA(0.06f, 0.09f, 0.14f, 0.85f), IGraphics::CORNER_ALL, 3.0f);

	TextRender()->TextColor(ColorRGBA(0.45f, 0.60f, 0.75f, 0.9f));
	TextRender()->Text(PanelX + 11.0f, ChartY + 2.0f, 3.0f, "LEARNING PROGRESS CURVE (X DISTANCE)");

	// Draw historical bars if available
	float MaxGraph = std::max(m_AllTimeMaxX * 1.1f, 800.0f);
	if(m_HistoryCount > 0)
	{
		float StepW = (BarW - 12.0f) / MAX_HISTORY;
		for(int h = 0; h < m_HistoryCount; h++)
		{
			float BarVal = std::clamp(m_aHistoryDist[h] / MaxGraph, 0.08f, 1.0f);
			float BarX = PanelX + 11.0f + h * StepW;
			float BarHeight = 14.0f * BarVal;
			Graphics()->DrawRect(BarX, ChartY + ChartH - 4.0f - BarHeight, StepW - 1.0f, BarHeight, ColorRGBA(0.2f, 0.8f, 1.0f, 0.75f), IGraphics::CORNER_NONE, 0.0f);
		}
	}
	else if(LeadX > 0.0f)
	{
		float NormVal = std::clamp(LeadX / MaxGraph, 0.02f, 1.0f);
		Graphics()->DrawRect(PanelX + 12.0f, ChartY + 12.0f, (BarW - 8.0f) * NormVal, 6.0f, ColorRGBA(0.2f, 0.75f, 1.0f, 0.85f), IGraphics::CORNER_ALL, 2.0f);
	}

	// Footer hotkeys & Mouse Status
	if(m_MouseUnlocked)
	{
		TextRender()->TextColor(ColorRGBA(0.3f, 1.0f, 0.5f, 1.0f));
		TextRender()->Text(PanelX + 8.0f, PanelY + PanelH - 9.0f, 3.2f, "● MOUSE ACTIVE (Click buttons | Alt to lock)");
		RenderTools()->RenderCursor(Ui()->MousePos(), 24.0f);
	}
	else
	{
		TextRender()->TextColor(ColorRGBA(0.5f, 0.6f, 0.7f, 0.8f));
		TextRender()->Text(PanelX + 8.0f, PanelY + PanelH - 9.0f, 3.0f, "[Alt] Free Mouse | [H] Close | [T] Turbo");
	}
}
