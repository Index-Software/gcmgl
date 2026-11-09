//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "WindowPS3.h"
#include "dbg.h"
#include "utils/UtlMemory.h"
#include <sys/process.h>
#include <sysutil/video.h>
#include <io/pad.h>

CWindowPS3::CWindowPS3() :
	m_IsRunning(false),
	m_Width(0),
	m_Height(0)
{
}

CWindowPS3::~CWindowPS3()
{
	Shutdown();
}

bool CWindowPS3::Init(const WindowConfig_t& windowConfig)
{
	if (ioPadInit(7) != 0)
	{
		Error("[CWindowPS3] Failed to initialize pad system\n");
		
		return false;
	}

	sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, SysutilExitCallback, this);

	m_Width = windowConfig.m_Width;
	m_Height = windowConfig.m_Height;

	m_IsRunning = true;

	return true;
}

void CWindowPS3::Shutdown()
{
	m_IsRunning = false;

	sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
	ioPadEnd();
}

void CWindowPS3::PollEvents()
{
	sysUtilCheckCallback();
}

void CWindowPS3::MakeContextCurrent()
{
	Msg("MakeContextCurrent() is a stub\n");
}

bool CWindowPS3::ShouldClose()
{
	return !m_IsRunning;
}

void* CWindowPS3::GetNativeHandle()
{
	Msg("GetNativeHandle() is a stub\n");

	return GCMGL_NULL;
}

void CWindowPS3::GetSize(uint32& width, uint32& height) const
{
	width = m_Width;
	height = m_Height;
}

void CWindowPS3::GetFramebufferSize(uint32& width, uint32& height) const
{
	width = m_Width;
	height = m_Height;
}

void CWindowPS3::SysutilExitCallback(
	u64 status,
	u64 param,
	void* pUserData)
{
	CWindowPS3* pWindow = reinterpret_cast<CWindowPS3*>(pUserData);
	Msg(
		"SysutilExitCallback triggered with status=0x%lx, param=0x%lx\n",
		status,
		param);

	switch (status)
	{
		case SYSUTIL_EXIT_GAME:
		{
			Msg("SYSUTIL_EXIT_GAME status received\n");
			if (pWindow != GCMGL_NULL)
			{
				pWindow->m_IsRunning = false;
			}

			break;
		}
		case SYSUTIL_DRAW_BEGIN:
		{
			Msg("SYSUTIL_DRAW_BEGIN received\n");

			break;
		}
		case SYSUTIL_DRAW_END:
		{
			Msg("SYSUTIL_DRAW_END received\n");

			break;
		}
		default:
		{
			Warning("[CWindowPS3] Unknown status received: 0x%lx\n", status);

			break;
		}
	}
}

IWindow* CreateWindowInstance()
{
	void* pMemory = CUtlMemory::Alloc(sizeof(CWindowPS3));
	if (pMemory)
	{
		return new(pMemory) CWindowPS3();
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