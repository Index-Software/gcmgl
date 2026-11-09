//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "WindowGLFW.h"
#include "dbg.h"
#include "utils/UtlMemory.h"

CWindowGLFW::CWindowGLFW() :
	m_pWindow(GCMGL_NULL)
{
}

CWindowGLFW::~CWindowGLFW()
{
	Shutdown();
}

bool CWindowGLFW::Init(const WindowConfig_t& windowConfig)
{
	if (!glfwInit())
	{
		Error("[CWindowGLFW] Failed to initialize GLFW\n");
		
		return false;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
	glfwWindowHint(GLFW_DEPTH_BITS, 24);

	m_pWindow = glfwCreateWindow(
		windowConfig.m_Width,
		windowConfig.m_Height,
		windowConfig.m_Title.Get(),
		GCMGL_NULL,
		GCMGL_NULL);
	if (m_pWindow == GCMGL_NULL)
	{
		Error("[CWindowGLFW] Failed to create OpenGL 3.2 context\n");

		return false;
	}

	glfwMakeContextCurrent(m_pWindow);

	return true;
}

void CWindowGLFW::Shutdown()
{
	if (m_pWindow != GCMGL_NULL)
	{
		glfwDestroyWindow(m_pWindow);
		m_pWindow = GCMGL_NULL;
	}

	glfwTerminate();
}

void CWindowGLFW::PollEvents()
{
	glfwPollEvents();
}

void CWindowGLFW::MakeContextCurrent()
{
	if (m_pWindow != GCMGL_NULL)
	{
		glfwMakeContextCurrent(m_pWindow);
	}
}

bool CWindowGLFW::ShouldClose()
{
	if (m_pWindow == GCMGL_NULL)
	{
		return true;
	}

	return glfwWindowShouldClose(m_pWindow) != 0;
}

void* CWindowGLFW::GetNativeHandle()
{
	return reinterpret_cast<void*>(m_pWindow);
}

void CWindowGLFW::GetSize(uint32& width, uint32& height) const
{
	int w = 0;
	int h = 0;
	if (m_pWindow)
	{
		glfwGetWindowSize(m_pWindow, &w, &h);
	}
	width = static_cast<uint32>(w);
	height = static_cast<uint32>(h);
}

void CWindowGLFW::GetFramebufferSize(uint32& width, uint32& height) const
{
	int w = 0;
	int h = 0;
	if (m_pWindow)
	{
		glfwGetFramebufferSize(m_pWindow, &w, &h);
	}
	width = static_cast<uint32>(w);
	height = static_cast<uint32>(h);
}

IWindow* CreateWindowInstance()
{
	void* pMemory = CUtlMemory::Alloc(sizeof(CWindowGLFW));
	if (pMemory)
	{
		return new(pMemory) CWindowGLFW();
	}

	return GCMGL_NULL;
}

void DestroyWindowInstance(IWindow* pWindow)
{
	if (pWindow)
	{
		pWindow->~IWindow();
		CUtlMemory::Free(pWindow);
	}
}