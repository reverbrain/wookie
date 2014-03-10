#include "warp/ngram.hpp"

#include <boost/program_options.hpp>

using namespace ioremap;

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Encoding detector options");

	std::string input, detect;
	generic.add_options()
		("help", "This help message")
		("detect-file", bpo::value<std::string>(&detect)->required(), "File with unknown encoding")
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

		if (vm.count("help")) {
			std::cout << generic << std::endl;
			return 0;
		}

		bpo::notify(vm);
	} catch (const std::exception &e) {
		std::cerr << "Invalid options: " << e.what() << "\n" << generic << std::endl;
		return -1;
	}

	if (!files.size()) {
		std::cerr << "You must provide input files for different languages/encodings for statistics\n" <<
			generic << std::endl;
		return -1;
	}

	try {
		warp::ngram::detector d;

		for (auto f = files.begin(); f != files.end(); ++f)
			d.load_file(f->c_str(), f->c_str());

		auto p = d.detect_file(detect.c_str());
		printf("%s: %s\n", detect.c_str(), p.c_str());
	} catch (const std::exception &e) {
		std::cerr << "Caught exception: " << e.what() << std::endl;
		return -1;
	}

	return 0;
}
