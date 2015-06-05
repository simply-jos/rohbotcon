#pragma once

#include "RohbotLib/Client/RohbotClient.hpp"

using namespace RohbotLib;

struct _win;
typedef _win WINDOW;

class ChatScreen
{
public:
	ChatScreen(RohbotClient& client);
	~ChatScreen();

	void Frame();
	void Draw();

private:
	RohbotClient& m_client;

	WINDOW* m_tabWindow;
	WINDOW* m_chatWindow;
	WINDOW* m_chatBarWindow;
	WINDOW* m_userlistWindow;

	int m_scrwidth, m_scrheight;

	std::string m_chatInputBuffer;

	void MouseClicked(int x, int y);

	void InitWindows();
	void OnResized(int w, int h);

	void DrawTabs();
	void DrawUserlist();

	void DrawChat();
	void DrawChatbar();
};