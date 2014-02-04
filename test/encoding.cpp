#include "wookie/ngram.hpp"

#include <boost/locale.hpp>
#include <boost/program_options.hpp>

#include <fstream>
#include <sstream>

using namespace ioremap;

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Encoding detector options");

	std::string input;
	generic.add_options()
		("help", "This help message")
		("input-file", bpo::value<std::string>(&input)->required(), "Input file")
		;

	bpo::variables_map vm;

	try {
		bpo::store(bpo::parse_command_line(argc, argv, generic), vm);
		if (vm.count("help")) {
			std::cout << generic << std::endl;
			return 0;
		}

		bpo::notify(vm);
	} catch (const std::exception &e) {
		std::cerr << "Invalid options: " << e.what() << "\n" << generic << std::endl;
		return -1;
	}

	try {
		std::ifstream in(input.c_str());
		std::ostringstream ss;

		ss << in.rdbuf();
		std::string text = ss.str();

		boost::locale::generator gen;
		std::locale loc(gen("en_US.UTF8"));

		namespace lb = boost::locale::boundary;

		lb::ssegment_index wmap(lb::word, text.begin(), text.end(), loc);
		wmap.rule(lb::word_any);

		std::vector<std::string> words;
		for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
			std::string token = boost::locale::to_lower(it->str(), loc);
			words.emplace_back(token);
		}

		std::map<std::string, int> nc = wookie::ngram::load_map(words, 3);
	} catch (const std::exception &e) {
		std::cerr << "Caught exception: " << e.what() << std::endl;
		return -1;
	}

	return 0;
}
