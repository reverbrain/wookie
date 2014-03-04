#ifndef __WOOKIE_SIMILARITY_HPP
#define __WOOKIE_SIMILARITY_HPP

#include "simdoc.hpp"

#include "wookie/dir.hpp"
#include "wookie/iconv.hpp"
#include "wookie/hash.hpp"
#include "wookie/lexical_cast.hpp"
#include "wookie/ngram.hpp"
#include "wookie/parser.hpp"
#include "wookie/tfidf.hpp"
#include "wookie/timer.hpp"
#include "wookie/url.hpp"

#include <algorithm>
#include <fstream>
#include <list>
#include <sstream>
#include <vector>

#include <boost/locale.hpp>
#include <boost/program_options.hpp>

#include <msgpack.hpp>

namespace ioremap { namespace similarity {

static inline std::vector<ngram> intersect(const std::vector<ngram> &f, const std::vector<ngram> &s)
{
	std::vector<ngram> tmp;

	if (!f.size() || !s.size())
		return tmp;

	tmp.resize(std::min(f.size(), s.size()));
	auto inter = std::set_intersection(f.begin(), f.end(), s.begin(), s.end(), tmp.begin());
	tmp.resize(inter - tmp.begin());

	return tmp;
}

namespace lb = boost::locale::boundary;

class document_parser {
	public:
		document_parser() : m_loc(m_gen("en_US.UTF8")) {
		}

		void load_encodings(const std::string &base) {
			wookie::iterate_directory(base, std::bind(&wookie::ngram::detector::load_file, &m_charset_detector,
						std::placeholders::_1, std::placeholders::_2));
		}

		bool feed(const char *path) {
			std::ifstream in(path);
			if (in.bad())
				return false;

			std::ostringstream ss;

			ss << in.rdbuf();

			std::string text = ss.str();

			if (!m_magic.is_text(text.c_str(), std::min<int>(text.size(), 1024)))
				return false;

			m_parser.parse(ss.str());
			return true;
		}

		std::string text(bool tokenize) {
			std::string text = m_parser.text(" ");
			std::string enc = m_charset_detector.detect(text);
			//printf("encoding: %s, text-length: %zd\n", enc.c_str(), text.size());

			if (enc.size() && enc != "utf8") {
				wookie::charset_convert convert("utf8", enc.c_str());

				std::string out = convert.convert(text);
				//printf("coverted: %s -> %s, %zd -> %zd\n", enc.c_str(), "utf8", text.size(), out.size());

				if (out.size() > text.size() / 3)
					text = out;
			}

			if (tokenize) {
				lb::ssegment_index wmap(lb::word, text.begin(), text.end(), m_loc);
				wmap.rule(lb::word_any);

				std::ostringstream tokens;

				for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
					std::string token = boost::locale::to_lower(it->str(), m_loc);
					tokens << token << " ";
				}

				text = tokens.str();
			}

			return text;
		}

		void update_tfidf(const std::string &text, wookie::tfidf::tf &tf) {
			lb::ssegment_index wmap(lb::word, text.begin(), text.end(), m_loc);
			wmap.rule(lb::word_any);

			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				std::string token = boost::locale::to_lower(it->str(), m_loc);

				m_tfidf.feed_word_for_one_file(token);
				tf.feed_word(token);
			}

			m_tfidf.update_collected_df();
		}

		void generate_ngrams(const std::string &text, std::vector<ngram> &ngrams) {
			lb::ssegment_index cmap(lb::character, text.begin(), text.end(), m_loc);
			cmap.rule(lb::character_any);

			std::list<std::string> ng;
			for (auto it = cmap.begin(), end = cmap.end(); it != end; ++it) {
				ng.push_back(*it);

				if (ng.size() == NGRAM_NUM) {
					std::ostringstream ss;
					std::copy(ng.begin(), ng.end(), std::ostream_iterator<std::string>(ss, ""));

					std::string txt = ss.str();
					ngram tmp_ngram = 0;
					memcpy(&tmp_ngram, txt.data(), std::min(sizeof(ngram), txt.size()));

					ngrams.push_back(tmp_ngram);
					ng.pop_front();
				}
			}

			std::set<long> tmp(ngrams.begin(), ngrams.end());
			ngrams.assign(tmp.begin(), tmp.end());
		}

		std::vector<wookie::tfidf::word_info> top(const wookie::tfidf::tf &tf, size_t num) {
			return m_tfidf.top(tf, num);
		}

		void merge_into(wookie::tfidf::tfidf &df) {
			df.merge(m_tfidf);
		}

	private:
		wookie::parser m_parser;
		wookie::ngram::detector m_charset_detector;
		boost::locale::generator m_gen;
		std::locale m_loc;
		wookie::magic m_magic;
		wookie::tfidf::tfidf m_tfidf;
};

struct learn_element {
	learn_element() : label(-1), valid(false) {
	}

	std::vector<int> doc_ids;
	std::string request;
	std::vector<ngram> req_ngrams;

	int label;
	bool valid;

	std::vector<int> features;

	MSGPACK_DEFINE(doc_ids, label, request, req_ngrams);

	bool generate_features(const simdoc &d1, const simdoc &d2) {
		const std::vector<ngram> &f = d1.ngrams;
		const std::vector<ngram> &s = d2.ngrams;

		if (!f.size() || !s.size())
			return false;

		features.push_back(f.size());
		features.push_back(s.size());

		std::vector<ngram> inter = intersect(f, s);
		features.push_back(inter.size());

		features.push_back(req_ngrams.size());
		features.push_back(intersect(inter, req_ngrams).size());

		valid = true;
		return true;
	}
};

}} // namespace ioremap::similarity

#endif /* __WOOKIE_SIMILARITY_HPP */
