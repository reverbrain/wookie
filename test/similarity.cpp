#include "wookie/parser.hpp"

#include <algorithm>
#include <fstream>
#include <list>
#include <sstream>
#include <thread>
#include <vector>

#include <iconv.h>
#include <string.h>

#include <boost/locale.hpp>

using namespace ioremap;

class charset_convert {
	public:
		charset_convert(const char *to, const char *from) {
			m_tmp.resize(128);

			m_iconv = iconv_open(to, from);
			if (m_iconv == (iconv_t)-1) {
				int err = errno;
				std::ostringstream ss;
				ss << "invalid conversion: " <<
					from << " -> " << to << " : " << strerror(err) << " [" << err << "]";
				throw std::runtime_error(ss.str());
			}
		}

		~charset_convert() {
			iconv_close(m_iconv);
		}

		void reset(void) {
			::iconv(m_iconv, NULL, NULL, NULL, NULL);
		}

		std::string convert(const std::string &in) {
			char *src = const_cast<char *>(&in[0]);
			size_t inleft = in.size();

			std::ostringstream out;

			while (inleft > 0) {
				char *dst = const_cast<char *>(&m_tmp[0]);
				size_t outleft = m_tmp.size();

				size_t size = ::iconv(m_iconv, &src, &inleft, &dst, &outleft);

				if (size == (size_t)-1) {
					if (errno == EINVAL)
						break;
					if (errno == E2BIG)
						continue;
					if (errno == EILSEQ) {
						src++;
						inleft--;
						continue;
					}
				}

				out.write(m_tmp.c_str(), m_tmp.size() - outleft);
			}

			return out.str();
		}

	private:
		iconv_t m_iconv;
		std::string m_tmp;
};

class document_parser {
	public:
		document_parser() : m_loc(m_gen("en_UR.UTF8")) {
		}

		void feed(const char *path, size_t ngram_num) {
			std::ifstream in(path);
			std::ostringstream ss;

			ss << in.rdbuf();

			m_tokens.clear();
			m_hashes.clear();

			m_parser.parse(ss.str(), "utf8");
			generate_tokens(ngram_num);
		}

		std::string text(const char *delim) const {
			return m_parser.text(delim);
		}

		const std::vector<std::string> &tokens(void) const {
			return m_tokens;
		}

		const std::vector<long> &hashes(void) const {
			return m_hashes;
		}

	private:
		wookie::parser m_parser;
		boost::locale::generator m_gen;
		std::locale m_loc;

		std::vector<std::string> m_tokens;
		std::vector<long> m_hashes;

		void generate_tokens(size_t ngram_num) {
			std::string text = m_parser.text(" ");

			namespace lb = boost::locale::boundary;
			lb::ssegment_index wmap(lb::word, text.begin(), text.end(), m_loc);
			wmap.rule(lb::word_any);

			std::list<std::string> ngram;

			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				std::string token = boost::locale::to_lower(it->str(), m_loc);

				ngram.push_back(token);
				m_tokens.emplace_back(token);

				if (ngram.size() == ngram_num) {
					std::ostringstream ss;
					std::copy(ngram.begin(), ngram.end(), std::ostream_iterator<std::string>(ss, ""));

					m_hashes.emplace_back(hash(ss.str(), 0));
					ngram.pop_front();
				}
			}

			std::set<long> tmp(m_hashes.begin(), m_hashes.end());
			m_hashes.assign(tmp.begin(), tmp.end());
		}


		// murmur hash
		long hash(const std::string &str, long seed) const {
			const uint64_t m = 0xc6a4a7935bd1e995LLU;
			const int r = 47;

			long h = seed ^ (str.size() * m);

			const uint64_t *data = (const uint64_t *)str.data();
			const uint64_t *end = data + (str.size() / 8);

			while (data != end) {
				uint64_t k = *data++;

				k *= m;
				k ^= k >> r;
				k *= m;

				h ^= k;
				h *= m;
			}

			const unsigned char *data2 = (const unsigned char *)data;

			switch (str.size() & 7) {
			case 7: h ^= (uint64_t)data2[6] << 48;
			case 6: h ^= (uint64_t)data2[5] << 40;
			case 5: h ^= (uint64_t)data2[4] << 32;
			case 4: h ^= (uint64_t)data2[3] << 24;
			case 3: h ^= (uint64_t)data2[2] << 16;
			case 2: h ^= (uint64_t)data2[1] << 8;
			case 1: h ^= (uint64_t)data2[0];
				h *= m;
			};

			h ^= h >> r;
			h *= m;
			h ^= h >> r;

			return h;
		}
};

class document {
	public:
		document(const char *path, const std::vector<long> &hashes, size_t num = 0) : m_path(path) {
			if (num > hashes.size() || !num)
				num = hashes.size();

			m_hashes.reserve(num);
			m_hashes.assign(hashes.begin(), hashes.begin() + num);
		}

		const std::string &name(void) const {
			return m_path;
		}

		const std::vector<long> &hashes(void) const {
			return m_hashes;
		}

		bool operator==(const document &doc) const {
			return equality(doc);
		}

	private:
		std::string m_path;
		std::vector<long> m_hashes;

		bool equality(const document &doc) const {
			size_t matched = 0;
			for (const auto h : doc.hashes()) {
				if (std::binary_search(m_hashes.begin(), m_hashes.end(), h)) {
					matched++;
				}
			}

			if (matched >= doc.hashes().size() * 1 / 10 + 1) {
#if 1
				printf("equal: %s [%zd] vs %s [%zd]: %zd\n",
					name().c_str(), hashes().size(),
					doc.name().c_str(), doc.hashes().size(),
					matched);
#endif
				return true;
			}

			return false;
		}
};

int main(int argc, char *argv[])
{
	document_parser p;
	std::vector<document> docs;

	for (int i = 1; i < argc; ++i) {
		try {
			p.feed(argv[i], 4);
			if (p.hashes().size() > 0)
				docs.emplace_back(argv[i], p.hashes());

#if 0
			std::cout << "================================" << std::endl;
			std::cout << argv[i] << ": hashes: " << p.hashes().size() << std::endl;
			std::ostringstream ss;
			std::vector<std::string> tokens = p.tokens();

			std::copy(tokens.begin(), tokens.end(), std::ostream_iterator<std::string>(ss, " "));
			std::cout << ss.str() << std::endl;
#endif
		} catch (const std::exception &e) {
			std::cerr << argv[i] << ": caught exception: " << e.what() << std::endl;
		}
	}

	std::vector<std::thread> threads;

	struct doc_thread {
		const std::vector<document> &docs;
		int id;
		int step;

		doc_thread(const std::vector<document> &d) : docs(d), id(0), step(0) {}

		void operator()(void) {
			for (size_t i = id; i < docs.size(); i += step) {
				const document &doc = docs[i];

				for (size_t j = i + 1; j < docs.size(); ++j) {
					const document &tmp = docs[j];

					if (tmp == doc) {
						printf("%d/%d: equal: %s vs %s\n",
							id, step, doc.name().c_str(), tmp.name().c_str());
					}
				}
			}
		}
	};

	int step = 16;
	for (int i = 0; i < step; ++i) {
		struct doc_thread dth(docs);
		dth.id = i;
		dth.step = step;

		threads.emplace_back(dth);
	}

	for (int i = 0; i < step; ++i) {
		threads[i].join();
	}
}
