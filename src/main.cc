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

#include <chrono>
#include <iostream>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <sstream>
#include <typeinfo>

#ifdef _MSC_VER
// Needed to resolve entry point
#include <SDL.h>
#else
#include <unistd.h>
#endif
#if defined(PRINT_SEGFAULT_BACKTRACE) && !defined(__amigaos4__)
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <execinfo.h>
#endif

#include "base/multithreading.h"
#include "base/time_string.h"
#include "base/wexception.h"
#include "build_info.h"
#include "config.h"
#include "logic/filesystem_constants.h"
#include "wlapplication.h"
#include "wlapplication_messages.h"

#ifdef __amigaos4__
#include <SDL_image.h>

// The SDK's static libwebp was built against pthreads.library. Widelands does
// not use WebP assets, so satisfy SDL2_image's optional loader hooks here. This
// keeps IMG_webp.o (and therefore libwebp/libpthread) out of the final binary.
extern "C" int IMG_InitWEBP(void) {
	return -1;
}
extern "C" void IMG_QuitWEBP(void) {
}
extern "C" int IMG_isWEBP(SDL_RWops*) {
	return 0;
}
extern "C" SDL_Surface* IMG_LoadWEBP_RW(SDL_RWops*) {
	return nullptr;
}
extern "C" IMG_Animation* IMG_LoadWEBPAnimation_RW(SDL_RWops*) {
	return nullptr;
}
extern "C" int IMG_InitTIF(void) {
	return -1;
}
extern "C" void IMG_QuitTIF(void) {
}
extern "C" int IMG_isTIF(SDL_RWops*) {
	return 0;
}
extern "C" SDL_Surface* IMG_LoadTIF_RW(SDL_RWops*) {
	return nullptr;
}
#endif

#ifdef WL_AMIGAOS4_VIRTIO_GL
// Standard AmigaOS version tag. The leading NUL prevents accidental display
// as ordinary text; `Version Widelands FULL` scans the executable for `$VER:`.
__attribute__((used)) static const char amigaos4_verstag[] =
   "\0$VER: Widelands " WL_AMIGAOS4_PORT_VERSION " (" WL_AMIGAOS4_VER_DATE ")";
#endif

#if defined(PRINT_SEGFAULT_BACKTRACE) && !defined(__amigaos4__)
// Taken from https://stackoverflow.com/a/77336
// TODO(Nordfriese): Implement this on Windows as well (see https://stackoverflow.com/a/26398082)
static void segfault_handler(const int sig) {
	constexpr int kMaxBacktraceSize = 256;
	void* array[kMaxBacktraceSize];
	size_t size = backtrace(array, kMaxBacktraceSize);

	std::cout << std::endl
	          << "##############################" << std::endl
	          << "FATAL ERROR: Received signal " << sig << " (" << strsignal(sig) << ")" << std::endl
	          << "Backtrace:" << std::endl;
	backtrace_symbols_fd(array, size, STDOUT_FILENO);
	std::cout
	   << std::endl
	   << "Please report this problem to help us improve Widelands, and provide the complete output."
	   << std::endl
	   << "##############################" << std::endl;

	std::string filename;
	if (WLApplication::segfault_backtrace_dir.empty()) {
		filename = "./widelands_crash_report_";
	} else {
		filename = WLApplication::segfault_backtrace_dir;
		filename += "/";
	}

	const std::string timestr = timestring();
	filename += timestr;

	std::string thread_name;
	if (is_initializer_thread()) {
		filename += "_ui";
		thread_name = "UI thread";
	} else if (is_logic_thread()) {
		filename += "_logic";
		thread_name = "logic thread";
	} else {
		std::ostringstream thread_id;
		thread_id << std::this_thread::get_id();
		filename += "_";
		filename += thread_id.str();
		thread_name = "thread " + thread_id.str();
	}

	filename += kCrashExtension;

	FILE* file = fopen(filename.c_str(), "w+");
	if (file == nullptr) {
		std::cout << "The crash report could not be saved to a file." << std::endl << std::endl;
	} else {
		fprintf /* NOLINT codecheck */ (
		   file,
		   "Crash report for Widelands %s %s at %s, signal %d (%s)\n\n**** BEGIN BACKTRACE ****\n",
		   build_ver_details().c_str(), thread_name.c_str(), timestr.c_str(), sig, strsignal(sig));
		fflush(file);
		backtrace_symbols_fd(array, size, fileno(file));
		fflush(file);
		fputs("**** END BACKTRACE ****\n", file);

		fclose(file);
		std::cout << "The crash report was also saved to " << filename << std::endl << std::endl;
	}

	::exit(sig);
}
#endif

#ifdef __amigaos4__
/* The stack this program's main thread gets.
 *
 * Without it Widelands runs on whatever the shell hands out, and that is not
 * enough for two things happening on the same thread. Its own call stacks are
 * deep -- nested UI panels, the richtext layouter -- and underneath them
 * virtio_gpu.library builds frames of its own: probe_control_queue0() alone
 * claims 35KB in a single stwux, and virtioBackendCreateFrameTarget() has
 * been seen at 195KB. A session rebuild happens inside present(), so those
 * land on top of the deepest part of the UI stack rather than beside it.
 *
 * The result is a DSI on a guard page, which reads as a wild pointer in the
 * driver and is nothing of the kind. newlib reads this symbol at startup. */
extern "C" {
unsigned long __stack_size = 2UL * 1024UL * 1024UL;
}
/* And the same size again as a stack cookie, because __stack_size alone did
   not take: the crash came back unchanged, with only 37KB below the stack
   pointer. A shell reads this string out of the binary and starts the
   process with it; __stack_size is what newlib applies, and which of the
   two is consulted depends on how the program was launched. */
static const char* const kStackCookie __attribute__((used)) = "$STACK:2097152";
#endif

/**
 * Cross-platform entry point for SDL applications.
 */
int main(int argc, char* argv[]) {
#ifdef WL_AMIGAOS4_VIRTIO_GL
	/* Beside the program, not on the share. SHARED: is not mounted when
	   Widelands is run from a hard disk, and then the whole log went nowhere
	   -- including every progress line that reopens this path. */
	if (std::freopen("PROGDIR:widelands.out", "w", stdout) != nullptr) {
		std::setvbuf(stdout, nullptr, _IOLBF, 0);
	}
	if (std::freopen("PROGDIR:widelands.out", "a", stderr) != nullptr) {
		std::setvbuf(stderr, nullptr, _IOLBF, 0);
	}
	std::cout << "AmigaOS4 port build " << WL_AMIGAOS4_PORT_VERSION << " ("
	          << WL_AMIGAOS4_VER_DATE << ')' << std::endl;
#endif
	std::cout << "This is Widelands version " << build_ver_details() << std::endl;

#ifdef __amigaos4__
	/* Which of the two does a failed load hang on?
	 *
	 * Two load hangs have now landed on the same shape: a file that is not
	 * there, asked for through RealFSImpl::load, which for a miss does exactly
	 * two things that the healthy fs.file_exists() path does not -- fopen() a
	 * name that does not exist, and throw. Both hangs went away the moment the
	 * call was replaced by file_exists(), which answers the same question by
	 * stat() in 8ms. Guessing between the two has cost two build-and-load
	 * rounds, so measure them.
	 *
	 * Every line is forced onto disk before the next step runs. stdout here is
	 * a file on the 9P share, and that handler holds writes until the file is
	 * closed -- a flush alone leaves the line invisible from the host, which
	 * is why log_progress() closes and reopens. Without this, a probe that
	 * hangs looks exactly like a program that never started.
	 *
	 * fopen goes first so that its numbers survive even if the throw is the
	 * one that never returns, and each probe announces itself beforehand so
	 * silence still names the culprit. Three times each: the first throw is
	 * the one that builds the unwinder's lookup table. */
	{
		/* Its own file on the share, closed after every line.
		 *
		 * PROGDIR:widelands.out is no good here: PROGDIR: is wherever the game
		 * was started from, which is not necessarily the share, and the copy
		 * that is on the share is opened by the logger on its first line --
		 * which is after all of this. So a probe that hangs leaves the share
		 * holding the previous run's log, untouched, and that is exactly what
		 * it looked like. Closing per line is what makes a line visible
		 * through the 9P handler at all. */
		bool probe_first = true;
		auto probe_say = [&probe_first](const std::string& text) {
			std::cout << "AMIGA PROBE: " << text << std::endl;
			if (FILE* f = std::fopen("SHARED:widelands/probe.txt", probe_first ? "w" : "a");
			    f != nullptr) {
				std::fputs(text.c_str(), f);
				std::fputc('\n', f);
				std::fclose(f);
				probe_first = false;
			}
		};
		auto probe_ms = [](const std::chrono::steady_clock::time_point& from) {
			return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from)
			   .count();
		};

		for (int probe = 1; probe <= 3; ++probe) {
			probe_say("fopen " + std::to_string(probe) + ": opening a file that does not exist");
			const auto started = std::chrono::steady_clock::now();
			FILE* missing = std::fopen("PROGDIR:this-file-does-not-exist", "rb");
			const double took = probe_ms(started);
			if (missing != nullptr) {
				std::fclose(missing);
			}
			probe_say("fopen " + std::to_string(probe) + ": took " + std::to_string(took) + " ms");
		}

		for (int probe = 1; probe <= 3; ++probe) {
			probe_say("throw " + std::to_string(probe) + ": about to throw");
			const auto started = std::chrono::steady_clock::now();
			try {
				throw std::runtime_error("exception probe");
			} catch (const std::exception&) {
			}
			const double took = probe_ms(started);
			probe_say("throw " + std::to_string(probe) + ": took " + std::to_string(took) + " ms");
		}
	}
#endif

#if defined(PRINT_SEGFAULT_BACKTRACE) && !defined(__amigaos4__)
	/* Handle several types of fatal crashes with a useful backtrace on supporting systems.
	 * We can't handle SIGABRT like this since we have to redirect that one elsewhere to
	 * suppress non-critical errors from Eris.
	 */
	for (int s : {SIGSEGV, SIGBUS, SIGFPE, SIGILL}) {
		signal(s, segfault_handler);
	}
#endif

	try {
		WLApplication& g_app = WLApplication::get(argc, const_cast<char const**>(argv));
		// TODO(unknown): handle exceptions from the constructor
		g_app.run();

		return 0;
	} catch (const ParameterError& e) {
		//  handle wrong commandline parameters
		show_usage(build_ver_details(), e.level_);
		if (e.what()[0] != 0) {
			std::cerr << std::string(60, '=') << std::endl << std::endl << e.what() << std::endl;
		}

		return 0;
	}
#ifdef NDEBUG
	catch (const WException& e) {
		std::cerr << "\nCaught exception (of type '" << typeid(e).name()
		          << "') in outermost handler!\nThe exception said: " << e.what()
		          << "\n\nThis should not happen. Please file a bug report on version "
		          << build_ver_details() << ".\n"
		          << "and remember to specify your operating system.\n\n"
		          << std::flush;

		return 1;
	} catch (const std::exception& e) {
		std::cerr << "\nCaught exception (of type '" << typeid(e).name()
		          << "') in outermost handler!\nThe exception said: " << e.what()
		          << "\n\nThis should not happen. Please file a bug report on version "
		          << build_ver_details() << ".\n"
		          << "and remember to specify your operating system.\n\n"
		          << std::flush;

		return 1;
	}
#endif
}
