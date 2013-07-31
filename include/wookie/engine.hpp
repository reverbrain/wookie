#ifndef WOOKIE_ENGINE_HPP
#define WOOKIE_ENGINE_HPP

#include <swarm/networkrequest.h>
#include <boost/program_options.hpp>

#include <wookie/document.hpp>

namespace ioremap { namespace wookie {

class engine_data;
class storage;

enum document_type
{
	document_cache,
	document_new,
	document_update
};

typedef std::function<std::vector<std::string> (const swarm::network_reply &reply)> parser_functor;
typedef std::function<bool (const swarm::network_reply &reply)> filter_functor;
typedef std::function<bool (const swarm::network_reply &reply, const std::string &url)> url_filter_functor;
typedef std::function<void (const swarm::network_reply &reply, document_type type)> process_functor;

filter_functor create_text_filter();
url_filter_functor create_domain_filter(const std::string &url);
parser_functor create_href_parser();

class engine
{
public:
	engine();
	engine(const engine &other) = delete;
	~engine();

	engine &operator =(const engine &other) = delete;

	storage *get_storage();

	void add_options(const boost::program_options::options_description &description);
	boost::program_options::options_description_easy_init add_options(const std::string &name);

	void add_parser(const parser_functor &parser);
	void add_filter(const filter_functor &filter);
	void add_url_filter(const url_filter_functor &filter);
	void add_processor(const process_functor &process);
	void add_fallback_processor(const process_functor &process);

	void show_help_message(std::ostream &out);

	int parse_command_line(int argc, char **argv, boost::program_options::variables_map &vm);

	void download(const std::string &url);
	void found_in_page_cache(const std::string &url, const document &doc);

	int run();

private:
	std::unique_ptr<engine_data> m_data;
};

}}

#endif // WOOKIE_ENGINE_HPP
