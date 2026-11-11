//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "RSXHeap.h"

#include "OffsetAllocator/offsetAllocator.hpp"
#include "utils/UtlMemory.h"
#include <new>
#include <stdlib.h>

static const uint32 NO_SPACE = 0xffffffff;
static const uint32 s_MaxAlignment = 128;

bool CRSXHeap::Init(uint32 size)
{
	m_pBase = rsxMemalign(s_MaxAlignment, size);
	if (!m_pBase)
	{
		return false;
	}

	rsxAddressToOffset(m_pBase, &m_BaseRSXOffset);

	void* pMem = CUtlMemory::Alloc(sizeof(OffsetAllocator::Allocator));
	if (!pMem)
	{
		rsxFree(m_pBase);
		m_pBase = GCMGL_NULL;

		return false;
	}

	m_pAllocator = new(pMem) OffsetAllocator::Allocator(size);

	return true;
}

void CRSXHeap::Shutdown()
{
	if (m_pAllocator)
	{
		static_cast<OffsetAllocator::Allocator*>(m_pAllocator)->~Allocator();
		CUtlMemory::Free(m_pAllocator);
		m_pAllocator = GCMGL_NULL;
	}
	if (m_pBase)
	{
		rsxFree(m_pBase);
		m_pBase = GCMGL_NULL;
	}
}

RSXAllocation_t CRSXHeap::Alloc(uint32 size, uint32 alignment)
{
	RSXAllocation_t rsxAllocation;
	if (alignment > s_MaxAlignment)
	{
		return rsxAllocation;
	}

	const uint32 alignedSize =
		(size + s_MaxAlignment - 1) & ~(s_MaxAlignment - 1);
	OffsetAllocator::Allocator* pAlloc =
		static_cast<OffsetAllocator::Allocator*>(m_pAllocator);
	OffsetAllocator::Allocation allocation = pAlloc->allocate(alignedSize);

	rsxAllocation.m_Metadata = allocation.metadata;
	if (allocation.offset == NO_SPACE)
	{
		return rsxAllocation;
	}

	rsxAllocation.m_Offset = m_BaseRSXOffset + allocation.offset;
	rsxAllocation.m_pPtr = reinterpret_cast<uint8*>(m_pBase) + allocation.offset;

	return rsxAllocation;
}

void CRSXHeap::Free(RSXAllocation_t rsxAllocation)
{
	if (rsxAllocation.m_pPtr == GCMGL_NULL)
	{
		return;
	}

	OffsetAllocator::Allocation allocation;
	allocation.offset = static_cast<uint32>(
		reinterpret_cast<uint8*>(rsxAllocation.m_pPtr) -
		reinterpret_cast<uint8*>(m_pBase));
	allocation.metadata = rsxAllocation.m_Metadata;
	static_cast<OffsetAllocator::Allocator*>(m_pAllocator)->free(allocation);
}