#include "wookie/storage.hpp"
#include "wookie/engine.hpp"
#include "wookie/document.hpp"

#include <boost/program_options.hpp>

#include <iostream>

using namespace ioremap;

int main(int argc, char *argv[])
{
	using namespace boost::program_options;

	std::string url;
	variables_map vm;
	bool text = false;

	wookie::engine engine;

	engine.add_options("Document reader options")
		("url", value<std::string>(&url), "Fetch object from storage by URL")
		("text", "Iterate over text objects, not HTML")
	;

	try {
		int err = engine.parse_command_line(argc, argv, vm);
		if (err < 0)
			return err;
	} catch (const std::exception &e) {
		std::cerr << "Command line parsing failed: " << e.what() << std::endl;
		engine.show_help_message(std::cerr);
		return -1;
	}

	if (url.size() == 0) {
		std::cerr << "You must provide either URL" << std::endl;
		engine.show_help_message(std::cerr);
		return -1;
	}

	text = vm.count("text") != 0;
	elliptics::key k(url);

	try {
		elliptics::session sess = engine.get_storage()->create_session();
		k.transform(sess);

		std::vector<dnet_raw_id> index;
		index.push_back(k.raw_id());

		std::vector<elliptics::find_indexes_result_entry> results;		
		results = engine.get_storage()->find(index);

		elliptics::session data_sess = engine.get_storage()->create_session();
		if (text)
			data_sess.set_namespace("text", 4);

		std::vector<std::string> urls;
		for (auto r : results) {
			for (auto idx : r.indexes) {
				wookie::document doc = wookie::storage::unpack_document(idx.data);
				urls.push_back(doc.key);
			}
		}

		elliptics::async_read_result bres = data_sess.bulk_read(urls);
		for (const auto &b : bres) {
			wookie::document doc = wookie::storage::unpack_document(b.file());
			std::cout << doc.data << std::endl;
		}
	} catch (const std::exception &e) {
		std::cerr << "Caught exception: " << e.what() << std::endl;
	}

	return 0;
}

