#ifndef GAME_CLIENT_COMPONENTS_AI_HUD_H
#define GAME_CLIENT_COMPONENTS_AI_HUD_H

#include <game/client/component.h>
#include <base/vmath.h>

class CAiHud : public CComponent
{
private:
	static constexpr int MAX_HISTORY = 24;
	float m_aHistoryDist[MAX_HISTORY] = {0};
	int m_HistoryCount = 0;
	int m_LastRecordedGen = 0;
	float m_AllTimeMaxX = 100.0f;
	int m_CurrentGen = 1;

	bool m_MouseUnlocked = false;
	vec2 m_MousePos = vec2(0, 0);

	struct SRect
	{
		float x = 0, y = 0, w = 0, h = 0;
		bool Inside(vec2 p) const { return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h; }
	};

	SRect m_BtnClose;
	SRect m_BtnMinPill;
	SRect m_BtnSpeedDown;
	SRect m_BtnSpeedUp;
	SRect m_BtnTurbo;
	SRect m_BtnCam;
	SRect m_BtnRestart;
	SRect m_BtnTurboResume;

	void AdjustSpeed(int Delta);
	void ToggleTurbo();
	void SendAiCommand(const char *pCmd);
	bool HandleClick(vec2 MousePos);
	void UpdateMousePos();
	void RenderTurboDashboard(int AliveBots, int TotalBots, float LeadX, const char *pLeaderName);

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	bool OnInput(const IInput::CEvent &Event) override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	void OnReset() override;
};

#endif // GAME_CLIENT_COMPONENTS_AI_HUD_H
