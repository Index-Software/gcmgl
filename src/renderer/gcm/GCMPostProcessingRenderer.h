//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef GCM_POST_PROCESSING_RENDERER_H
#define GCM_POST_PROCESSING_RENDERER_H

#pragma once

#include <rsx/rsx.h>
#include "platform.h"
#include "utils/RSXHeap.h"

struct GCMPostProcessState_t
{
	uint32 m_FragmentProgramOffset;
	uint32 m_OffscreenColorOffset;
	uint32 m_OffscreenDepthOffset;
	uint32 m_QuadVerticesOffset;
	void* m_pVertexProgramAligned;
	void* m_pVertexProgramUCode;
	void* m_pFragmentProgramAligned;
	void* m_pFragmentProgramBuffer;
	void* m_pOffscreenColor;
	void* m_pOffscreenDepth;
	void* m_pQuadVertices;
	const rsxVertexProgram* m_pVertexProgram;
	const rsxFragmentProgram* m_pFragmentProgram;
	RSXAllocation_t m_FragmentProgramAlloc;
	RSXAllocation_t m_FragmentProgramBufferAlloc;
	RSXAllocation_t m_OffscreenColorAlloc;
	RSXAllocation_t m_OffscreenDepthAlloc;
	RSXAllocation_t m_QuadVerticesAlloc;
};

class CGCMPostProcessingRenderer
{
public:
	static GCMPostProcessState_t InitState(CRSXHeap& staticHeap);
	static void ShutdownState(
		GCMPostProcessState_t& state,
		CRSXHeap& staticHeap);
	static void Begin(const GCMPostProcessState_t& state);
	static void End(const GCMPostProcessState_t& state);
};

#endif // GCM_POST_PROCESSING_RENDERER_H