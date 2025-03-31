#include "Application.hpp"

#include <iostream>
#include <thread>
#include <glad/glad.h>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "Core/Timer.hpp"
#include "Graphics/Window.hpp"
#include "InputManager.hpp"
#include "AsteroidScene.hpp"
#include "Networking/NetworkEngine.hpp"
#include "Graphics/GraphicsEngine.hpp"
#include "Events/EventQueue.hpp"

//std::pair<int, int> windowSize;

//static bool isHost = false;
//static bool isClient = false;
void ShowNetworkUI() {
	//static char ipAddress[64] = "127.0.0.1";
	//static char ipAddress[64] = "192.168.0.4";
	static char ipAddress[64] = "10.5.0.2";
	static std::string ipAddressString{};
	static char port[16] = "1234";
	auto& ne = NetworkEngine::GetInstance();

	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.8f);

	ImGui::Begin("Network", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

	if (!ne.isHosting && !ne.isClient) {
		ImGui::InputText("Port", port, IM_ARRAYSIZE(port));
		ImGui::SameLine();
		if (ImGui::Button("Host")) {
			ne.isHosting = NetworkEngine::GetInstance().Host(port);
			ipAddressString = NetworkEngine::GetInstance().GetIPAddress();
			//if (ne.isHosting) EventQueue::GetInstance().Push(std::make_unique<SpawnPlayerEvent>(NetworkEngine::GetInstance().GenerateID()));
			//std::cout << "Hosting on port " << port << std::endl;
		}

		ImGui::Separator();

		ImGui::InputText("IP Address", ipAddress, IM_ARRAYSIZE(ipAddress));
		ImGui::SameLine();
		ImGui::InputText("Server Port", port, IM_ARRAYSIZE(port));

		ImGui::SameLine();
		if (ImGui::Button("Connect")) {
			ne.isClient = NetworkEngine::GetInstance().Connect(ipAddress, port);
			ipAddressString = ipAddress;
			
			//std::cout << "Connecting to " << ipAddress << ":" << port << std::endl;
		}
	} else {
		if (ne.isHosting) {
			ImGui::Text("Server Started");
			std::string numClients = "Connected Clients: " + std::to_string(ne.GetInstance().GetNumConnectedClients());
			ImGui::Text(numClients.c_str());
		}
		else ImGui::Text("Client connected to");
		ImGui::Text(("IP Address: " + ipAddressString).c_str());
		std::string portText = "Port: " + std::string(port);
		ImGui::Text(portText.c_str());

		if (ne.isHosting) {
			if (ImGui::Button("Start Game")) {
				EventQueue::GetInstance().Push(std::make_unique<RequestStartGameEvent>());
			}
		}
	}
	ImGui::End();
}

void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
	window;
	glViewport(0, 0, width, height);
	//windowSize.first = width;
	//windowSize.second = height;
	GraphicsEngine::GetInstance().UpdateProjection(width, height);
	//std::cout << "Window resized: " << width << "x" << height << std::endl;
}

//using Tick = uint32_t;
//Tick simulationTick = 0; // global tick tracker
//Tick localTick = simulationTick; // for client

void Application::Run() {
	Timer timer;
	std::unique_ptr<Window> m_context;
	timer.Start();

	m_context = std::make_unique<Window>("Asteroid Shooter", 1920, 1080, false);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cerr << "Failed to initialize GLAD" << std::endl;
		return;
	}
	glViewport(0, 0, 1920, 1080);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//glfwSwapInterval(0);
	glfwSetFramebufferSizeCallback(m_context->GetWindow(), FramebufferSizeCallback);

	NetworkEngine::GetInstance().Initialize();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForOpenGL(m_context->GetWindow(), true);
	ImGui_ImplOpenGL3_Init("#version 460");

	InputManager::GetInstance().Initialise(m_context->GetWindow());

	AsteroidScene as;
	as.Initialize();

	//const int TARGET_FPS = 165;
	//const int FRAME_DURATION = 1000 / TARGET_FPS; // in milliseconds

	while (!glfwWindowShouldClose(m_context->GetWindow())) {
		//auto start = std::chrono::high_resolution_clock::now();
		if (InputManager::GetInstance().GetKeyDown(GLFW_KEY_ESCAPE)) break;

		timer.Update();
		InputManager::GetInstance().Update();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ShowNetworkUI();

		as.Update(timer.GetDeltaTime());
		for (int i = 0; i < timer.GetFixedSteps(); ++i) {
			//as.Update(timer.GetDeltaTime(), timer.GetFixedDT(), timer.GetFixedSteps());
			//if (isHost) {
			//	if (simulationTick % 120 == 0) { // Every 60 ticks (1 second) send sync.
					//NetworkEngine::GetInstance().SendTickSync(simulationTick);
					//std::cout << simulationTick << std::endl;
			//	}
			//} else if (isClient) {
				//std::cout << localTick << std::endl;
				//NetworkEngine::GetInstance().ProcessTickSync(simulationTick, localTick);
			//}
			as.FixedUpdate(timer.GetFixedDT());

			NetworkEngine::GetInstance().simulationTick++;
			NetworkEngine::GetInstance().localTick++;
		}
		as.Render();
		as.ProcessEvents();
		NetworkEngine::GetInstance().Update(timer.GetDeltaTime());

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


		m_context->SwapBuffers();
	}

	as.Exit();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	NetworkEngine::GetInstance().Exit();

	m_context.reset();
	glfwTerminate();
}
