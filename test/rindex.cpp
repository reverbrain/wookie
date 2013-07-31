#include "wookie/storage.hpp"
#include "wookie/operators.hpp"
#include "wookie/url.hpp"

#include "wookie/engine.hpp"
#include "wookie/split.hpp"
#include "wookie/index_data.hpp"

#include <boost/program_options.hpp>

using namespace ioremap;

struct rindex_processor
{
	wookie::engine &engine;
	std::string base;
	bool fallback;
	wookie::split splitter;

	rindex_processor(wookie::engine &engine, const std::string &base, bool fallback)
		: engine(engine), base(base), fallback(fallback)
	{
	}

	void process(const std::string &url, const std::string &content, const dnet_time &ts, const std::string &base_index)
	{
	    std::vector<std::string> ids;
	    std::vector<elliptics::data_pointer> objs;

	    // each reverse index contains wookie::index_data object for every key stored
	    if (content.size()) {
		std::vector<std::string> tokens;
		wookie::mpos_t pos = splitter.feed(content, tokens);

		for (auto && p : pos) {
		    ids.emplace_back(std::move(p.first));
		    objs.emplace_back(wookie::index_data(ts, p.first, p.second).convert());
		}
	    }

	    // base index contains wookie::document object for every key stored
	    if (base_index.size()) {
		wookie::document doc;
		doc.ts = ts;
		doc.key = url;
		doc.data = url;

		msgpack::sbuffer doc_buffer;
		msgpack::pack(&doc_buffer, doc);

		ids.push_back(base_index);
		objs.emplace_back(elliptics::data_pointer::copy(doc_buffer.data(), doc_buffer.size()));
	    }

	    if (ids.size()) {
		std::cout << "Updating ... " << url << std::endl;
		engine.get_storage()->create_session().set_indexes(url, ids, objs).wait();
	    }
	}

	void process_text(const ioremap::swarm::network_reply &reply, wookie::document_type) {
		struct dnet_time ts;
		dnet_current_time(&ts);

		wookie::parser p;
		if (!fallback)
			p.parse(reply.get_data());

		try {
			process(reply.get_url(), p.text(), ts, base + ".collection");
		} catch (const std::exception &e) {
			std::cerr << reply.get_url() << ": index processing exception: " << e.what() << std::endl;
			engine.download(reply.get_request().get_url());
		}
	}

	static wookie::process_functor create(wookie::engine &engine, const std::string &url, bool fallback)
	{
		ioremap::swarm::network_url base_url;
		std::string base;

		if (!base_url.set_base(url))
			ioremap::elliptics::throw_error(-EINVAL, "Invalid URL '%s': set-base failed", url.c_str());

		base = base_url.host();
		if (base.empty())
			ioremap::elliptics::throw_error(-EINVAL, "Invalid URL '%s': base is empty", url.c_str());

		return std::bind(&rindex_processor::process_text,
			std::make_shared<rindex_processor>(engine, base, fallback),
			std::placeholders::_1,
			std::placeholders::_2);
	}
};

int main(int argc, char *argv[])
{
	using namespace boost::program_options;

	std::string find;
	std::string url;
	variables_map vm;
	wookie::engine engine;

	engine.add_options("RIndex options")
		("find", value<std::string>(&find), "Find pages containing all tokens (space separated)")
		("url", value<std::string>(&url), "Url to download")
	;

	engine.parse_command_line(argc, argv, vm);

	if (!vm.count("find") && !vm.count("url")) {
		std::cerr << "You must provide either URL or FIND option" << std::endl;
		engine.show_help_message(std::cerr);
		return -1;
	}

	try {
		if (find.size()) {
			ioremap::wookie::operators op(*engine.get_storage());
			std::vector<dnet_raw_id> results;
			results = op.find(find);

			std::cout << "Found documents: " << std::endl;
			for (auto && r : results) {
				std::cout << r << std::endl;
			}
		} else {
			engine.add_filter(wookie::create_text_filter());
			engine.add_url_filter(wookie::create_domain_filter(url));
			engine.add_parser(wookie::create_href_parser());
			engine.add_processor(rindex_processor::create(engine, url, false));
			engine.add_fallback_processor(rindex_processor::create(engine, url, true));

			engine.download(url);

			return engine.run();
		}
	} catch (const std::exception &e) {
		std::cerr << "main thread exception: " << e.what() << std::endl;
	}
	return 0;

}
