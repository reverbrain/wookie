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

#ifndef __WOOKIE_SPLIT_HPP
#define __WOOKIE_SPLIT_HPP

#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/locale.hpp>

#include <warp/lex.hpp>

/*
 * None knows, but compact-language-detector does not work without it
 */
#define CLD_WINDOWS

#include "cld/compact_lang_det.h"
#include "cld/ext_lang_enc.h"
#include "cld/lang_enc.h"

#include <stem/stem.hpp>

namespace ioremap { namespace wookie {

typedef std::map<std::string, std::vector<int> > mpos_t;
typedef std::map<std::string, boost::shared_ptr<stem> > mstem_t;

class split {
	public:
		split() : m_loc(m_gen("en_US.UTF8")), m_lex(m_loc), m_lex_loaded(false) {}
		split(const std::string &path) : split() {
			if (path.size()) {
				m_lex.load(path);
				m_lex_loaded = true;
			}
		}

		mpos_t feed(const std::string &text, std::vector<std::string> &tokens) {
			lb::ssegment_index wmap(lb::word, text.begin(), text.end(), m_loc);
			wmap.rule(lb::word_any);

			mpos_t mpos;
			mstem_t stems;

			int pos = 0;
			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				std::string token = boost::locale::to_lower(it->str(), m_loc);

				std::string tmp = m_lex.root(token);
				if (tmp.size()) {
					token = tmp;
				} else {
					const char *lang = lang_detect(token.data(), token.size());

					mstem_t::iterator stem_it = stems.find(lang);
					if (stem_it == stems.end()) {
						boost::shared_ptr<stem> st(new stem(lang, NULL));
						stems.insert(std::make_pair(lang, st));

						token = st->get(token.data(), token.size());
					} else {
						token = stem_it->second->get(token.data(), token.size());
					}
				}

				if (token.size()) {
					mpos_t::iterator it = mpos.find(token);
					if (it != mpos.end()) {
						it->second.push_back(pos);
					} else {
						std::vector<int> vec;
						vec.push_back(pos);
						mpos.insert(std::make_pair(token, vec));

						tokens.push_back(token);
					}
				}

				++pos;
			}

			return mpos;
		}

	private:
		static const std::string m_split_string;
		boost::locale::generator m_gen;
		std::locale m_loc;
		ioremap::warp::lex m_lex;
		bool m_lex_loaded;

		const char *lang_detect(const char *data, const int length) {
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

			Language lang;
			lang = CompactLangDet::DetectLanguage(0,
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

			if (!IsValidLanguage(lang))
				return "eng";

			return LanguageCodeISO639_2(lang);
		}

};

}}

#endif /* __WOOKIE_SPLIT_HPP */
