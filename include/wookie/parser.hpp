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

#ifndef __WOOKIE_PARSER_HPP
#define __WOOKIE_PARSER_HPP

#include "wookie/dir.hpp"
#include "wookie/iconv.hpp"
#include "wookie/tfidf.hpp"
#include "wookie/url.hpp"

#include <buffio.h>
#include <tidy.h>
#include <tidyenum.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <iterator>
#include <list>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

#include <boost/locale.hpp>

#include <warp/ngram.hpp>

namespace ioremap { namespace wookie {

typedef long lngram;

#define NGRAM_NUM	4
#if NGRAM_NUM > 8 / 2
#error "NGRAM_NUM doesn't fit long with 2-byte charset"
#endif

class parser {
	public:
		parser() : m_loc(m_gen("en_US.UTF8")) {
		}

		~parser() {
		}

		void load_encodings(const std::string &base) {
			wookie::iterate_directory(base, std::bind(&warp::ngram::detector::load_file, &m_charset_detector,
						std::placeholders::_1, std::placeholders::_2));
		}

		void feed_file(const char *path) {
			reset();

			std::ifstream in(path);
			if (in.bad()) {
				std::ostringstream ss;
				ss << "parser: could not feed file '" << path << "': " << in.rdstate();
				throw std::runtime_error(ss.str());
			}

			std::ostringstream ss;

			ss << in.rdbuf();

			std::string text = ss.str();

			if (!m_magic.is_text(text.c_str(), std::min<int>(text.size(), 1024))) {
				std::ostringstream ss;
				ss << "parser: file '" << path << "' isn't text file";
				throw std::runtime_error(ss.str());
			}

			feed_text(ss.str());
		}

		void feed_text(const std::string &page) {
			reset();

			if (page.size() == 0)
				return;

			TidyBuffer errbuf;
			TidyDoc tdoc = tidyCreate();

			tidyBufInit(&errbuf);

			tidySetCharEncoding(tdoc, "raw");
			tidyOptSetBool(tdoc, TidyXhtmlOut, yes);
			tidySetErrorBuffer(tdoc, &errbuf);

			int err = tidyParseString(tdoc, page.c_str());
			if (err < 0) {
				tidyBufFree(&errbuf);
				std::ostringstream ss;
				ss << "parser: failed to parse page: " << err;
				throw std::runtime_error(ss.str());
			}

			TidyNode html = tidyGetHtml(tdoc);
			traverse_tree(tdoc, html);

			tidyBufFree(&errbuf);
			tidyRelease(tdoc);
		}

		const std::vector<std::string> &urls(void) const {
			return m_urls;
		}

		const std::vector<std::string> &tokens(void) const {
			return m_tokens;
		}

		std::string text(const char *join) const {
			std::ostringstream ss;

			std::copy(m_tokens.begin(), m_tokens.end(), std::ostream_iterator<std::string>(ss, join));
			return std::move(ss.str());
		}

		void generate_ngrams(const std::string &text, std::vector<lngram> &ngrams) {
			namespace lb = boost::locale::boundary;
			lb::ssegment_index cmap(lb::character, text.begin(), text.end(), m_loc);
			cmap.rule(lb::character_any);

			std::list<std::string> ng;
			for (auto it = cmap.begin(), end = cmap.end(); it != end; ++it) {
				ng.push_back(*it);

				if (ng.size() == NGRAM_NUM) {
					std::ostringstream ss;
					std::copy(ng.begin(), ng.end(), std::ostream_iterator<std::string>(ss, ""));

					std::string txt = ss.str();
					long tmp_ngram = 0;
					memcpy(&tmp_ngram, txt.data(), std::min(sizeof(lngram), txt.size()));

					ngrams.push_back(tmp_ngram);
					ng.pop_front();
				}
			}

			std::set<long> tmp(ngrams.begin(), ngrams.end());
			ngrams.assign(tmp.begin(), tmp.end());
		}

		std::vector<std::string> word_tokens(void) {
			std::vector<std::string> tokens;

			namespace lb = boost::locale::boundary;

			for (auto t = m_tokens.begin(); t != m_tokens.end(); ++t) {
				lb::ssegment_index wmap(lb::word, t->begin(), t->end(), m_loc);
				wmap.rule(lb::word_any);

				for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
					std::string token = boost::locale::to_lower(it->str(), m_loc);
					tokens.emplace_back(token);
				}
			}

			return tokens;
		}

		std::string string_tokens(const char *join) {
			std::vector<std::string> tokens = word_tokens();

			std::ostringstream ss;

			std::copy(tokens.begin(), tokens.end(), std::ostream_iterator<std::string>(ss, join));
			return std::move(ss.str());
		}

		void update_tfidf(const std::string &text, tfidf::tf &tf) {
			namespace lb = boost::locale::boundary;

			lb::ssegment_index wmap(lb::word, text.begin(), text.end(), m_loc);
			wmap.rule(lb::word_any);

			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				std::string token = boost::locale::to_lower(it->str(), m_loc);

				m_tfidf.feed_word_for_one_file(token);
				tf.feed_word(token);
			}

			m_tfidf.update_collected_df();
		}

		std::vector<tfidf::word_info> top(const tfidf::tf &tf, size_t num) {
			return m_tfidf.top(tf, num);
		}

		void merge_into(tfidf::tfidf &df) {
			df.merge(m_tfidf);
		}

	private:
		std::vector<std::string> m_urls;
		std::vector<std::string> m_tokens;

		warp::ngram::detector m_charset_detector;
		boost::locale::generator m_gen;
		std::locale m_loc;
		magic m_magic;

		tfidf::tfidf m_tfidf;

		void reset(void) {
			m_urls.clear();
			m_tokens.clear();
		}

		void traverse_tree(TidyDoc tdoc, TidyNode tnode) {
			TidyNode child;

			for (child = tidyGetChild(tnode); child; child = tidyGetNext(child)) {
				if (tidyNodeGetId(child) == TidyTag_A) {
					TidyAttr href = tidyAttrGetHREF(child);
					m_urls.push_back(convert(tidyAttrValue(href)));
				}

				if (tidyNodeIsSCRIPT(child) || tidyNodeIsSTYLE(child)) {
					continue;
				}

				if (tidyNodeIsText(child)) {
					if (tidyNodeHasText(tdoc, child)) {
						TidyBuffer buf;
						tidyBufInit(&buf);

						tidyNodeGetText(tdoc, child, &buf);
						std::string text;
						text.assign((char *)buf.bp, buf.size);
						m_tokens.emplace_back(convert(text));

						tidyBufFree(&buf);
					}
				}

				traverse_tree(tdoc, child);
			}
		}

		std::string convert(const std::string &text) {
			std::string enc = m_charset_detector.detect(text);
			//printf("encoding: %s, text-length: %zd\n", enc.c_str(), text.size());

			if (enc.size() && enc != "utf8") {
				wookie::charset_convert convert("utf8", enc.c_str());

				std::string out = convert.convert(text);
				//printf("coverted: %s -> %s, %zd -> %zd\n", enc.c_str(), "utf8", text.size(), out.size());

				if (out.size() > text.size() / 3)
					return out;
			}

			return text;
		}
};

}};

#endif /* __WOOKIE_PARSER_HPP */
