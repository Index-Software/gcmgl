//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef UTILS_H
#define UTILS_H

#pragma once

#include "platform.h"
#include "utils/FixedString.h"
#include "utils/UtlVector.h"

class CUtils
{
public:
	// Relative paths resolve against this root, empty by default
	static void SetContentRoot(const CFixedString& root);
	static CFixedString GetFullPath(const CFixedString& path);

	static CFixedString ReadFile(const CFixedString& path);
	static bool WriteFile(
		const CFixedString& path,
		const CFixedString& contents);
	static bool CreateDirectory(const CFixedString& path);
	static CUtlVector<uint8> ReadBinaryFile(const CFixedString& path);
	static bool FileExists(const CFixedString& path);
private:
	static CFixedString s_ContentRoot;
};

#endif // UTILS_H