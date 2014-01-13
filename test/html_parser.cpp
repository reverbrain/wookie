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

#include <boost/program_options.hpp>

using namespace ioremap;

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Document parser options");

	generic.add_options()
		("help", "This help message")
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
	wookie::parser parser;

	for (auto f : files) {
		try {
			std::ifstream in(f.c_str());
			if (in.bad())
				continue;

			std::ostringstream ss;
			ss << in.rdbuf();

			parser.parse(ss.str(), "utf8");

			std::cout << "================================" << std::endl;
			std::cout << f << std::endl;
			std::cout << parser.text(" ") << std::endl;
		} catch (const std::exception &e) {
			std::cerr << f << ": caught exception: " << e.what() << std::endl;
		}
	}
}
