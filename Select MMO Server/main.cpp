// main.cpp
#include "SelectServer.hpp"
#include "Game.hpp"
#include "Logger.hpp"

#include <cstdio>
#include <timeapi.h>

#pragma comment(lib, "winmm.lib")

int main(void)
{
	SelectServer *server;

	timeBeginPeriod(1);
	server = nullptr;

	Logger::get_instance().set_threshold(Logger::LogLevel::Info);
	if (!Logger::get_instance().set_backend(Logger::SinkBackend::STDIO))
		return EXIT_FAILURE;
	if (!Logger::get_instance().set_target_file("Samplelog.log"))
		return EXIT_FAILURE;

	server = new SelectServer(5000);

	Game game(*server);

	std::printf("[Main] Starting game loop.\n");

	game.run();

	delete server;

	std::printf("[Main] Exiting.\n");
	timeEndPeriod(1);
	return 0;
}
