#ifndef __WOOKIE_SPLIT_HPP
#define __WOOKIE_SPLIT_HPP

#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/locale.hpp>

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
		split() {
			boost::locale::generator gen;
			m_loc = gen("en_US.UTF8");
		}

		mpos_t feed(const std::string &text, std::vector<std::string> &tokens) {
			std::vector<std::string> strs;
			boost::split(strs, text, boost::is_any_of(m_split_string));

			mpos_t mpos;
			mstem_t stems;

			int pos = 0;
			for (std::vector<std::string>::iterator it = strs.begin(); it != strs.end(); ++it) {
				std::string token = it->data();
				token = boost::locale::to_lower(token, m_loc);

				if (!token.size())
					continue;

				const char *lang = lang_detect(token.data(), token.size());

				mstem_t::iterator stem_it = stems.find(lang);
				if (stem_it == stems.end()) {
					boost::shared_ptr<stem> st(new stem(lang, NULL));
					stems.insert(std::make_pair(lang, st));

					token = st->get(token.data(), token.size());
				} else {
					token = stem_it->second->get(token.data(), token.size());
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
		std::locale m_loc;

		const char *lang_detect(const char *data, const int length) {
			bool is_plain_text = true;
			bool do_allow_extended_languages = true;
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
