#ifndef __WOOKIE_SIMILARITY_HPP
#define __WOOKIE_SIMILARITY_HPP

#include "wookie/iconv.hpp"
#include "wookie/hash.hpp"
#include "wookie/lexical_cast.hpp"
#include "wookie/ngram.hpp"
#include "wookie/parser.hpp"
#include "wookie/timer.hpp"

#include <algorithm>
#include <fstream>
#include <list>
#include <sstream>
#include <vector>

#include <boost/locale.hpp>
#include <boost/program_options.hpp>

#include <msgpack.hpp>

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>

namespace ioremap { namespace similarity {

typedef long ngram;

#define NGRAM_NUM	4
#if NGRAM_NUM > 8 / 2
#error "NGRAM_NUM doesn't fit long with 2-byte charset"
#endif

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
			int fd;
			DIR *dir;
			struct dirent64 *d;

			if (base.size() == 0)
				return;

			fd = openat(AT_FDCWD, base.c_str(), O_RDONLY);
			if (fd == -1) {
				std::ostringstream ss;
				ss << "failed to open dir '" << base << "': " << strerror(errno);
				throw std::runtime_error(ss.str());
			}

			dir = fdopendir(fd);

			while ((d = readdir64(dir)) != NULL) {
				if (d->d_name[0] == '.' && d->d_name[1] == '\0')
					continue;
				if (d->d_name[0] == '.' && d->d_name[1] == '.' && d->d_name[2] == '\0')
					continue;

				if (d->d_type != DT_DIR) {
					m_charset_detector.load_file((base + "/" + d->d_name).c_str(), d->d_name);
				}
			}
			close(fd);
		}

		void feed(const char *path) {
			std::ifstream in(path);
			if (in.bad())
				return;

			std::ostringstream ss;

			ss << in.rdbuf();

			m_parser.parse(ss.str());
		}

		std::string text(void) const {
			std::string text = m_parser.text(" ");
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

		void generate_ngrams(const std::string &text, std::vector<ngram> &ngrams) {
			generate(text, ngrams);
		}


	private:
		wookie::parser m_parser;
		wookie::ngram::detector m_charset_detector;
		boost::locale::generator m_gen;
		std::locale m_loc;

		void generate(const std::string &text, std::vector<ngram> &hashes) {
			lb::ssegment_index wmap(lb::word, text.begin(), text.end(), m_loc);
			wmap.rule(lb::word_any);

			std::ostringstream tokens;

			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				std::string token = boost::locale::to_lower(it->str(), m_loc);
				tokens << token;
			}

			std::string tstr = tokens.str();
			lb::ssegment_index cmap(lb::character, tstr.begin(), tstr.end(), m_loc);
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

					hashes.push_back(tmp_ngram);
					ng.pop_front();
				}
			}

			std::set<long> tmp(hashes.begin(), hashes.end());
			hashes.assign(tmp.begin(), tmp.end());
		}
};

class document {
	public:
		document(int doc_id) : m_doc_id(doc_id) {
		}

		std::vector<ngram> &ngrams(void) {
			return m_ngrams;
		}

		const std::vector<ngram> &ngrams(void) const {
			return m_ngrams;
		}

		int id(void) const {
			return m_doc_id;
		}

	private:
		int m_doc_id;
		std::vector<ngram> m_ngrams;
};

struct learn_element {
	learn_element() : valid(false) {
	}

	std::vector<int> doc_ids;
	std::string request;
	bool valid;

	std::vector<int> features;

	MSGPACK_DEFINE(doc_ids, request);
};

}} // namespace ioremap::similarity

#endif /* __WOOKIE_SIMILARITY_HPP */
