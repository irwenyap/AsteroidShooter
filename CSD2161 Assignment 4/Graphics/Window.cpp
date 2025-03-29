#include "Window.hpp"
#include <iostream>

Window::Window(std::string title, int width, int height, bool) :

	m_window(nullptr, glfwDestroyWindow),
	m_title(title)
{
	if (!glfwInit()) {
		std::cerr << "[Window] Failed to initialize GLFW\n";
		return;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	m_window.reset(glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr));

	if (!m_window) {
		std::cerr << "[Window] Failed to create GLFW window\n";
		glfwTerminate();
		return;
	}

	glfwMakeContextCurrent(m_window.get());

	//glEnable(GL_DEPTH_TEST);
	glfwSetInputMode(m_window.get(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

GLFWwindow* Window::GetWindow() const
{
	return m_window.get();
}

void Window::SwapBuffers()
{
	glfwSwapBuffers(m_window.get());
}

void Window::SetTitle(const char* title)
{
	glfwSetWindowTitle(m_window.get(), (m_title + std::string(title)).c_str());
}