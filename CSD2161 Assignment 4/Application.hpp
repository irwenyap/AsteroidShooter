#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <string>

extern std::string g_PlayerName;

class Application {
public:
	Application() = default;
	~Application() = default;

	void Run();
};

#endif