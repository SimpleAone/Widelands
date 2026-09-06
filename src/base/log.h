/*
 * Copyright (C) 2002-2026 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef WL_BASE_LOG_H
#define WL_BASE_LOG_H

#include <cstdint>
#include <limits>
#include <cstdio>
#include <string>

#include "base/macros.h"
#include "base/times.h"

enum class LogType {
	kInfo,     // normal info messages
	kDebug,    // additional debug output
	kLua,      // output from Lua scripts
	kWarning,  // warnings
	kError     // fatal errors
};

extern bool g_verbose;

// Print a formatted log messages to stdout on most systems and 'stdout.txt' on windows.
// If `gametime` is not invalid, a timestamp for the gametime will be prepended to the
// output; otherwise, the real time will be used for the timestamp.
void do_log(LogType, const Time& gametime, const char*, ...) PRINTF_FORMAT(3, 4);

#define log_info_time(time, ...) do_log(LogType::kInfo, time, __VA_ARGS__)
#define log_dbg_time(time, ...) do_log(LogType::kDebug, time, __VA_ARGS__)
#define log_warn_time(time, ...) do_log(LogType::kWarning, time, __VA_ARGS__)
#define log_err_time(time, ...) do_log(LogType::kError, time, __VA_ARGS__)

/* Per-frame bring-up tracing for the AmigaOS4 port, off by default.
 *
 * These checkpoints found several real faults while the port had no picture
 * at all, but they sit in the text and texture paths, which run dozens of
 * times per frame. Left on they were 157080 log lines from the font handler
 * alone out of 201699, a 16MB file in a few minutes of main menu -- and the
 * log is line buffered onto a 9P share, so every one of them is a write
 * across the host boundary. Measured against it: the GPU pipeline costs
 * 4-9ms of a 100ms frame, so the rendering was never what was slow.
 *
 * Build with -DWL_AMIGAOS4_CHECKPOINTS to get them back. Startup
 * checkpoints still use log_info: they run once and cost nothing. */
#ifdef WL_AMIGAOS4_CHECKPOINTS
#define log_checkpoint(...) do_log(LogType::kInfo, Time(), __VA_ARGS__)
#else
#define log_checkpoint(...) ((void)0)
#endif

/* A progress line that actually reaches the host.
 *
 * stdout is a file on a 9P share: the handler holds writes until the file is
 * closed, so an ordinary log line during a long silent phase never arrives
 * and the log simply appears to stop. Every stall this port has had -- the
 * texture atlas, the map list, a throw during startup -- looked identical
 * from outside for that reason. Reopening the file is what pushes it across.
 *
 * For the slow phases that report nothing, never per frame: it costs an
 * open and a close each time. */
#ifdef __amigaos4__
void log_mirror_write(const char* text);
void log_mirror_reopen();
#define log_progress(...)                                                                          \
	do {                                                                                            \
		do_log(LogType::kInfo, Time(), __VA_ARGS__);                                                 \
		std::fflush(stdout);                                                                         \
		std::freopen("PROGDIR:widelands.out", "a", stdout);                                          \
		log_mirror_reopen();                                                                         \
	} while (false)
#else
#define log_progress(...) do_log(LogType::kInfo, Time(), __VA_ARGS__)
#endif

#define log_info(...) do_log(LogType::kInfo, Time(), __VA_ARGS__)
#define log_dbg(...) do_log(LogType::kDebug, Time(), __VA_ARGS__)
#define log_warn(...) do_log(LogType::kWarning, Time(), __VA_ARGS__)
#define log_err(...) do_log(LogType::kError, Time(), __VA_ARGS__)

#define verb_log_info_time(time, ...)                                                              \
	if (g_verbose)                                                                                  \
	do_log(LogType::kInfo, time, __VA_ARGS__)
#define verb_log_dbg_time(time, ...)                                                               \
	if (g_verbose)                                                                                  \
	do_log(LogType::kDebug, time, __VA_ARGS__)
#define verb_log_warn_time(time, ...)                                                              \
	if (g_verbose)                                                                                  \
	do_log(LogType::kWarning, time, __VA_ARGS__)
#define verb_log_err_time(time, ...)                                                               \
	if (g_verbose)                                                                                  \
	do_log(LogType::kError, time, __VA_ARGS__)

#define verb_log_info(...)                                                                         \
	if (g_verbose)                                                                                  \
	do_log(LogType::kInfo, Time(), __VA_ARGS__)
#define verb_log_dbg(...)                                                                          \
	if (g_verbose)                                                                                  \
	do_log(LogType::kDebug, Time(), __VA_ARGS__)
#define verb_log_warn(...)                                                                         \
	if (g_verbose)                                                                                  \
	do_log(LogType::kWarning, Time(), __VA_ARGS__)
#define verb_log_err(...)                                                                          \
	if (g_verbose)                                                                                  \
	do_log(LogType::kError, Time(), __VA_ARGS__)

#ifdef _WIN32
/** Set the directory that stdout.txt shall be written to.
 *  This should be the same dir where widelands writes its config file. Returns true on success.
 */
bool set_logging_dir(const std::string& homedir);
// Set the directory that stdout.txt shall be written to to the directory the program is started
// from. Use this only for test cases.
void set_testcase_logging_dir();

#else
inline void set_testcase_logging_dir() {
}
#endif
#endif  // end of include guard: WL_BASE_LOG_H
