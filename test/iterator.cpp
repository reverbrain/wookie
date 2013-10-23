#include "wookie/storage.hpp"
#include "wookie/engine.hpp"
#include "wookie/document.hpp"

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>

#include "warp/lex.hpp"

using namespace ioremap;

int main(int argc, char *argv[])
{
	using namespace boost::program_options;

	std::string url;
	variables_map vm;
	bool text = false;
	std::string msgin, gram;

	wookie::engine engine;

	engine.add_options("Document reader options")
		("url", value<std::string>(&url), "Fetch object from storage by URL")
		("text", "Iterate over text objects, not HTML")
		("msgpack-input", value<std::string>(&msgin), "Path to Zaliznyak dictionary in msgpacked format")
		("grammar", value<std::string>(&gram), "Grammar string suitable for ioremap::warp::parser, space separates single word descriptions")
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

		if (!msgin.size() || !gram.size()) {
			for (const auto &b : bres) {
				wookie::document doc = wookie::storage::unpack_document(b.file());
				std::cout << doc << std::endl;
			}

			return 0;
		}

		boost::locale::generator gen;
		std::locale loc = gen("en_US.UTF8");

		ioremap::warp::lex l(loc);
		l.load(msgin);

		std::vector<std::string> tokens;
		std::istringstream iss(gram);
		std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), std::back_inserter<std::vector<std::string>>(tokens));

		std::vector<ioremap::warp::parsed_word::feature_mask> vgram = l.generate(tokens);

		for (const auto &b : bres) {
			wookie::document doc = wookie::storage::unpack_document(b.file());

			std::vector<std::string> sentences;
			boost::split(sentences, doc.data, boost::is_any_of("|"));

			std::cout << doc << ", sentences: " << sentences.size() << std::endl;

			for (auto & sent : sentences) {
				lb::ssegment_index wmap(lb::word, sent.begin(), sent.end(), loc);
				wmap.rule(lb::word_any);

				std::vector<std::string> words;
				for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
					words.push_back(boost::locale::to_lower(it->str(), loc));
				}

				for (auto pos : l.grammar(vgram, words)) {
					for (size_t i = 0; i < vgram.size(); ++i) {
						std::cout << words[i + pos] << " ";
					}

					std::cout << std::endl;
					for (size_t i = 0; i < vgram.size(); ++i) {
						std::string & word = words[i + pos];

						auto lres = l.lookup(word);

						std::cout << "word: " << word << ": ";
						for (auto v : lres) {
							std::cout << v.ending_len <<
								"," << std::hex << "0x" << v.features <<
								" " << std::dec;
						}
						std::cout << std::endl;
					}
					std::cout << std::endl;
				}
			}
		}

	} catch (const std::exception &e) {
		std::cerr << "Caught exception: " << e.what() << std::endl;
	}

	return 0;
}

