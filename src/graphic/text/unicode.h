/*
 * Copyright (C) 2002-2026 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef WL_GRAPHIC_TEXT_UNICODE_H
#define WL_GRAPHIC_TEXT_UNICODE_H

#ifdef WL_USE_ICU

#include <unicode/uchar.h>
#include <unicode/unistr.h>

#else

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

using UChar = uint16_t;
using UChar32 = uint32_t;

// ICU block subset used by bidi.cc. Values are private to the embedded
// implementation; only equality and set membership matter.
enum class UBlockCode {
	UBLOCK_INVALID,
	UBLOCK_CJK_COMPATIBILITY, UBLOCK_CJK_COMPATIBILITY_FORMS,
	UBLOCK_CJK_COMPATIBILITY_IDEOGRAPHS, UBLOCK_CJK_COMPATIBILITY_IDEOGRAPHS_SUPPLEMENT,
	UBLOCK_CJK_RADICALS_SUPPLEMENT, UBLOCK_CJK_STROKES, UBLOCK_CJK_SYMBOLS_AND_PUNCTUATION,
	UBLOCK_CJK_UNIFIED_IDEOGRAPHS, UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_A,
	UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_B, UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_C,
	UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_D, UBLOCK_HIRAGANA, UBLOCK_KATAKANA,
	UBLOCK_KATAKANA_PHONETIC_EXTENSIONS, UBLOCK_ENCLOSED_CJK_LETTERS_AND_MONTHS,
	UBLOCK_HANGUL_COMPATIBILITY_JAMO, UBLOCK_HANGUL_JAMO, UBLOCK_HANGUL_JAMO_EXTENDED_A,
	UBLOCK_HANGUL_JAMO_EXTENDED_B, UBLOCK_HANGUL_SYLLABLES,
	UBLOCK_ARABIC, UBLOCK_ARABIC_SUPPLEMENT, UBLOCK_ARABIC_EXTENDED_A,
	UBLOCK_ARABIC_PRESENTATION_FORMS_A, UBLOCK_ARABIC_PRESENTATION_FORMS_B,
	UBLOCK_ARABIC_MATHEMATICAL_ALPHABETIC_SYMBOLS, UBLOCK_DEVANAGARI,
	UBLOCK_DEVANAGARI_EXTENDED, UBLOCK_VEDIC_EXTENSIONS, UBLOCK_HEBREW
};

inline UBlockCode ublock_getCode(UChar32 c) {
	if (c >= 0x0590 && c <= 0x05ff) return UBlockCode::UBLOCK_HEBREW;
	if (c >= 0x0600 && c <= 0x06ff) return UBlockCode::UBLOCK_ARABIC;
	if (c >= 0x0750 && c <= 0x077f) return UBlockCode::UBLOCK_ARABIC_SUPPLEMENT;
	if (c >= 0x08a0 && c <= 0x08ff) return UBlockCode::UBLOCK_ARABIC_EXTENDED_A;
	if (c >= 0x0900 && c <= 0x097f) return UBlockCode::UBLOCK_DEVANAGARI;
	if (c >= 0x1cd0 && c <= 0x1cff) return UBlockCode::UBLOCK_VEDIC_EXTENSIONS;
	if (c >= 0xa8e0 && c <= 0xa8ff) return UBlockCode::UBLOCK_DEVANAGARI_EXTENDED;
	if (c >= 0xfb50 && c <= 0xfdff) return UBlockCode::UBLOCK_ARABIC_PRESENTATION_FORMS_A;
	if (c >= 0xfe70 && c <= 0xfeff) return UBlockCode::UBLOCK_ARABIC_PRESENTATION_FORMS_B;
	if (c >= 0x1ee00 && c <= 0x1eeff) return UBlockCode::UBLOCK_ARABIC_MATHEMATICAL_ALPHABETIC_SYMBOLS;
	if (c >= 0x1100 && c <= 0x11ff) return UBlockCode::UBLOCK_HANGUL_JAMO;
	if (c >= 0x2e80 && c <= 0x2eff) return UBlockCode::UBLOCK_CJK_RADICALS_SUPPLEMENT;
	if (c >= 0x3000 && c <= 0x303f) return UBlockCode::UBLOCK_CJK_SYMBOLS_AND_PUNCTUATION;
	if (c >= 0x3040 && c <= 0x309f) return UBlockCode::UBLOCK_HIRAGANA;
	if (c >= 0x30a0 && c <= 0x30ff) return UBlockCode::UBLOCK_KATAKANA;
	if (c >= 0x3130 && c <= 0x318f) return UBlockCode::UBLOCK_HANGUL_COMPATIBILITY_JAMO;
	if (c >= 0x31c0 && c <= 0x31ef) return UBlockCode::UBLOCK_CJK_STROKES;
	if (c >= 0x31f0 && c <= 0x31ff) return UBlockCode::UBLOCK_KATAKANA_PHONETIC_EXTENSIONS;
	if (c >= 0x3200 && c <= 0x32ff) return UBlockCode::UBLOCK_ENCLOSED_CJK_LETTERS_AND_MONTHS;
	if (c >= 0x3300 && c <= 0x33ff) return UBlockCode::UBLOCK_CJK_COMPATIBILITY;
	if (c >= 0x3400 && c <= 0x4dbf) return UBlockCode::UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_A;
	if (c >= 0x4e00 && c <= 0x9fff) return UBlockCode::UBLOCK_CJK_UNIFIED_IDEOGRAPHS;
	if (c >= 0xa960 && c <= 0xa97f) return UBlockCode::UBLOCK_HANGUL_JAMO_EXTENDED_A;
	if (c >= 0xac00 && c <= 0xd7af) return UBlockCode::UBLOCK_HANGUL_SYLLABLES;
	if (c >= 0xd7b0 && c <= 0xd7ff) return UBlockCode::UBLOCK_HANGUL_JAMO_EXTENDED_B;
	if (c >= 0xf900 && c <= 0xfaff) return UBlockCode::UBLOCK_CJK_COMPATIBILITY_IDEOGRAPHS;
	if (c >= 0xfe30 && c <= 0xfe4f) return UBlockCode::UBLOCK_CJK_COMPATIBILITY_FORMS;
	if (c >= 0x2f800 && c <= 0x2fa1f) return UBlockCode::UBLOCK_CJK_COMPATIBILITY_IDEOGRAPHS_SUPPLEMENT;
	if (c >= 0x20000 && c <= 0x2ffff) return UBlockCode::UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_B;
	return UBlockCode::UBLOCK_INVALID;
}

namespace icu {

// The AmigaOS4/newlib port only needs this small subset of ICU UnicodeString.
// It deliberately retains UTF-16 indexing semantics so the existing BiDi and
// word-wrapping code behaves identically.
class UnicodeString {
public:
	UnicodeString() = default;

	UnicodeString(const char* input, const char* encoding) {
		if (input != nullptr && encoding != nullptr && std::string(encoding) == "UTF-8") {
			assign_utf8(input);
		}
	}

	[[nodiscard]] int32_t length() const {
		return static_cast<int32_t>(value_.size());
	}

	[[nodiscard]] UChar charAt(int32_t index) const {
		return index >= 0 && index < length() ? value_[static_cast<size_t>(index)] : UINT16_C(0xffff);
	}

	[[nodiscard]] UChar32 char32At(int32_t index) const {
		const UChar first = charAt(index);
		if (first >= UINT16_C(0xd800) && first <= UINT16_C(0xdbff)) {
			const UChar second = charAt(index + 1);
			if (second >= UINT16_C(0xdc00) && second <= UINT16_C(0xdfff)) {
				return UINT32_C(0x10000) +
				       ((static_cast<UChar32>(first) - UINT32_C(0xd800)) << 10) +
				       (static_cast<UChar32>(second) - UINT32_C(0xdc00));
			}
		}
		return first;
	}

	UnicodeString& insert(int32_t index, UChar character) {
		if (index >= 0 && index <= length()) {
			value_.insert(value_.begin() + index, character);
		}
		return *this;
	}

	UnicodeString& operator+=(UChar character) {
		value_.push_back(character);
		return *this;
	}

	UnicodeString& operator=(const char* input) {
		value_.clear();
		if (input != nullptr) {
			assign_utf8(input);
		}
		return *this;
	}

	[[nodiscard]] int32_t lastIndexOf(UChar character) const {
		const size_t position = value_.find_last_of(character);
		return position == std::u16string::npos ? -1 : static_cast<int32_t>(position);
	}

	[[nodiscard]] UnicodeString tempSubString(int32_t start, int32_t count) const {
		UnicodeString result;
		if (start < 0 || count <= 0 || start >= length()) {
			return result;
		}
		const size_t first = static_cast<size_t>(start);
		const size_t available = value_.size() - first;
		const size_t amount = static_cast<size_t>(count) < available ?
		                         static_cast<size_t>(count) : available;
		result.value_ = value_.substr(first, amount);
		return result;
	}

	std::string& toUTF8String(std::string& output) const {
		for (size_t index = 0; index < value_.size(); ++index) {
			UChar32 codepoint = value_[index];
			if (codepoint >= UINT32_C(0xd800) && codepoint <= UINT32_C(0xdbff) &&
			    index + 1 < value_.size()) {
				const UChar second = value_[index + 1];
				if (second >= UINT16_C(0xdc00) && second <= UINT16_C(0xdfff)) {
					codepoint = UINT32_C(0x10000) + ((codepoint - UINT32_C(0xd800)) << 10) +
					            (static_cast<UChar32>(second) - UINT32_C(0xdc00));
					++index;
				}
			}
			append_utf8(codepoint, output);
		}
		return output;
	}

private:
	static constexpr UChar32 kReplacementCharacter = UINT32_C(0xfffd);

	void assign_utf8(const char* input) {
		const auto* bytes = reinterpret_cast<const uint8_t*>(input);
		const size_t input_size = std::strlen(input);
		for (size_t index = 0; index < input_size;) {
			UChar32 codepoint = kReplacementCharacter;
			size_t consumed = 1;
			const uint8_t first = bytes[index];
			if (first < UINT8_C(0x80)) {
				codepoint = first;
			} else if (index + 1 < input_size && first >= UINT8_C(0xc2) &&
			           first <= UINT8_C(0xdf) &&
			           continuation(bytes[index + 1])) {
				codepoint = (static_cast<UChar32>(first & UINT8_C(0x1f)) << 6) |
				            (bytes[index + 1] & UINT8_C(0x3f));
				consumed = 2;
			} else if (index + 2 < input_size && first >= UINT8_C(0xe0) &&
			           first <= UINT8_C(0xef) &&
			           continuation(bytes[index + 1]) && continuation(bytes[index + 2])) {
				const UChar32 candidate = (static_cast<UChar32>(first & UINT8_C(0x0f)) << 12) |
				                          (static_cast<UChar32>(bytes[index + 1] & UINT8_C(0x3f)) << 6) |
				                          (bytes[index + 2] & UINT8_C(0x3f));
				if (candidate >= UINT32_C(0x800) &&
				    !(candidate >= UINT32_C(0xd800) && candidate <= UINT32_C(0xdfff))) {
					codepoint = candidate;
					consumed = 3;
				}
			} else if (index + 3 < input_size && first >= UINT8_C(0xf0) &&
			           first <= UINT8_C(0xf4) &&
			           continuation(bytes[index + 1]) && continuation(bytes[index + 2]) &&
			           continuation(bytes[index + 3])) {
				const UChar32 candidate = (static_cast<UChar32>(first & UINT8_C(0x07)) << 18) |
				                          (static_cast<UChar32>(bytes[index + 1] & UINT8_C(0x3f)) << 12) |
				                          (static_cast<UChar32>(bytes[index + 2] & UINT8_C(0x3f)) << 6) |
				                          (bytes[index + 3] & UINT8_C(0x3f));
				if (candidate >= UINT32_C(0x10000) && candidate <= UINT32_C(0x10ffff)) {
					codepoint = candidate;
					consumed = 4;
				}
			}
			append_utf16(codepoint);
			index += consumed;
		}
	}

	static bool continuation(uint8_t byte) {
		return (byte & UINT8_C(0xc0)) == UINT8_C(0x80);
	}

	void append_utf16(UChar32 codepoint) {
		if (codepoint <= UINT32_C(0xffff)) {
			value_.push_back(static_cast<UChar>(codepoint));
			return;
		}
		codepoint -= UINT32_C(0x10000);
		value_.push_back(static_cast<UChar>(UINT32_C(0xd800) + (codepoint >> 10)));
		value_.push_back(static_cast<UChar>(UINT32_C(0xdc00) + (codepoint & UINT32_C(0x3ff))));
	}

	static void append_utf8(UChar32 codepoint, std::string& output) {
		if (codepoint >= UINT32_C(0xd800) && codepoint <= UINT32_C(0xdfff)) {
			codepoint = kReplacementCharacter;
		}
		if (codepoint <= UINT32_C(0x7f)) {
			output.push_back(static_cast<char>(codepoint));
		} else if (codepoint <= UINT32_C(0x7ff)) {
			output.push_back(static_cast<char>(UINT32_C(0xc0) | (codepoint >> 6)));
			output.push_back(static_cast<char>(UINT32_C(0x80) | (codepoint & UINT32_C(0x3f))));
		} else if (codepoint <= UINT32_C(0xffff)) {
			output.push_back(static_cast<char>(UINT32_C(0xe0) | (codepoint >> 12)));
			output.push_back(static_cast<char>(UINT32_C(0x80) | ((codepoint >> 6) & UINT32_C(0x3f))));
			output.push_back(static_cast<char>(UINT32_C(0x80) | (codepoint & UINT32_C(0x3f))));
		} else {
			output.push_back(static_cast<char>(UINT32_C(0xf0) | (codepoint >> 18)));
			output.push_back(static_cast<char>(UINT32_C(0x80) | ((codepoint >> 12) & UINT32_C(0x3f))));
			output.push_back(static_cast<char>(UINT32_C(0x80) | ((codepoint >> 6) & UINT32_C(0x3f))));
			output.push_back(static_cast<char>(UINT32_C(0x80) | (codepoint & UINT32_C(0x3f))));
		}
	}

	std::u16string value_;
};

}  // namespace icu

#endif  // WL_USE_ICU
#endif  // WL_GRAPHIC_TEXT_UNICODE_H
