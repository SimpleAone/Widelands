/*
 * Timing a phase from the inside.
 *
 * The loading messages a player sees are coarse: one of them covers finding
 * a map loader, reading its header, running the add-on postload and creating
 * the players, and those want completely different fixes. Sub-timing them
 * needs nothing more than a stopwatch that reports the gap since the last
 * time it was asked, and reports it through log_progress -- the 9P share
 * holds writes until the file is closed, so an ordinary log line during a
 * long silent phase never arrives.
 *
 * Nothing at all outside AmigaOS, and cheap enough there to leave in: it
 * fires once per phase, never per frame or per item.
 */
#ifndef WL_BASE_AMIGA_PHASE_H
#define WL_BASE_AMIGA_PHASE_H

#ifdef __amigaos4__

#include <SDL_timer.h>

#include <string>

#include "base/log.h"

class AmigaPhase {
public:
	explicit AmigaPhase(const std::string& what) : last_(SDL_GetPerformanceCounter()) {
		log_progress("AMIGA PHASE: %s", what.c_str());
	}

	void mark(const std::string& what) {
		const Uint64 now = SDL_GetPerformanceCounter();
		log_progress("AMIGA PHASE:   %s took %.0fms", what.c_str(),
		             static_cast<double>(now - last_) * 1000.0 /
		                static_cast<double>(SDL_GetPerformanceFrequency()));
		last_ = now;
	}

private:
	Uint64 last_;
};

#else

class AmigaPhase {
public:
	explicit AmigaPhase(const char*) {
	}
	void mark(const char*) {
	}
};

#endif

#endif  // end of include guard: WL_BASE_AMIGA_PHASE_H
