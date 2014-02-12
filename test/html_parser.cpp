#include "similarity.hpp"

#include "wookie/parser.hpp"
#include "wookie/lexical_cast.hpp"

#include <algorithm>
#include <fstream>
#include <list>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#include <string.h>

#include <boost/locale.hpp>
#include <boost/program_options.hpp>

using namespace ioremap;
using namespace ioremap::similarity;

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Document parser options");

	std::string enc_dir;
	generic.add_options()
		("help", "This help message")
		("tokenize", "Tokenize text")
		("encoding-dir", bpo::value<std::string>(&enc_dir), "Load encodings from given wookie directory")
		("ngrams", "Generate ngrams and their intersection")
		;

	bpo::positional_options_description p;
	p.add("files", -1);

	std::vector<std::string> files;
	bpo::options_description hidden("Hidden options");
	hidden.add_options()
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

	if (!files.size()) {
		std::cerr << "No input files\n" << generic << std::endl;
		return -1;
	}

	document_parser parser;

	if (enc_dir.size())
		parser.load_encodings(enc_dir);

	std::vector<simdoc> documents;

	for (auto f : files) {
		try {
			std::cout << "================================" << std::endl;
			std::cout << f << std::endl;

			if (!parser.feed(f.c_str())) {
				std::cout << "NOT A TEXT" << std::endl;
				continue;
			}

			std::string text = parser.text();

			if (vm.count("tokenize")) {
				boost::locale::generator gen;
				std::locale loc(gen("en_US.UTF8"));

				namespace lb = boost::locale::boundary;
				lb::ssegment_index wmap(lb::word, text.begin(), text.end(), loc);
				wmap.rule(lb::word_any);

				for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
					std::string token = boost::locale::to_lower(it->str(), loc);
					std::cout << token << " ";
				}
				std::cout << std::endl;
			} else {
				std::cout << text << std::endl;
			}

			if (vm.count("ngrams")) {
				simdoc doc;
				parser.generate_ngrams(text, doc.ngrams);
				documents.emplace_back(doc);
			}
		} catch (const std::exception &e) {
			std::cerr << f << ": caught exception: " << e.what() << std::endl;
		}
	}

	if (documents.size() >= 2) {
		for (auto fit = documents.begin(), sit = fit + 1; sit != documents.end(); ++fit, ++sit) {
			const std::vector<ngram> &f = fit->ngrams;
			const std::vector<ngram> &s = sit->ngrams;

			printf("ngrams intersect: size: %zd vs %zd: ", f.size(), s.size());
			auto tmp = intersect(f, s);
			printf("intersection-size: %zd: %s", tmp.size(), tmp.size() * 100 / std::min(f.size(), s.size()) > 50 ? "MATCH" : "NO MATCH");
			printf("\n");
		}
	}
}
