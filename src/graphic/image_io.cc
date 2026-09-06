/*
 * Copyright (C) 2006-2026 by the Widelands Development Team
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

#include "graphic/image_io.h"

#include <algorithm>

#include <cstddef>
#include <memory>

#include <SDL_image.h>
#include <png.h>

#include "base/log.h"
#include "base/wexception.h"
#include "graphic/texture.h"
#include "io/fileread.h"
#include "io/filesystem/layered_filesystem.h"
#include "io/streamwrite.h"

namespace {

// A helper function for save_to_png. Writes the compressed data to
// the StreamWrite.
void png_write_function(png_structp png_ptr, png_bytep png_data, png_size_t length) {
	static_cast<StreamWrite*>(png_get_io_ptr(png_ptr))->data(png_data, length);
}

// A helper function for save_to_png.
// Flush function to avoid crashes with default libpng flush function
void png_flush_function(png_structp png_ptr) {
	static_cast<StreamWrite*>(png_get_io_ptr(png_ptr))->flush();
}

inline void ensure_sdl_image_is_initialized() {
	static bool is_initialized = false;
	if (!is_initialized) {
		IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);
		is_initialized = true;
	}
}

}  // namespace

#ifdef __amigaos4__
int g_max_image_dimension = 0;

/* Background art only.
 *
 * The main menu rotates seventeen backgrounds of 1920x1080 and a splash of
 * 2592x1944 -- 7MB and 19MB apiece as RGBA -- for a window they are scaled
 * into anyway. That was tens of megabytes and 41 seconds of one run.
 *
 * It must stay to backgrounds. The other images this size are animation
 * spritesheets (tribes/ships/amazons/sail_nw_4_pc.png is 3690x2035), where
 * frames are cut at fixed pixel positions: scaling one silently breaks every
 * frame in it. That is why the cap lives here, where the path is known, and
 * not in the Texture constructor, which sees only pixels. */
static bool is_background_image(const std::string& fname) {
	return fname.find("loadscreens") != std::string::npos;
}

static SDL_Surface* downscale_to_fit(SDL_Surface* surface) {
	if (surface == nullptr || g_max_image_dimension <= 0 ||
	    (surface->w <= g_max_image_dimension && surface->h <= g_max_image_dimension)) {
		return surface;
	}
	const double scale = std::min(static_cast<double>(g_max_image_dimension) / surface->w,
	                              static_cast<double>(g_max_image_dimension) / surface->h);
	const int scaled_w = std::max(1, static_cast<int>(surface->w * scale));
	const int scaled_h = std::max(1, static_cast<int>(surface->h * scale));
	SDL_Surface* smaller =
	   SDL_CreateRGBSurfaceWithFormat(0, scaled_w, scaled_h, 32, SDL_PIXELFORMAT_ABGR8888);
	if (smaller == nullptr) {
		return surface;
	}
	SDL_SetSurfaceAlphaMod(surface, SDL_ALPHA_OPAQUE);
	SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);
	SDL_SetSurfaceAlphaMod(smaller, SDL_ALPHA_OPAQUE);
	SDL_SetSurfaceBlendMode(smaller, SDL_BLENDMODE_NONE);
	if (SDL_BlitScaled(surface, nullptr, smaller, nullptr) != 0) {
		SDL_FreeSurface(smaller);
		return surface;
	}
	log_info("AMIGA IMAGE: scaled %dx%d down to %dx%d", surface->w, surface->h, scaled_w, scaled_h);
	SDL_FreeSurface(surface);
	return smaller;
}
#endif

std::unique_ptr<Texture> load_image(const std::string& fname, FileSystem* fs) {
	SDL_Surface* surface = load_image_as_sdl_surface(fname, fs);
	#ifdef __amigaos4__
	if (is_background_image(fname)) {
		surface = downscale_to_fit(surface);
	}
	#endif
	return std::unique_ptr<Texture>(new Texture(surface));
}

SDL_Surface* load_image_as_sdl_surface(const std::string& fname, FileSystem* fs) {
	#ifdef WL_AMIGAOS4_VIRTIO_GL
	log_info("AMIGA IMAGE CHECKPOINT: before IMG_Init for %s", fname.c_str());
	#endif
	ensure_sdl_image_is_initialized();
	#ifdef WL_AMIGAOS4_VIRTIO_GL
	log_info("AMIGA IMAGE CHECKPOINT: after IMG_Init for %s", fname.c_str());
	#endif

	FileRead fr;
	bool found;
	if (fs != nullptr) {
		found = fr.try_open(*fs, fname);
	} else {
		found = fr.try_open(*g_fs, fname);
	}

	if (!found) {
		throw ImageNotFound(fname);
	}
	#ifdef WL_AMIGAOS4_VIRTIO_GL
	log_info("AMIGA IMAGE CHECKPOINT: file loaded for %s (%u bytes)", fname.c_str(),
	         static_cast<unsigned>(fr.get_size()));
	log_info("AMIGA IMAGE CHECKPOINT: before IMG_Load_RW for %s", fname.c_str());
	#endif

	SDL_Surface* sdlsurf = IMG_Load_RW(SDL_RWFromMem(fr.data(0), fr.get_size()), 1);
	if (sdlsurf == nullptr) {
		throw ImageLoadingError(fname, IMG_GetError());
	}
	#ifdef WL_AMIGAOS4_VIRTIO_GL
	log_info("AMIGA IMAGE CHECKPOINT: after IMG_Load_RW for %s", fname.c_str());
	#endif
	return sdlsurf;
}

bool save_to_png(Texture* texture, StreamWrite* sw, ColorType color_type) {
	png_structp png_ptr = png_create_write_struct(
	   PNG_LIBPNG_VER_STRING, static_cast<png_voidp>(nullptr), nullptr, nullptr);

	if (png_ptr == nullptr) {
		throw wexception("save_to_png: could not create png struct");
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == nullptr) {
		png_destroy_write_struct(&png_ptr, static_cast<png_infopp>(nullptr));
		throw wexception("save_to_png: could not create png info struct");
	}

	// Set jump for error
	if (setjmp(png_jmpbuf(png_ptr))) {  // NOLINT
		png_destroy_write_struct(&png_ptr, &info_ptr);
		throw wexception("save_to_png: Error writing PNG!");
	}

	//  Set another write function. This is potentially dangerouse because the
	//  flush function is internally called by png_write_end(), this will crash
	//  on newer libpngs. See here:
	//     https://bugs.freedesktop.org/show_bug.cgi?id=17212
	//
	//  Simple solution is to define a dummy flush function which I did here.
	png_set_write_fn(png_ptr, sw, &png_write_function, &png_flush_function);

	// Fill info struct
	png_set_IHDR(png_ptr, info_ptr, texture->width(), texture->height(), 8,
	             (color_type == ColorType::RGB) ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGBA,
	             PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	// Start writing
	png_write_info(png_ptr, info_ptr);
	{
		const uint16_t surf_w = texture->width();
		const uint16_t surf_h = texture->height();
		const uint32_t row_size = (color_type == ColorType::RGB) ? 3 * surf_w : 4 * surf_w;

		std::unique_ptr<png_byte[]> row(new png_byte[row_size]);

		// Write each row
		texture->lock();

		// Write each row
		RGBAColor color;
		if (color_type == ColorType::RGB) {
			for (uint32_t y = 0; y < surf_h; ++y) {
				for (uint32_t x = 0; x < surf_w; ++x) {
					color = texture->get_pixel(x, y);
					row[static_cast<std::size_t>(3) * x] = color.r;
					row[3 * x + 1] = color.g;
					row[3 * x + 2] = color.b;
				}
				png_write_row(png_ptr, row.get());
			}
		} else {
			for (uint32_t y = 0; y < surf_h; ++y) {
				for (uint32_t x = 0; x < surf_w; ++x) {
					color = texture->get_pixel(x, y);
					row[static_cast<std::size_t>(4) * x] = color.r;
					row[4 * x + 1] = color.g;
					row[4 * x + 2] = color.b;
					row[4 * x + 3] = color.a;
				}
				png_write_row(png_ptr, row.get());
			}
		}
		texture->unlock(Texture::Unlock_NoChange);
	}
	// End write
	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	return true;
}
