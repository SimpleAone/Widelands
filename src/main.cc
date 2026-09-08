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

#include <iostream>
#include <cstdio>
#include <cstring>
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
#ifdef __amigaos4__
/* Give the unwinder its tables. Without this, no throw in this program can
 * ever return, and that is not a figure of speech.
 *
 * _Unwind_Find_FDE in this toolchain is the registry version: it locks a mutex
 * and searches seen_objects/unseen_objects. Normally crtbegin's frame_dummy
 * fills that list at startup by calling __register_frame_info. This crt has no
 * frame_dummy and no __EH_FRAME_BEGIN__ -- nm finds neither, and .ctors holds
 * nothing but its terminators -- so the list stays empty for the life of the
 * process and not one FDE is ever found. Every throw died on the spot, with no
 * message, and it looked like a hang only because the VirtGL worker keeps
 * running after the main thread is gone. Both "load hangs" were this.
 *
 * The tables themselves were always fine: .eh_frame_hdr decodes to version 1,
 * pcrel|sdata4, and an eh_frame_ptr landing exactly on .eh_frame. So decode it
 * and hand it over.
 */
extern "C" void __register_frame_info(const void* begin, void* ob);

/* Placed at the start of .eh_frame_hdr by the linker script. Nothing in libgcc
   references it, which is the whole problem. */
extern "C" const unsigned char __GNU_EH_FRAME_HDR[];

/* struct object is private to libgcc -- seven pointers and a word. Oversized on
   purpose, and static because the unwinder keeps the pointer for good. */
static long long amiga_eh_object[16];

/* The terminator crtend normally supplies, and the other half of the bug.
 *
 * libgcc walks the FDE chain until it meets a zero length word, and this
 * .eh_frame ends in the middle of its last FDE. Registering without this made
 * the walk run off the end, read whatever followed as a CIE and die in strlen
 * on the augmentation string. On AmigaOS4 that is certain rather than likely:
 * ElfLoadSeg gives every section its own allocation, so past the end is not
 * zero padding, it is nothing at all.
 *
 * The linker script lays it out as
 *     .eh_frame : ONLY_IF_RO { KEEP (*(.eh_frame)) *(.eh_frame.*) }
 * so a .eh_frame.<suffix> section lands after every ordinary contribution
 * whatever the link order -- which is where these four bytes have to be. */
__attribute__((used, section(".eh_frame.zzz_terminator"), aligned(4)))
static const unsigned char amiga_eh_frame_terminator[4] = {0, 0, 0, 0};

/* Also a constructor, so a throw during static initialisation is covered too;
   main() calls it as well and the second call does nothing. */
__attribute__((constructor(101))) static void amiga_register_eh_frame() {
	static bool done = false;
	if (done) {
		return;
	}
	const unsigned char* hdr = __GNU_EH_FRAME_HDR;
	if (hdr[0] != 1) {
		return;  // unknown header version; leave things as they were
	}
	const unsigned char* p = hdr + 4;
	const void* frame = nullptr;
	if (hdr[1] == 0x1b) {  // DW_EH_PE_pcrel | DW_EH_PE_sdata4
		int offset;
		std::memcpy(&offset, p, sizeof offset);
		frame = p + offset;
	} else if (hdr[1] == 0x03) {  // DW_EH_PE_udata4, absolute
		unsigned address;
		std::memcpy(&address, p, sizeof address);
		frame = reinterpret_cast<const void*>(static_cast<unsigned long>(address));
	} else {
		return;  // unexpected encoding; do not hand libgcc a wrong pointer
	}
	__register_frame_info(frame, amiga_eh_object);
	done = true;
}
#endif

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
	amiga_register_eh_frame();
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
