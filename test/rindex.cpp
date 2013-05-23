#include "wookie/storage.hpp"
#include "wookie/operators.hpp"
#include "wookie/url.hpp"

#include <boost/program_options.hpp>

using namespace ioremap;

int main(int argc, char *argv[])
{
	namespace po = boost::program_options;
	po::options_description desc("Options (required options are marked with *)");

	int groups_array[] = {1, 2, 3};
	std::vector<int> groups(groups_array, groups_array + ARRAY_SIZE(groups_array));
	std::string group_string;
	std::string log_file;
	int log_level;
	std::string remote;
	std::string url;
	std::string ns;
	std::string find;
	int tnum;

	desc.add_options()
		("help", "This help message")
		("log-file", po::value<std::string>(&log_file)->default_value("/dev/stdout"), "Log file")
		("log-level", po::value<int>(&log_level)->default_value(DNET_LOG_ERROR), "Log level")
		("groups", po::value<std::string>(&group_string), "Groups which will host indexes and data, format: 1:2:3")
		("url", po::value<std::string>(&url), "Url to download")
		("uthreads", po::value<int>(&tnum)->default_value(3), "Number of URL downloading and processing threads")
		("namespace", po::value<std::string>(&ns), "Namespace for urls and indexes")
		("find", po::value<std::string>(&find), "Find pages containing all tokens (space separated)")
		("remote", po::value<std::string>(&remote),
		 	"Remote node to connect, format: address:port:family (IPv4 - 2, IPv6 - 10)")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help") || !vm.count("remote")) {
		std::cerr << desc << std::endl;
		return -1;
	}

	if (!vm.count("find") && !vm.count("url")) {
		std::cerr << "You must provide either URL or FIND option" << std::endl << desc << std::endl;
		return -1;
	}

	if (group_string.size()) {
		struct digitizer {
			int operator() (const std::string &str) {
				return atoi(str.c_str());
			}
		};

		groups.clear();

		std::istringstream iss(group_string);
		std::transform(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(),
				std::back_inserter<std::vector<int>>(groups), digitizer());
	}

	elliptics::file_logger log(log_file.c_str(), log_level);
	wookie::storage st(log, ns);

	try {
		st.add_remote(remote.c_str());
	} catch (const elliptics::error &e) {
		std::cerr << "Could not connect to " << remote << ": " << e.what() << std::endl;
		return -1;
	}

	boost::locale::generator gen;
	std::locale loc=gen("en_US.UTF8"); 
	std::locale::global(loc);

	st.set_groups(groups);

	try {
		if (find.size()) {
			ioremap::wookie::operators op(st);
			std::vector<dnet_raw_id> results;
			results = op.find(find);

			std::cout << "Found documents: " << std::endl;
			for (auto && r : results) {
				std::cout << r << std::endl;
			}
		} else {
			wookie::dmanager downloader(tnum);
			wookie::url_processor rtest(url, wookie::url::within_domain, st, downloader);
			downloader.start();
		}
	} catch (const std::exception &e) {
		std::cerr << "main thread exception: " << e.what() << std::endl;
	}
	return 0;

}
