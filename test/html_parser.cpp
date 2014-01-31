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

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Document parser options");

	std::string enc;
	generic.add_options()
		("help", "This help message")
		("tokenize", "Tokenize text")
		("encoding", bpo::value<std::string>(&enc), "Input directory")
		("ngrams")
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

	xmlInitParser();
	document_parser parser;
	std::vector<document> documents;

	for (auto f : files) {
		try {
			parser.feed(f.c_str());

			std::cout << "================================" << std::endl;
			std::cout << f << std::endl;

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
				document doc(0);
				generate_ngrams(parser, text, doc.ngrams());
				documents.emplace_back(doc);
			}
		} catch (const std::exception &e) {
			std::cerr << f << ": caught exception: " << e.what() << std::endl;
		}
	}
}
