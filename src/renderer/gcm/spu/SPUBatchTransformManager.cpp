//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "SPUBatchTransformManager.h"
#include "dbg.h"
#include "platform/ps3/spu/SPUUtils.h"
#include "utils/UtlMemory.h"
#include "renderer/Renderer.h"
#include "rsxutil/rsxutil.h"
#include <string.h>
#include <sys/thread.h>
#include <sys/systime.h>
#include <sys/event_queue.h>

extern "C"
{
	extern uint8_t _binary_SPUBatchTransformJob_elf_start[];
	extern uint8_t _binary_SPUBatchTransformJob_elf_size[];
}

CSPUBatchTransformManager::CSPUBatchTransformManager()
{
	for (uint32 i = 0; i < s_BatchTransformSPUCount; i++)
	{
		m_pBatchJobs[i] = GCMGL_NULL;
		m_SPUThreadIDs[i] = 0;
		m_EventQueues[i] = 0;
	}
	m_SPUGroupID = 0;
	m_IsShuttingDown = false;
}

CSPUBatchTransformManager::~CSPUBatchTransformManager()
{
	Shutdown();
}

bool CSPUBatchTransformManager::Init()
{
	const int32 spuInitializeResult = sysSpuInitialize(6, 0);
	if (spuInitializeResult < 0)
	{
		return false;
	}

	const int32 spuImageImportResult = sysSpuImageImport(
		&m_SPUImage,
		_binary_SPUBatchTransformJob_elf_start,
		0);
	if (spuImageImportResult < 0)
	{
		return false;
	}

	sysSpuThreadGroupAttribute groupAttr = { 0 };
	groupAttr.name = const_cast<char*>("gcmBatchGroup");

	const int32 spuThreadGroupCreateResult = sysSpuThreadGroupCreate(
		&m_SPUGroupID,
		s_BatchTransformSPUCount,
		100,
		&groupAttr);
	if (spuThreadGroupCreateResult < 0)
	{
		sysSpuImageClose(&m_SPUImage);

		return false;
	}

	for (uint32 i = 0; i < s_BatchTransformSPUCount; i++)
	{
		sysSpuThreadAttribute spuThreadAttribute = { 0 };
		spuThreadAttribute.name = const_cast<char*>("gcmBatchSpu");
		spuThreadAttribute.option = SPU_THREAD_ATTR_NONE;

		m_pBatchJobs[i] = static_cast<SPUBatchTransformJob_t*>(
			CUtlMemory::AlignedAlloc(sizeof(SPUBatchTransformJob_t), 128));
		if (!m_pBatchJobs[i])
		{
			sysSpuThreadGroupDestroy(m_SPUGroupID);
			sysSpuImageClose(&m_SPUImage);

			return false;
		}
		memset(m_pBatchJobs[i], 0, sizeof(SPUBatchTransformJob_t));

		m_pBatchJobs[i]->m_Command = SPU_BATCH_CMD_IDLE;
		m_pBatchJobs[i]->m_Status = SPU_BATCH_STATUS_IDLE;

		sysSpuThreadArgument spuThreadArgument = { 0 };
		spuThreadArgument.arg0 = CSPUUtils::PtrToEA(m_pBatchJobs[i]);

		const int32 spuThreadInitializeResult = sysSpuThreadInitialize(
			&m_SPUThreadIDs[i],
			m_SPUGroupID,
			i,
			&m_SPUImage,
			&spuThreadAttribute,
			&spuThreadArgument);
		if (spuThreadInitializeResult < 0)
		{
			for (uint32 j = 0; j <= i; j++)
			{
				if (m_pBatchJobs[j])
				{
					CUtlMemory::AlignedFree(m_pBatchJobs[j]);
					m_pBatchJobs[j] = GCMGL_NULL;
				}
			}

			sysSpuThreadGroupDestroy(m_SPUGroupID);
			sysSpuImageClose(&m_SPUImage);

			return false;
		}

		sysSpuThreadSetConfiguration(
			m_SPUThreadIDs[i],
			SPU_SIGNAL1_OVERWRITE | SPU_SIGNAL2_OVERWRITE);

		sys_event_queue_attr_t eventQueueAttr;
		eventQueueAttr.attr_protocol = SYS_EVENT_QUEUE_FIFO;
		eventQueueAttr.type = SYS_EVENT_QUEUE_PPU;
		eventQueueAttr.name[0] = '\0';

		const int32 eventQueueCreateResult = sysEventQueueCreate(
			&m_EventQueues[i],
			&eventQueueAttr,
			SYS_EVENT_QUEUE_KEY_LOCAL,
			1);
		const int32 spuThreadConnectEventResult = (eventQueueCreateResult >= 0)
			? sysSpuThreadConnectEvent(
				m_SPUThreadIDs[i],
				m_EventQueues[i],
				SPU_THREAD_EVENT_USER,
				0)
			: -1;

		if (eventQueueCreateResult < 0 || spuThreadConnectEventResult < 0)
		{
			for (uint32 j = 0; j <= i; j++)
			{
				if (m_EventQueues[j])
				{
					sysEventQueueDestroy(m_EventQueues[j], 0);
					m_EventQueues[j] = 0;
				}

				if (m_pBatchJobs[j])
				{
					CUtlMemory::AlignedFree(m_pBatchJobs[j]);
					m_pBatchJobs[j] = GCMGL_NULL;
				}
			}

			sysSpuThreadGroupDestroy(m_SPUGroupID);
			sysSpuImageClose(&m_SPUImage);

			return false;
		}
	}

	const int32 spuThreadGroupStartResult = sysSpuThreadGroupStart(
		m_SPUGroupID);
	if (spuThreadGroupStartResult < 0)
	{
		for (uint32 i = 0; i < s_BatchTransformSPUCount; i++)
		{
			if (m_EventQueues[i])
			{
				sysEventQueueDestroy(m_EventQueues[i], 0);
				m_EventQueues[i] = 0;
			}

			if (m_pBatchJobs[i])
			{
				CUtlMemory::AlignedFree(m_pBatchJobs[i]);
				m_pBatchJobs[i] = GCMGL_NULL;
			}
		}

		sysSpuThreadGroupDestroy(m_SPUGroupID);
		sysSpuImageClose(&m_SPUImage);

		return false;
	}

	return true;
}

void CSPUBatchTransformManager::Shutdown()
{
	m_IsShuttingDown = true;

	if (m_SPUGroupID != 0)
	{
		for (uint32 i = 0; i < s_BatchTransformSPUCount; i++)
		{
			if (m_pBatchJobs[i])
			{
				__sync_synchronize();

				if (m_pBatchJobs[i]->m_Status != SPU_BATCH_STATUS_IDLE &&
					m_pBatchJobs[i]->m_Status != SPU_BATCH_STATUS_DONE)
				{
					m_pBatchJobs[i]->m_Command = SPU_BATCH_CMD_TERMINATE;
					m_pBatchJobs[i]->m_Status = SPU_BATCH_STATUS_BUSY;

					__sync_synchronize();

					if (m_SPUThreadIDs[i] != 0)
					{
						sysSpuThreadWriteSignal(m_SPUThreadIDs[i], 0, 1);
					}
				}
			}
		}

		bool hasActiveJobs = true;
		for (uint32 i = 0; i < 1000; i++)
		{
			__sync_synchronize();

			hasActiveJobs = false;

			for (uint32 j = 0; j < s_BatchTransformSPUCount; j++)
			{
				if (m_pBatchJobs[j] &&
					m_pBatchJobs[j]->m_Status != SPU_BATCH_STATUS_IDLE &&
					m_pBatchJobs[j]->m_Status != SPU_BATCH_STATUS_DONE)
				{
					hasActiveJobs = true;

					break;
				}
			}

			if (!hasActiveJobs)
			{
				break;
			}

			sysUsleep(1000); // 1ms
		}

		sysUsleep(10000); // 10ms

		sysSpuThreadGroupTerminate(m_SPUGroupID, 0);

		uint32 cause;
		uint32 status;
		sysSpuThreadGroupJoin(m_SPUGroupID, &cause, &status);
		sysSpuThreadGroupDestroy(m_SPUGroupID);
		m_SPUGroupID = 0;
	}

	for (uint32 i = 0; i < s_BatchTransformSPUCount; i++)
	{
		if (m_pBatchJobs[i])
		{
			CUtlMemory::AlignedFree(m_pBatchJobs[i]);
			m_pBatchJobs[i] = GCMGL_NULL;
		}

		if (m_EventQueues[i])
		{
			sysEventQueueDestroy(m_EventQueues[i], 0);
			m_EventQueues[i] = 0;
		}

		m_SPUThreadIDs[i] = 0;
	}

	sysSpuImageClose(&m_SPUImage);
}

SPUResult_t::Enum CSPUBatchTransformManager::BeginBatch(
	const char* pSrcVertices,
	const uint32* pSrcIndices,
	const BatchChunkTransform_t* pTransforms,
	char* pDstVertices,
	uint32* pDstIndices,
	uint32 vertexCount,
	uint32 indexCount,
	uint32 batchCount,
	uint32 vertexStride,
	uint32 vertexPosOffset,
	uint32 baseVertex)
{
	if (m_IsShuttingDown)
	{
		return SPUResult_t::NotUsed;
	}

	if (vertexCount == 0 || batchCount == 0)
	{
		return SPUResult_t::NotUsed;
	}

	if ((vertexStride % 16) != 0)
	{
		AssertMsg(
			0,
			"CSPUBatchTransformManager: Vertex stride is not a multiple of 16");

		return SPUResult_t::Error;
	}

	if ((indexCount % 4) != 0)
	{
		AssertMsg(
			0,
			"CSPUBatchTransformManager: Index count is not a multiple of 4");

		return SPUResult_t::Error;
	}
	
	if ((reinterpret_cast<uintptr_t>(pSrcVertices) % 16) != 0)
	{
		AssertMsg(
			0,
			"CSPUBatchTransformManager: Source vertices are not 16-byte aligned");

		return SPUResult_t::Error;
	}

	if ((reinterpret_cast<uintptr_t>(pDstVertices) % 16) != 0)
	{
		AssertMsg(
			0,
			"CSPUBatchTransformManager: Destination vertices are not 16-byte aligned");

		return SPUResult_t::Error;
	}

	uint32 batchIndex = 0;

	for (uint32 i = 0; i < s_BatchTransformSPUCount; i++)
	{
		if (!m_pBatchJobs[i])
		{
			continue;
		}

		uint32 chunkBatches =
			(batchCount + (s_BatchTransformSPUCount - 1 - i)) / s_BatchTransformSPUCount;
		if (chunkBatches == 0)
		{
			continue;
		}

		uint64 offset = uint64(batchIndex) * vertexCount * vertexStride;
		
		SPUBatchTransformJob_t& spuBatchTransformJob = *m_pBatchJobs[i];
		spuBatchTransformJob.m_SrcVerticesEffAddr = CSPUUtils::PtrToEA(
			pSrcVertices);
		spuBatchTransformJob.m_SrcIndicesEffAddr = CSPUUtils::PtrToEA(
			pSrcIndices);
		spuBatchTransformJob.m_TransformsEffAddr = CSPUUtils::PtrToEA(
			pTransforms + batchIndex);
		spuBatchTransformJob.m_DstVerticesEffAddr = CSPUUtils::PtrToEA(
			pDstVertices + offset);
		spuBatchTransformJob.m_DstIndicesEffAddr = CSPUUtils::PtrToEA(
			pDstIndices + (batchIndex * indexCount));
		spuBatchTransformJob.m_Command = SPU_BATCH_CMD_TRANSFORM;
		spuBatchTransformJob.m_Status = SPU_BATCH_STATUS_IDLE;
		spuBatchTransformJob.m_VertexCount = vertexCount;
		spuBatchTransformJob.m_IndexCount = indexCount;
		spuBatchTransformJob.m_BatchCount = chunkBatches;
		spuBatchTransformJob.m_VertexStride = vertexStride;
		spuBatchTransformJob.m_TransformStride = sizeof(BatchChunkTransform_t);
		spuBatchTransformJob.m_VertexPositionOffset = vertexPosOffset;
		spuBatchTransformJob.m_BaseVertex = baseVertex + (batchIndex * vertexCount);

		batchIndex += chunkBatches;
	}

	__sync_synchronize();

	bool hasError = false;

	for (uint32 i = 0; i < s_BatchTransformSPUCount; i++)
	{
		if (m_pBatchJobs[i] && m_pBatchJobs[i]->m_Command == SPU_BATCH_CMD_TRANSFORM)
		{
			const int32 spuThreadWriteSignalResult = sysSpuThreadWriteSignal(
				m_SPUThreadIDs[i],
				0,
				1);
			if (spuThreadWriteSignalResult < 0)
			{
				m_pBatchJobs[i]->m_Command = SPU_BATCH_CMD_IDLE;
				m_pBatchJobs[i]->m_Status = SPU_BATCH_STATUS_ERROR;

				hasError = true;
			}
		}
	}

	return hasError ? SPUResult_t::Error : SPUResult_t::Success;
}

SPUResult_t::Enum CSPUBatchTransformManager::WaitBatch()
{
	bool hasError = false;

	for (uint32 i = 0; i < s_BatchTransformSPUCount; i++)
	{
		if (!m_pBatchJobs[i] || m_pBatchJobs[i]->m_Command == SPU_BATCH_CMD_IDLE)
		{
			continue;
		}

		sys_event_t event;
		if (sysEventQueueReceive(m_EventQueues[i], &event, 0) < 0)
		{
			hasError = true;
		}
		else if (static_cast<uint32>(event.data_2) == SPU_BATCH_STATUS_ERROR)
		{
			hasError = true;
		}

		m_pBatchJobs[i]->m_Command = SPU_BATCH_CMD_IDLE;
		m_pBatchJobs[i]->m_Status = SPU_BATCH_STATUS_IDLE;
	}

	return hasError ? SPUResult_t::Error : SPUResult_t::Success;
}