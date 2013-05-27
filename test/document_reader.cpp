#include "wookie/storage.hpp"
#include "wookie/document.hpp"

#include <boost/program_options.hpp>

#include <iostream>

using namespace ioremap;

static wookie::document dreader_unpack(elliptics::data_pointer &result)
{
	msgpack::unpacked msg;
	msgpack::unpack(&msg, result.data<char>(), result.size());

	wookie::document doc;
	msg.get().convert(&doc);

	std::cout << doc << std::endl;

	return doc;
}

int main(int argc, char *argv[])
{
	int groups_array[] = {1, 2, 3};
	std::vector<int> groups(groups_array, groups_array + ARRAY_SIZE(groups_array));
	std::string group_string;
	std::string log_file;
	int log_level;
	std::string remote;
	std::string url;
	std::string ns;
	std::string doc_out;
	std::string id;
	bool iterate = false;

	namespace po = boost::program_options;
	po::options_description desc("Options");
	desc.add_options()
		("help", "This help message")
		("log-file", po::value<std::string>(&log_file)->default_value("/dev/stdout"), "Log file")
		("log-level", po::value<int>(&log_level)->default_value(DNET_LOG_ERROR), "Log level")
		("groups", po::value<std::string>(&group_string), "Groups which will host indexes and data, format: 1:2:3")
		("namespace", po::value<std::string>(&ns), "Namespace for urls and indexes")
		("iterate", "Iterate over documents in given collection or just download")
		("url", po::value<std::string>(&url), "Fetch object from storage by URL")
		("id", po::value<std::string>(&id), "Fetch object from storage by ID")
		("document-output", po::value<std::string>(&doc_out), "Put object into this file")
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

	iterate = vm.count("iterate") != 0;

	if (url.size() == 0 && id.size() == 0) {
		std::cerr << "You must provide either URL or ID\n" << desc << std::endl;
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
	st.set_groups(groups);

	try {
		st.add_remote(remote.c_str());
	} catch (const elliptics::error &e) {
		std::cerr << "Could not connect to " << remote << ": " << e.what() << std::endl;
		return -1;
	}

	elliptics::key k(url);
	if (url.size() == 0) {
		struct dnet_id raw;
		dnet_parse_numeric_id(const_cast<char *>(id.c_str()), raw.id);
		raw.group_id = 0;
		raw.type = 0;

		k = elliptics::key(raw);
	}

	if (!iterate) {
		elliptics::data_pointer result = st.read_data(k).get_one().file();
		wookie::document doc = dreader_unpack(result);

		if (doc_out.size() > 0) {
			std::ofstream out(doc_out.c_str(), std::ios::trunc);

			out.write(doc.data.c_str(), doc.data.size());
		}
	} else {
		elliptics::session sess = st.create_session();
		k.transform(sess);

		std::vector<dnet_raw_id> index;
		index.push_back(k.raw_id());

		std::vector<elliptics::find_indexes_result_entry> results;		
		results = st.find(index);

		for (auto r : results) {
			for (auto idx : r.indexes) {
				dreader_unpack(idx.second);
			}
		}
	}

	return 0;
}
