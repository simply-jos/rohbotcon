#include "RohbotLib/Client/RohbotClient.hpp"
#include "pdcurses/curses.h"
#include "Gui/Screen/ChatScreen.hpp"

using namespace RohbotLib;

int main(char** argv, int argc)
{
	bool connected = false;

	while (!connected)
	{
		RohbotClient client;

		client.SetLoggedInCallback([&](const RohbotUser &user)
		{
			//printf("Logged in as %s\n", user.GetUsername().c_str());
		});

		client.SetOtherLoggedInCallback([&](const RohbotUser &user)
		{
			//printf("%s entered chat.\n", user.GetUsername().c_str());
		});

		client.SetOtherLoggedOutCallback([&](const RohbotUser &user)
		{
			//printf("%s disconnected.\n", user.GetUsername().c_str());
		});

		client.SetSystemMessageReceivedCallback([&](std::string message)
		{
			//printf("::%s\n", message.c_str());
		});

		try
		{
			client.Connect("fpp.literallybrian.com", "/ws/", 443, "", "");
		}

		catch (std::exception exception)
		{
			printf("Failed to connect to Rohbot");
			continue;
		}

		ChatScreen chatscreen(client);
		while (client.LoggedIn())
		{
			client.Think();

			chatscreen.Frame();
			chatscreen.Draw();

			Sleep(16);
		}
	}
}