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

struct ngram {
	ngram(int n, std::vector<long> &h) : n(n) {
		hashes.swap(h);
	}

	ngram() : n(0) {}
	ngram(int n) : n(n) {}
	ngram(const ngram &src) {
		n = src.n;
		hashes = src.hashes;
	}

	ngram(ngram &&src) {
		n = src.n;
		hashes.swap(src.hashes);
	}

	int n;
	std::vector<long> hashes;

	static ngram intersect(const ngram &f, const ngram &s) {
		ngram tmp(f.n);
		tmp.hashes.resize(f.hashes.size());

		auto inter = std::set_intersection(f.hashes.begin(), f.hashes.end(),
				s.hashes.begin(), s.hashes.end(), tmp.hashes.begin());
		tmp.hashes.resize(inter - tmp.hashes.begin());

		return tmp;
	}

	MSGPACK_DEFINE(n, hashes);
};

#define NGRAM_START	1
#define NGRAM_NUM	3


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

		void feed(const char *path, const std::string &enc) {
			std::ifstream in(path);
			if (in.bad())
				return;

			std::ostringstream ss;

			ss << in.rdbuf();

			m_parser.parse(ss.str(), enc);
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
			for (int i = NGRAM_START; i <= NGRAM_START + NGRAM_NUM; ++i) {
				std::vector<long> hashes;

				generate(text, i, hashes);

				ngrams.emplace_back(i, hashes);
			}
		}


	private:
		wookie::parser m_parser;
		wookie::ngram::detector m_charset_detector;
		boost::locale::generator m_gen;
		std::locale m_loc;

		void generate(const std::string &text, int ngram_num, std::vector<long> &hashes) {
			namespace lb = boost::locale::boundary;
			lb::ssegment_index wmap(lb::word, text.begin(), text.end(), m_loc);
			wmap.rule(lb::word_any);

			std::list<std::string> ngram;

			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				std::string token = boost::locale::to_lower(it->str(), m_loc);

				ngram.push_back(token);

				if ((int)ngram.size() == ngram_num) {
					std::ostringstream ss;
					std::copy(ngram.begin(), ngram.end(), std::ostream_iterator<std::string>(ss, ""));

					hashes.emplace_back(wookie::hash::murmur(ss.str(), 0));
					ngram.pop_front();
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
