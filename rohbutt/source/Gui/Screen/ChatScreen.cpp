#include "ChatScreen.hpp"

#include "pdcurses/curses.h"
#include "pdcurses/panel.h"

#define USERLIST_WIDTH 20

ChatScreen::ChatScreen(RohbotClient& client) : m_client(client)
{
	setlocale(LC_ALL, "");

	initscr();

	if (has_colors())
	{
		start_color();
		init_color(COLOR_GREEN, 0, 255, 0);
		init_color(COLOR_BLUE, 0, 100, 255);
		init_pair(1, COLOR_GREEN, COLOR_BLACK);
		init_pair(2, COLOR_GREEN, COLOR_BLACK);
	}
	
	curs_set(0);
	nodelay(stdscr, true);
	cbreak();
	noecho();
	keypad(stdscr, true);

	mousemask(ALL_MOUSE_EVENTS, 0);
}

ChatScreen::~ChatScreen()
{
	endwin();
}

void ChatScreen::InitWindows()
{
	m_tabWindow = newwin(2, m_scrwidth, 0, 0);
	m_chatWindow = newwin(m_scrheight - 3 - 2, m_scrwidth - USERLIST_WIDTH, 2, 0);
	m_chatBarWindow = newwin(3, m_scrwidth, m_scrheight - 3, 0);
	m_userlistWindow = newwin(m_scrheight - 2 - 3, USERLIST_WIDTH, 2, m_scrwidth - USERLIST_WIDTH);

	nodelay(m_tabWindow, true);
}

void ChatScreen::OnResized(int w, int h)
{
	m_scrwidth = w;
	m_scrheight = h;

	InitWindows();
}

void ChatScreen::MouseClicked(int x, int y)
{
	//Check if we're switching rooms
	int currentPosX = 0;
	for (auto chatroom : m_client.GetChatrooms())
	{
		std::string name = chatroom->GetName();
		currentPosX += 3;

		int left = currentPosX;
		int right = left + name.length();

		currentPosX += name.length();

		if (x >= left && x <= right &&
			y <= 1)
			{
				m_client.SetPrimaryChatroom(chatroom);
				break;
			}
	}
}

void ChatScreen::Frame()
{
	char ch = getch();

	while (ch != -1)
	{
		switch (ch)
		{
		case 8: //Backspace
			if (m_chatInputBuffer.size() > 0)
				m_chatInputBuffer = m_chatInputBuffer.substr(0, m_chatInputBuffer.length() - 1);
			break;

		case 10: //Enter
			m_client.GetPrimaryChatroom().lock()->SendChat(m_chatInputBuffer);
			m_chatInputBuffer = "";
			break;

		case 95: //Shift + tab
			m_client.SetPrimaryChatroomDelta(1);
			break;

		case 27:
		{
			MEVENT evt;
			if (nc_getmouse(&evt) == OK)
				if (evt.bstate & BUTTON1_CLICKED)
					MouseClicked(evt.x, evt.y);

			break;
		}

		default:
			m_chatInputBuffer += ch;
		}

		ch = getch();
	}
}

void ChatScreen::Draw()
{
	int h, w;
	getmaxyx(stdscr, h, w);

	if (w != m_scrwidth || h != m_scrheight)
		OnResized(w, h);

	DrawTabs();

	DrawChat();
	DrawChatbar();

	DrawUserlist();
}

void ChatScreen::DrawTabs()
{
	wclear(m_tabWindow);

	//Draw bottom outline
	for (int x = 0; x < m_scrwidth; x++)
		mvwprintw(m_tabWindow, 1, x, "_");

	//Draw tabs
	int currentPosX = 0;
	for (auto chatroom : m_client.GetChatrooms())
	{
		std::string name = chatroom->GetName();
		bool selectedChatroom = m_client.GetPrimaryChatroom().lock() == chatroom;

		//Draw left padding
		mvwprintw(m_tabWindow, 0, currentPosX, " / ");

		currentPosX += 3;

		if (selectedChatroom)
			wattron(m_tabWindow, COLOR_PAIR(1));

		//Draw title
		const char* c = name.c_str();
		do
		{
			mvwprintw(m_tabWindow, 0, currentPosX, "%c", *c);
		} while (*c++ != 0 && currentPosX++);

		if (selectedChatroom)
			wattroff(m_tabWindow, COLOR_PAIR(1));

		//Draw right padding
		mvwprintw(m_tabWindow, 0, currentPosX, " / ");
	}

	wrefresh(m_tabWindow);
}

void ChatScreen::DrawUserlist()
{
	wclear(m_userlistWindow);

	//Draw left padding
	for (int y = 0; y < m_scrheight - 3 - 2; y++)
		mvwprintw(m_userlistWindow, y, 0, "|");

	//Draw users
	int line = 0;
	std::vector< std::shared_ptr<RohbotUser> > users = m_client.GetPrimaryChatroom().lock()->GetUsers();

	wattron(m_userlistWindow, COLOR_PAIR(1));

	for (auto user : users)
	{
		if (line == m_scrheight - 3 - 2)
			break;

		mvwprintw(m_userlistWindow, line, 1, "%s", user->GetUsername().c_str());

		line++;
	}

	wattroff(m_userlistWindow, COLOR_PAIR(1));

	wrefresh(m_userlistWindow);
}

const char* ltrim(const char* input)
{
	const char* trim = input;
	while (*trim == ' ')
		trim++;

	return trim;
}

static std::vector<std::string> GetWrappedLines(std::string text, int width)
{
	std::vector<std::string> lines;
	std::string currentLine;

	for (int i = 0; i < text.length();)
	{
		char c = text[i];
		if (currentLine.length() < width && c != '\n')
		{
			currentLine += c;
			i++;
		}
		else if (c == '\n')
		{
			text = text.substr(currentLine.length() + 1);

			lines.push_back(ltrim(currentLine.c_str()));
			currentLine = "";
			i = 0;
		}
		else
		{
			//Find the final space in the current line, that's
			//where we need to break.
			int finalSpaceIndex = currentLine.rfind(" ");

			//If we couldn't find it..
			if (finalSpaceIndex <= 0)
				finalSpaceIndex = currentLine.length() - 1;

			currentLine = currentLine.substr(0, finalSpaceIndex);

			lines.push_back(ltrim(currentLine.c_str()));

			text = text.substr(finalSpaceIndex);
			currentLine = "";
			i = 0;
		}
	}

	if (!currentLine.empty())
		lines.push_back(ltrim(currentLine.c_str()));

	return lines;
}

static int DrawChatEntry(WINDOW* window, int line, std::string user, std::vector<std::string> lines)
{
	int startLine = line;

	for (auto lineStr : lines)
		mvwprintw(window, line++, 0, "%s", lineStr.c_str());

	//Draw colored username
	wattron(window, COLOR_PAIR(2));
	mvwprintw(window, startLine, 0, "%s", (user + ": ").c_str());
	wattroff(window, COLOR_PAIR(2));

	return lines.size();
}

static std::string HTMLDecode(std::string _in)
{
	std::string in = _in;

	for (int i = 0; i < in.length(); i++)
	{
		if (in[i] == '&')
		{
			if (in.substr(i, 4) == "&lt;")
			{
				in.erase(i, 4);
				in.insert(i, "<");
				i -= 3;
			} else if (in.substr(i, 4) == "&gt;")
			{
				in.erase(i, 4);
				in.insert(i, ">");
				i -= 3;
			} else if (in.substr(i, 6) == "&quot;")
			{
				in.erase(i, 6);
				in.insert(i, "\"");
				i -= 3;
			} else if (in.substr(i, 5) == "&amp;")
			{
				in.erase(i, 5);
				in.insert(i, "&");
			}
			else
			{
				if (in[i + 1] == '#')
				{
					int end = in.find(";", i);

					if (end != -1)
					{
						std::string num = in.substr(i + 2, end - i - 2);
						if (num[0] == 'x')
						{
							//hex

						}
						else
						{
							//dec
							char ch = atoi(num.c_str());

							in.erase(i, end - i + 1);
							in.insert(i, std::string() + ch);
						}
					}
				}
			}
		}
	}

	return in;
}

void ChatScreen::DrawChat()
{
	std::shared_ptr<Chatroom> primaryChatroom = m_client.GetPrimaryChatroom().lock();

	if (!primaryChatroom)
		return;

	wclear(m_chatWindow);

	struct DrawMessage
	{
		std::string user;
		std::vector<std::string> lines;
	};

	//Draw chat
	int lineSum = 0;
	int maxHeight = m_scrheight - 3 - 2;
	std::vector<Chatroom::ChatroomMessage> messages = primaryChatroom->GetMessages();
	std::vector<DrawMessage> drawMessages;

	auto T = messages.end();
	while (T != messages.begin())
	{
		--T;

		DrawMessage dm;

		dm.lines = GetWrappedLines(HTMLDecode(T->user + ": " + T->text), m_scrwidth - USERLIST_WIDTH);
		dm.user = T->user;

		drawMessages.push_back(dm);

		lineSum += dm.lines.size();

		if (lineSum > maxHeight)
		{
			lineSum = maxHeight;
			break;
		}
	}

	for (auto dm : drawMessages)
		lineSum -= DrawChatEntry(m_chatWindow, lineSum - dm.lines.size(), dm.user, dm.lines);

	wrefresh(m_chatWindow);
}

void ChatScreen::DrawChatbar()
{
	wclear(m_chatBarWindow);

	//Draw top padding
	int currentPosX = 0;
	while (currentPosX < m_scrwidth)
		mvwprintw(m_chatBarWindow, 0, currentPosX++, "_");

	//Draw input text
	mvwprintw(m_chatBarWindow, 1, 0, "%s", (m_chatInputBuffer + "_").c_str());

	wrefresh(m_chatBarWindow);
}