/*
 * Copyright 2013+ Evgeniy Polyakov <zbr@ioremap.net>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __WOOKIE_LANG_DETECT_HPP
#define __WOOKIE_LANG_DETECT_HPP

/*
 * None knows, but compact-language-detector does not work without it
 */
#define CLD_WINDOWS

#include "cld/compact_lang_det.h"
#include "cld/ext_lang_enc.h"
#include "cld/lang_enc.h"

namespace ioremap { namespace wookie {

class lang_detect {
	public:
		lang_detect(const char *data, const int length) {
			bool is_plain_text = true;
			bool do_allow_extended_languages = false;
			bool do_pick_summary_language = false;
			bool do_remove_weak_matches = false;
			bool is_reliable;
			const char* tld_hint = NULL;
			int encoding_hint = UNKNOWN_ENCODING;
			Language language_hint = UNKNOWN_LANGUAGE;

			double normalized_score3[3];
			Language language3[3];
			int percent3[3];
			int text_bytes;

			m_lang = CompactLangDet::DetectLanguage(0,
					data, length,
					is_plain_text,
					do_allow_extended_languages,
					do_pick_summary_language,
					do_remove_weak_matches,
					tld_hint,
					encoding_hint,
					language_hint,
					language3,
					percent3,
					normalized_score3,
					&text_bytes,
					&is_reliable);
		}

		const char *lang(void) const {
			if (!IsValidLanguage(m_lang))
				return "eng";

			return LanguageCodeISO639_2(m_lang);
		}
	private:
		Language m_lang;

};

}} // namespace ioremap::wookie


#endif /* __WOOKIE_LANG_DETECT_HPP */
