// Win Service
// Copyright (c) 2023-2024 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023-2024 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_WINSERVICE_COPYRIGHT_HPP
#define XYO_WINSERVICE_COPYRIGHT_HPP

#ifndef XYO_WINSERVICE_DEPENDENCY_HPP
#	include <XYO/WinService/Dependency.hpp>
#endif

namespace XYO::WinService::Copyright {
	XYO_WINSERVICE_EXPORT const char *copyright();
	XYO_WINSERVICE_EXPORT const char *publisher();
	XYO_WINSERVICE_EXPORT const char *company();
	XYO_WINSERVICE_EXPORT const char *contact();
};

#endif
