// Win Service
// Copyright (c) 2023-2025 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023-2025 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_WINSERVICE_DEPENDENCY_HPP
#define XYO_WINSERVICE_DEPENDENCY_HPP

#ifndef XYO_WIN_HPP
#	include <XYO/Win.hpp>
#endif

// -- Export

#ifndef XYO_WINSERVICE_INTERNAL
#	ifdef XYO_WIN_SERVICE_INTERNAL
#		define XYO_WINSERVICE_INTERNAL
#	endif
#endif

#ifdef XYO_WINSERVICE_INTERNAL
#	define XYO_WINSERVICE_EXPORT XYO_PLATFORM_LIBRARY_EXPORT
#else
#	define XYO_WINSERVICE_EXPORT XYO_PLATFORM_LIBRARY_IMPORT
#endif
#ifdef XYO_WINSERVICE_LIBRARY
#	undef XYO_WINSERVICE_EXPORT
#	define XYO_WINSERVICE_EXPORT
#endif

namespace XYO::WinService {
	using namespace XYO::System;
	using namespace XYO::Win;
};

#endif
