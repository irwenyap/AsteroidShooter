/*********************************************************************
 * \file		InputManager.cpp
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

#include "InputManager.hpp"

InputManager& InputManager::GetInstance()
{
	static InputManager inputManager;
	return inputManager;
}

void InputManager::Initialise(GLFWwindow* window)
{
	glfwSetKeyCallback(window, KeyCallback);
	glfwSetMouseButtonCallback(window, MouseButtonCallback);
	glfwSetCursorPosCallback(window, CursorPositionCallback);
	glfwSetScrollCallback(window, ScrollCallback);
}

void InputManager::Update()
{
	prevKeyStates = keyStates;
	prevMouseButtonStates = mouseButtonStates;

	glfwPollEvents();
}

bool InputManager::GetKeyDown(int key)
{
	return keyStates[key] && !prevKeyStates[key];
}

bool InputManager::GetKey(int key)
{
	auto it = keyStates.find(key);
	return it != keyStates.end() && it->second;
}

bool InputManager::GetMouseDown(int button)
{
	return mouseButtonStates[button] && !prevMouseButtonStates[button];
}

double InputManager::GetMouseX() const
{
	return cursorX;
}

double InputManager::GetMouseY() const
{
	return cursorY;
}