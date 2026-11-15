//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "Utils.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

CFixedString CUtils::s_ContentRoot;

void CUtils::SetContentRoot(const CFixedString& root)
{
	s_ContentRoot = root;
}

CFixedString CUtils::GetFullPath(const CFixedString& path)
{
	if (s_ContentRoot.IsEmpty() || path.find("/") == 0) return path;

	return s_ContentRoot + path;
}

// Reads a text file into a string
CFixedString CUtils::ReadFile(const CFixedString& path)
{
	CUtlVector<uint8> bytes = ReadBinaryFile(path);
	if (bytes.Count() > 0)
	{
		bytes.AddToTail('\0');
	}

	CFixedString result;
	if (bytes.Count() > 0)
	{
		result = reinterpret_cast<const char*>(bytes.Base());
	}

	return result;
}

bool CUtils::WriteFile(const CFixedString& path, const CFixedString& contents)
{
	const CFixedString fullPath = GetFullPath(path);

	char directory[512];
	size_t length = strlen(fullPath.c_str());
	if (length >= sizeof(directory))
	{
		return false;
	}

	memcpy(directory, fullPath.c_str(), length + 1);
	char* pLastSlash = strrchr(directory, '/');
	if (pLastSlash && pLastSlash != directory)
	{
		*pLastSlash = '\0';

		if (!CreateDirectory(CFixedString(directory)))
		{
			return false;
		}
	}

	FILE* pFile = fopen(fullPath.c_str(), "wb");
	if (!pFile)
	{
		return false;
	}

	fwrite(contents.c_str(), 1, size_t(contents.Length()), pFile);
	fclose(pFile);

	return true;
}

bool CUtils::CreateDirectory(const CFixedString& path)
{
	const CFixedString fullPath = GetFullPath(path);

	char directory[512];
	size_t length = strlen(fullPath.c_str());
	if (length == 0 || length >= sizeof(directory))
	{
		return false;
	}

	memcpy(directory, fullPath.c_str(), length + 1);

	for (char* pSlash = directory + 1; *pSlash; pSlash++)
	{
		if (*pSlash == '/')
		{
			*pSlash = '\0';
			mkdir(directory, 0755);
			*pSlash = '/';
		}
	}

	mkdir(directory, 0755);

	struct stat info;

	return stat(directory, &info) == 0 && S_ISDIR(info.st_mode);
}

CUtlVector<uint8> CUtils::ReadBinaryFile(const CFixedString& path)
{
	const CFixedString fullPath = GetFullPath(path);

	FILE* pFile = fopen(fullPath.c_str(), "rb");
	if (!pFile)
	{
		return CUtlVector<uint8>();
	}

	fseek(pFile, 0, SEEK_END);
	long size = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);

	if (size <= 0)
	{
		fclose(pFile);

		return CUtlVector<uint8>();
	}

	CUtlVector<uint8> buffer;
	buffer.SetCount(int32(size));

	fread(buffer.Base(), 1, size_t(size), pFile);
	fclose(pFile);

	return buffer;
}

bool CUtils::FileExists(const CFixedString& path)
{
	const CFixedString fullPath = GetFullPath(path);

	FILE* pFile = fopen(fullPath.c_str(), "rb");
	if (!pFile)
	{
		return false;
	}

	fclose(pFile);

	return true;
}