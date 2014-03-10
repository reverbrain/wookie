#include "wookie/ngram.hpp"
#include "wookie/parser.hpp"
#include "wookie/timer.hpp"

#include "warp/pack.hpp"

#include <algorithm>
#include <fstream>
#include <list>
#include <sstream>
#include <string>
#include <vector>

#include <boost/locale.hpp>
#include <boost/program_options.hpp>
#include <boost/locale/util.hpp>

using namespace ioremap;

static const boost::locale::generator __fuzzy_locale_generator;
static const std::locale __fuzzy_locale(__fuzzy_locale_generator("en_US.UTF8"));
static const auto __fuzzy_utf8_converter = boost::locale::util::create_utf8_converter();

#define LOAD_ROOTS

struct letter {
	unsigned int	l;

	letter() : l(0) {}
	letter(unsigned int _l) : l(_l) {}
	letter(const letter &other) {
		l = other.l;
	}

	std::string str() const {
		char tmp[8];
		memset(tmp, 0, sizeof(tmp));
		__fuzzy_utf8_converter->from_unicode(l, tmp, tmp + 8);

		return tmp;
	}

	bool operator==(const letter &other) const {
		return l == other.l;
	}
};

inline std::ostream &operator <<(std::ostream &out, const letter &l)
{
	out << l.str();
	return out;
}

struct letter_traits {
	typedef letter char_type;
	typedef letter int_type;
	typedef std::streampos pos_type;
	typedef std::streamoff off_type;
	typedef std::mbstate_t state_type;

	static void assign(char_type &c1, const char_type &c2) {
		c1 = c2;
	}

	static bool eq(const char_type &c1, const char_type &c2) {
		return c1.l == c2.l;
	}

	static bool lt(const char_type &c1, const char_type &c2) {
		return c1.l < c2.l;
	}

	static int compare(const char_type *s1, const char_type *s2, std::size_t n) {
		for (std::size_t i = 0; i < n; ++i) {
			if (eq(s1[i], char_type())) {
				if (eq(s2[i], char_type())) {
					return 0;
				}

				return -1;
			}

			if (lt(s1[i], s2[i]))
				return -1;
			else if (lt(s2[i], s1[i]))
				return 1;
		}

		return 0;
	}

	static std::size_t length(const char_type* s) {
		std::size_t i = 0;

		while (!eq(s[i], char_type()))
			++i;

		return i;
	}

	static const char_type *find(const char_type *s, std::size_t n, const char_type& a) {
		for (std::size_t i = 0; i < n; ++i)
			if (eq(s[i], a))
				return s + i;
		return 0;
	}

	static char_type *move(char_type *s1, const char_type *s2, std::size_t n) {
		return static_cast<char_type *>(memmove(s1, s2, n * sizeof(char_type)));
	}

	static char_type *copy(char_type *s1, const char_type *s2, std::size_t n) {
		std::copy(s2, s2 + n, s1);
		return s1;
	}

	static char_type *assign(char_type *s, std::size_t n, char_type a) {
		std::fill_n(s, n, a);
		return s;
	}

	static char_type to_char_type(const int_type &c) {
		return static_cast<char_type>(c);
	}

	static int_type to_int_type(const char_type &c) {
		return static_cast<int_type>(c);
	}

	static bool eq_int_type(const int_type &c1, const int_type &c2) {
		return c1.l == c2.l;
	}

	static int_type eof() {
		return static_cast<int_type>(~0U);
	}

	static int_type not_eof(const int_type &c) {
		return !eq_int_type(c, eof()) ? c : to_int_type(char_type());
	}
};

typedef std::basic_string<letter, letter_traits> lstring;

inline std::ostream &operator <<(std::ostream &out, const lstring &ls)
{
	for (auto it = ls.begin(); it != ls.end(); ++it) {
		out << *it;
	}
	return out;
}


class lconvert {
	public:
		static lstring from_utf8(const char *text, size_t size) {
			namespace lb = boost::locale::boundary;
			std::string::const_iterator begin(text);
			std::string::const_iterator end(text + size);

			lb::ssegment_index wmap(lb::character, begin, end, __fuzzy_locale);
			wmap.rule(lb::character_any);

			lstring ret;

			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				std::string str = it->str();
				const char *ptr = str.c_str();
				auto code = __fuzzy_utf8_converter->to_unicode(ptr, ptr + str.size());

				letter l(code);
				ret.append(&l, 1);
			}

			return ret;
		}

		static lstring from_utf8(const std::string &text) {
			return from_utf8(text.c_str(), text.size());
		}

		static std::string to_string(const lstring &l) {
			std::ostringstream ss;
			ss << l;
			return ss.str();
		}
};

class fuzzy {
	public:
		fuzzy(int num) : m_ngram(num) {}

		void feed_text(const std::string &text) {
			namespace lb = boost::locale::boundary;

			lb::ssegment_index wmap(lb::word, text.begin(), text.end(), __fuzzy_locale);
			wmap.rule(lb::word_any);

			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				lstring word = lconvert::from_utf8(boost::locale::to_lower(it->str(), __fuzzy_locale));
				m_ngram.load(word, word);
			}
		}

		void feed_word(const lstring &word) {
			m_ngram.load(word, word);
		}

		std::vector<wookie::ngram::ncount<lstring>> search(const std::string &text) {
			lstring t = lconvert::from_utf8(boost::locale::to_lower(text, __fuzzy_locale));
			return search(t);
		}

		std::vector<wookie::ngram::ncount<lstring>> search(const lstring &text) {
			auto ngrams = wookie::ngram::ngram<lstring, lstring>::split(text, m_ngram.n());

			std::map<lstring, int> word_count;

			for (auto it = ngrams.begin(); it != ngrams.end(); ++it) {
				auto tmp = m_ngram.lookup_word(*it);

				for (auto word = tmp.begin(); word != tmp.end(); ++word) {
					auto wc = word_count.find(*word);
					if (wc == word_count.end())
						word_count[*word] = 1;
					else
						wc->second++;
				}
			}

			std::vector<wookie::ngram::ncount<lstring>> counts;
			for (auto wc = word_count.begin(); wc != word_count.end(); ++wc) {
				wookie::ngram::ncount<lstring> nc;
				nc.word = wc->first;
				nc.count = (double)wc->second / (double)wc->first.size();

				if (nc.count > 0.01)
					counts.emplace_back(nc);
			}

			std::sort(counts.begin(), counts.end());
#if 0
			std::cout << text << "\n";
			for (auto nc = counts.begin(); nc != counts.end(); ++nc) {
				std::cout << nc->word << ": " << nc->count << std::endl;
			}
#endif
			return counts;
		}

	private:
		wookie::ngram::ngram<lstring, lstring> m_ngram;
};

class spell {
	public:
		spell(int ngram, const std::string &path) : m_fuzzy(ngram) {
			wookie::timer tm;

			warp::unpacker unpack(path);
#ifdef LOAD_ROOTS
			unpack.unpack(std::bind(&spell::unpack_roots, this, std::placeholders::_1));
#else
			unpack.unpack(std::bind(&spell::unpack_everything, this, std::placeholders::_1));
#endif

			printf("spell checker loaded: words: %zd, time: %lld ms\n",
					m_fe.size(), (unsigned long long)tm.elapsed());
		}

		std::vector<lstring> search(const std::string &text) {
			wookie::timer tm;

			lstring t = lconvert::from_utf8(boost::locale::to_lower(text, __fuzzy_locale));
			auto fsearch = m_fuzzy.search(t);

			printf("spell checker lookup: rough search: words: %zd, fuzzy-search-time: %lld ms\n",
					fsearch.size(), (unsigned long long)tm.elapsed());

			std::vector<lstring> ret;

#ifdef LOAD_ROOTS
			ret = search_roots(t, fsearch);
#else
			ret = search_everything(t, fsearch);
#endif

			printf("spell checker lookup: checked: words: %zd, total-search-time: %lld ms:\n",
					ret.size(), (unsigned long long)tm.restart());

			for (auto r = ret.begin(); r != ret.end(); ++r)
				std::cout << *r << std::endl;
			return ret;
		}

	private:
		std::map<lstring, std::vector<warp::feature_ending>> m_fe;
		fuzzy m_fuzzy;

		bool unpack_roots(const warp::entry &e) {
			lstring tmp = lconvert::from_utf8(e.root);
			m_fe[tmp] = e.fe;
			m_fuzzy.feed_word(tmp);
			return true;
		}

		bool unpack_everything(const warp::entry &e) {
			for (auto f = e.fe.begin(); f != e.fe.end(); ++f) {
				std::string word = e.root + f->ending;
				m_fuzzy.feed_text(word);
			}
			return true;
		}

		std::vector<lstring> search_roots(const lstring &t, const std::vector<wookie::ngram::ncount<lstring>> &fsearch) {
			wookie::timer tm;

			std::vector<lstring> ret;
			int min_dist = 1024;

			long total_endings = 0;
			long total_words = 0;

			ret.reserve(fsearch.size() / 4);
			for (auto w = fsearch.begin(); w != fsearch.end(); ++w) {
				const auto & fe = m_fe.find(w->word);
				if (fe != m_fe.end()) {
					std::set<std::string> checked_endings;

					for (auto ending = fe->second.begin(); ending != fe->second.end(); ++ending) {
						auto tmp_end = checked_endings.find(ending->ending);
						if (tmp_end != checked_endings.end())
							continue;

						checked_endings.insert(ending->ending);
						lstring word = w->word + lconvert::from_utf8(ending->ending);

						int dist = ldist(t, word);
						if (dist <= min_dist) {
							if (dist < min_dist)
								ret.clear();

							ret.emplace_back(word);
							min_dist = dist;
						}

						total_endings++;
					}
					total_words++;
				}
			}

			printf("spell checker lookup: checked endings: roots: %ld, endings: %ld, dist: %d, time: %lld ms:\n",
					total_words, total_endings,
					min_dist, (unsigned long long)tm.restart());

			return ret;
		}

		std::vector<lstring> search_everything(const lstring &t, const std::vector<wookie::ngram::ncount<lstring>> &fsearch) {
			std::vector<lstring> ret;
			int min_dist = 1024;

			ret.reserve(fsearch.size());

			for (auto w = fsearch.begin(); w != fsearch.end(); ++w) {
				int dist = ldist(t, w->word);
				if (dist <= min_dist) {
					if (dist < min_dist)
						ret.clear();

					ret.emplace_back(w->word);
					min_dist = dist;
				}
			}

			return ret;
		}


		int ldist(const lstring &s, const lstring &t) {
			// degenerate cases
			if (s == t)
				return 0;
			if (s.size() == 0)
				return t.size();
			if (t.size() == 0)
				return s.size();

			// create two work vectors of integer distances
			std::vector<int> v0(t.size() + 1);
			std::vector<int> v1(t.size() + 1);

			// initialize v0 (the previous row of distances)
			// this row is A[0][i]: edit distance for an empty s
			// the distance is just the number of characters to delete from t
			for (size_t i = 0; i < v0.size(); ++i)
				v0[i] = i;

			for (size_t i = 0; i < s.size(); ++i) {
				// calculate v1 (current row distances) from the previous row v0

				// first element of v1 is A[i+1][0]
				//   edit distance is delete (i+1) chars from s to match empty t
				v1[0] = i + 1;

				// use formula to fill in the rest of the row
				for (size_t j = 0; j < t.size(); ++j) {
					int cost = (s[i] == t[j]) ? 0 : 1;
					v1[j + 1] = std::min(v1[j] + 1, v0[j + 1] + 1);
					v1[j + 1] = std::min(v1[j + 1], v0[j] + cost);
				}

				// copy v1 (current row) to v0 (previous row) for next iteration
				v0 = v1;
			}

			return v1[t.size()];
		}
};

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Fuzzy search tool options");

	int num;
	std::string enc_dir, msgin;
	generic.add_options()
		("help", "This help message")
		("ngram", bpo::value<int>(&num)->default_value(3), "Number of symbols in each ngram")
		("msgpack-input", bpo::value<std::string>(&msgin), "Packed Zaliznyak dictionary file")
		("encoding-dir", bpo::value<std::string>(&enc_dir), "Load encodings from given wookie directory")
		;

	bpo::positional_options_description p;
	p.add("text", 1).add("files", -1);

	std::vector<std::string> files;
	std::string text;

	bpo::options_description hidden("Positional options");
	hidden.add_options()
		("text", bpo::value<std::string>(&text), "text to lookup")
		("files", bpo::value<std::vector<std::string>>(&files), "files to parse")
	;

	bpo::variables_map vm;

	try {
		bpo::options_description cmdline_options;
		cmdline_options.add(generic).add(hidden);

		bpo::store(bpo::command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), vm);
		bpo::notify(vm);
	} catch (const std::exception &e) {
		std::cerr << "Invalid options: " << e.what() << "\n" << generic << std::endl;
		return -1;
	}

	if (!text.size()) {
		std::cerr << "No text to lookup\n" << generic << "\n" << hidden << std::endl;
		return -1;
	}

	if (!files.size() && !msgin.size()) {
		std::cerr << "There is no input files nor packed input\n" << generic << "\n" << hidden << std::endl;
		return -1;
	}

	try {
		if (msgin.size()) {
			spell sp(num, msgin);
			sp.search(text);
		} else {
			wookie::parser parser;
			if (enc_dir.size())
				parser.load_encodings(enc_dir);

			fuzzy f(num);

			for (auto file = files.begin(); file != files.end(); ++file) {
				parser.feed_file(file->c_str());

				f.feed_text(parser.text(" "));
			}

			f.search(text);
		}
	} catch (const std::exception &e) {
		std::cerr << "Exception: " << e.what() << std::endl;
		return -1;
	}

	return 0;
}
