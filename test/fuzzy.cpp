#include "wookie/ngram.hpp"
#include "wookie/parser.hpp"

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
};

#define FUZZY_NGRAM_NUM	2

class fuzzy {
	public:
		fuzzy() : m_ngram(FUZZY_NGRAM_NUM), m_converted(false) {}

		void feed_text(const std::string &text) {
			namespace lb = boost::locale::boundary;

			lb::ssegment_index wmap(lb::word, text.begin(), text.end(), __fuzzy_locale);
			wmap.rule(lb::word_any);

			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				lstring word = lconvert::from_utf8(boost::locale::to_lower(it->str(), __fuzzy_locale));
				m_ngram.load(word, word);
			}
		}

		void search(const std::string &text) {
			if (!m_converted) {
				m_ngram.convert();
				m_converted = true;
			}

			lstring t = lconvert::from_utf8(boost::locale::to_lower(text, __fuzzy_locale));
			auto ngrams = wookie::ngram::ngram<lstring, lstring>::split(t, FUZZY_NGRAM_NUM);

			std::vector<lstring> ret;
			for (auto it = ngrams.begin(); it != ngrams.end(); ++it) {
				auto tmp = m_ngram.lookup_word(*it);

				std::cout << text << ": " << *it << ": ";
				for (auto n = tmp.begin(); n != tmp.end(); ++n)
					std::cout << *n << " ";
				std::cout << std::endl;
			}
		}

	private:
		wookie::ngram::ngram<lstring, lstring> m_ngram;
		bool m_converted;
};

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Fuzzy search tool options");
	size_t word_freq_num = 0;

	std::string enc_dir;
	generic.add_options()
		("help", "This help message")
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

	if (!files.size()) {
		std::cerr << "No input files\n" << generic << "\n" << hidden << std::endl;
		return -1;
	}

	try {
		wookie::parser parser;
		if (enc_dir.size())
			parser.load_encodings(enc_dir);

		fuzzy f;

		for (auto file = files.begin(); file != files.end(); ++file) {
			parser.feed_file(file->c_str());

			f.feed_text(parser.text(" "));
		}

		f.search(text);
	} catch (const std::exception &e) {
		std::cerr << "Exception: " << e.what() << std::endl;
		return -1;
	}

	return 0;
}
