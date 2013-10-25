/*
 * Copyright 2013+ Evgeniy Polyakov <zbr@ioremap.net>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
	std::string doc_out;
	std::string id;
	bool iterate = false;
	variables_map vm;

	wookie::engine engine;

	engine.add_options("Document reader options")
		("iterate", "Iterate over documents in given collection or just download")
		("url", value<std::string>(&url), "Fetch object from storage by URL")
		("id", value<std::string>(&id), "Fetch object from storage by ID")
		("document-output", value<std::string>(&doc_out), "Put object into this file")
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

	iterate = vm.count("iterate") != 0;

	if (url.size() == 0 && id.size() == 0) {
		std::cerr << "You must provide either URL or ID" << std::endl;
		engine.show_help_message(std::cerr);
		return -1;
	}

	elliptics::key k(url);
	if (url.size() == 0) {
		struct dnet_id raw;
		memset(&raw, 0, sizeof(raw));
		dnet_parse_numeric_id(const_cast<char *>(id.c_str()), raw.id);
		raw.group_id = 0;

		k = elliptics::key(raw);
	}

	try {
		if (!iterate) {
			wookie::document doc = engine.get_storage()->read_document(k);
			std::cout << doc << std::endl;

			if (doc_out.size() > 0) {
				std::ofstream out(doc_out.c_str(), std::ios::trunc);

				out.write(doc.data.c_str(), doc.data.size());
			}
		} else {
			elliptics::session sess = engine.get_storage()->create_session();
			k.transform(sess);

			std::vector<dnet_raw_id> index;
			index.push_back(k.raw_id());

			std::vector<elliptics::find_indexes_result_entry> results;		
			results = engine.get_storage()->find(index);

			for (auto r : results) {
				for (auto idx : r.indexes) {
					wookie::document doc = wookie::storage::unpack_document(idx.data);
					std::cout << doc << std::endl;
				}
			}
		}
	} catch (const std::exception &e) {
		std::cerr << "Caught exception: " << e.what() << std::endl;
	}

	return 0;
}
