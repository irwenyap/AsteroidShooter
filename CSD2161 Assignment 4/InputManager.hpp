/*********************************************************************
 * \file		InputManager.hpp
 * \brief		Handles keyboard and mouse input
 *
 * \author		y.ziyangirwen, 2301345 (y.ziyangirwen@digipen.edu)
 * \date		1 September 2024
 *
 * \copyright	Copyright(C) 2024 DigiPen Institute of Technology.
 *				Reproduction or disclosure of this file or its
 *              contents without the prior written consent of DigiPen
 *              Institute of Technology is prohibited.
 *********************************************************************/

#ifndef INPUT_MANAGER_HPP
#define INPUT_MANAGER_HPP

#include <unordered_map>
#include "Graphics/Window.hpp"
#include "ImGui/imgui_impl_glfw.h"


class InputManager {
public:
	static InputManager& GetInstance();

    void Initialise(GLFWwindow* window);
    void Update();
    
	bool GetKeyDown(int key);
	bool GetKey(int key);

    bool GetMouseDown(int button);

    double GetMouseX() const;
    double GetMouseY() const;

private:
    InputManager() : cursorX(0.0), cursorY(0.0), scrollOffsetX(0.0), scrollOffsetY(0.0) {}

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        window, scancode, mods;
        InputManager::GetInstance().keyStates[key] = (action != GLFW_RELEASE);
        ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    }

    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        window, mods;
        InputManager::GetInstance().mouseButtonStates[button] = (action != GLFW_RELEASE);
        ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    }

    static void CursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
        window;
        InputManager::GetInstance().cursorX = xpos;
        InputManager::GetInstance().cursorY = ypos;
        ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
    }

    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        window;
        InputManager::GetInstance().scrollOffsetX += xoffset;
        InputManager::GetInstance().scrollOffsetY += yoffset;
        ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
    }

	std::unordered_map<int, bool> keyStates;
	std::unordered_map<int, bool> mouseButtonStates;

    std::unordered_map<int, bool> prevKeyStates;
    std::unordered_map<int, bool> prevMouseButtonStates;

	double cursorX, cursorY;
	double scrollOffsetX, scrollOffsetY;
};


#endif // !INPUT_MANAGER_HPP