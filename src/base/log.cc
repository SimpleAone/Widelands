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

#include "base/log.h"

#include <cassert>
#include <cstdarg>
#include <cstdio>
#ifdef _WIN32
#include <fstream>
#endif
#include <iostream>
#include <memory>
#include <vector>

#include <SDL_log.h>
#include <SDL_timer.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include "base/multithreading.h"
#include "base/string.h"
#ifdef _WIN32
#include "build_info.h"
#endif

#ifdef __amigaos4__
/* A second copy of the log on the share.
 *
 * stdout goes to PROGDIR: so a run from a hard disk keeps its log, and that
 * is also the fast one -- every write to a 9P share is a round trip to the
 * host. But the share is the only place reachable from outside the emulator,
 * so the same lines are mirrored there.
 *
 * Best effort: if the share is not mounted the mirror simply never opens and
 * nothing else changes. */
static FILE* g_log_mirror = nullptr;
static bool g_log_mirror_tried = false;

/* Buffered, and deliberately not flushed per line.
 *
 * It used to fflush after every line, which bought exactly nothing: the note
 * on log_mirror_reopen() below is the reason -- the 9P handler holds writes
 * until the file is closed, so a flush pushes the line out of stdio and into
 * a handler that sits on it anyway. Nothing became visible any sooner, and
 * every line paid a write across the host boundary for it. The load path is
 * thousands of lines.
 *
 * What is lost by buffering is nothing that was not already lost: on a crash
 * the handler discards its own buffer too. The copy that survives a crash is
 * stdout on PROGDIR:, which is still flushed per line and is on a local
 * disk. */
static char g_log_mirror_buffer[64 * 1024];

static void log_mirror_open(const char* mode) {
	g_log_mirror = std::fopen("SHARED:widelands/widelands.out", mode);
	if (g_log_mirror != nullptr) {
		std::setvbuf(g_log_mirror, g_log_mirror_buffer, _IOFBF,
		             sizeof g_log_mirror_buffer);
	}
}

void log_mirror_write(const char* text) {
	if (!g_log_mirror_tried) {
		g_log_mirror_tried = true;
		log_mirror_open("w");
	}
	if (g_log_mirror != nullptr) {
		std::fputs(text, g_log_mirror);
	}
}

/* The 9P handler holds writes until the file is closed, so a flush is not
   enough to make a line visible from the host while the game is still
   running. Closing and reopening is. Called from log_progress(), which is
   for the slow silent phases only -- never per frame. */
void log_mirror_reopen() {
	if (g_log_mirror != nullptr) {
		std::fclose(g_log_mirror);   /* flushes the buffer on the way out */
		log_mirror_open("a");
	}
}
#endif

namespace {

// Forward declaration to work around cyclic dependency.
void sdl_logging_func(void* userdata,
                      int /*unused*/,
                      SDL_LogPriority /*unused*/,
                      const char* message);

#ifdef _WIN32
std::string get_output_directory() {
// This took inspiration from SDL 1.2 logger code.
#ifdef _WIN32_WCE
	wchar_t path[MAX_PATH];
#else
	char path[MAX_PATH];
#endif
	auto pathlen = GetModuleFileName(nullptr, path, MAX_PATH);
	while (pathlen > 0 && path[pathlen] != '\\') {
		--pathlen;
	}
	path[pathlen] = '\0';
	return path;
}

// This Logger emulates the SDL1.2 behavior of writing a stdout.txt and stderr.txt.
class WindowsLogger {
public:
	WindowsLogger(const std::string& dir)
	   : stdout_filename_(dir + "\\stdout.txt"), stderr_filename_(dir + "\\stderr.txt") {
		stdout_.open(stdout_filename_);
		stderr_.open(stderr_filename_);
		if (!stdout_.good() || !stderr_.good()) {
			throw wexception(
			   "Unable to initialize stdout logging destination: %s", stdout_filename_.c_str());
		}
		SDL_LogSetOutputFunction(sdl_logging_func, this);
		std::cout << "Log output will be written to: " << stdout_filename_ << std::endl;

		// Repeat version info so that we'll have it available in the log file too
		stdout_ << "This is Widelands version " << build_ver_details() << std::endl;
		stdout_.flush();
	}

	void log_cstring(const char* buffer) {
		stdout_ << buffer;
		stdout_.flush();
	}

private:
	const std::string stdout_filename_, stderr_filename_;
	std::ofstream stdout_, stderr_;

	DISALLOW_COPY_AND_ASSIGN(WindowsLogger);
};

void sdl_logging_func(void* userdata,
                      int /* category */,
                      SDL_LogPriority /* priority */,
                      const char* message) {
	static_cast<WindowsLogger*>(userdata)->log_cstring(message);
}
#else  // _WIN32


class Logger {
public:
	Logger() {
		SDL_LogSetOutputFunction(sdl_logging_func, this);
	}

	void log_cstring(const char* buffer) {
		std::cout << buffer;
		#ifndef __amigaos4__
		std::cout.flush();
		#else
		/* Not flushed per line here.
		 *
		 * A run started from the share has PROGDIR: on the share too, so
		 * stdout is a 9P file and the flush was a round trip to the host --
		 * twice per log line, since do_log sends the prefix and the text
		 * through separately. Warnings and errors still force it out (below),
		 * log_progress() still pushes stdout across at every slow phase, and
		 * the stream is flushed at exit. What buffering can cost is the tail
		 * of an ordinary info log after a hard crash. */
		log_mirror_write(buffer);
		#endif
	}

	void flush() {
		std::cout.flush();
		#ifdef __amigaos4__
		std::fflush(stdout);
		#endif
	}

private:
	DISALLOW_COPY_AND_ASSIGN(Logger);
};

void sdl_logging_func(void* userdata,
                      int /* category */,
                      SDL_LogPriority /* priority */,
                      const char* message) {
	static_cast<Logger*>(userdata)->log_cstring(message);
}
#endif

}  // namespace

// Default to stdout for logging.
bool g_verbose = false;

#ifdef _WIN32
// Start with nullptr so that we won't initialize an empty file in the program's directory
std::unique_ptr<WindowsLogger> logger(nullptr);

// Set the logging dir to the given homedir
bool set_logging_dir(const std::string& homedir) {
	try {
		logger.reset(new WindowsLogger(homedir));
	} catch (const std::exception& e) {
		std::cout << e.what() << std::endl;
		return false;
	}
	return true;
}

// Set the logging dir to the program's dir. For running test cases where we don't have a homedir.
void set_testcase_logging_dir() {
	logger.reset(new WindowsLogger(get_output_directory()));
}

#else
std::unique_ptr<Logger> logger(new Logger());
#endif

static const char* to_string(const LogType& type) {
	switch (type) {
	case LogType::kInfo:
		return "INFO";
	case LogType::kDebug:
		return "DEBUG";
	case LogType::kLua:
		return "LUA";
	case LogType::kWarning:
		return "WARNING";
	case LogType::kError:
		return "ERROR";
	default:
		NEVER_HERE();
	}
}

void do_log(const LogType type, const Time& gametime, const char* const fmt, ...) {
	MutexLock m(MutexLock::ID::kLog);
	assert(logger != nullptr);

	// message type and timestamp
	char buffer_prefix[256];
	{
		uint32_t t = gametime.is_valid() ? gametime.get() : SDL_GetTicks();
		const uint32_t hours = t / (1000 * 60 * 60);
		t -= hours * 1000 * 60 * 60;
		const uint32_t minutes = t / (1000 * 60);
		t -= minutes * 1000 * 60;
		const uint32_t seconds = t / 1000;
		t -= seconds * 1000;
		snprintf(buffer_prefix, sizeof(buffer_prefix), "[%02u:%02u:%02u.%03u %s] %s: ", hours,
		         minutes, seconds, t, gametime.is_invalid() ? "real" : "game", to_string(type));
	}

	// actual log output
	char buffer[2048];
	va_list va;
	va_start(va, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, va);
	va_end(va);

	std::vector<std::string> vec;
	split(vec, buffer, {'\n'});
	for (std::string& str : vec) {
		if (str.find_first_not_of(' ') == std::string::npos) {
			continue;
		}
		logger->log_cstring(buffer_prefix);
		str.push_back('\n');
		logger->log_cstring(str.c_str());
	}
#ifdef __amigaos4__
	/* Anything that reports a problem goes out now, so a log that stops does
	   so after the line explaining why rather than before it. Ordinary info
	   lines ride the buffer -- see log_cstring. */
	if (type == LogType::kWarning || type == LogType::kError) {
		logger->flush();
	}
#endif
}
