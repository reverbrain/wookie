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

#ifndef WOOKIE_ENGINE_HPP
#define WOOKIE_ENGINE_HPP

#include <swarm/urlfetcher/url_fetcher.hpp>
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

typedef std::function<std::vector<std::string> (const swarm::url_fetcher::response &reply, const std::string &)> parser_functor;
typedef std::function<bool (const swarm::url_fetcher::response &reply, const std::string &)> filter_functor;
typedef std::function<bool (const swarm::url_fetcher::response &reply, const swarm::url &url)> url_filter_functor;
typedef std::function<void (const swarm::url_fetcher::response &reply, const std::string &, document_type type)> process_functor;

filter_functor create_text_filter();
url_filter_functor create_domain_filter(const std::string &url);
url_filter_functor create_port_filter(const std::vector<int> &ports);
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

	void download(const swarm::url &url);
	void found_in_page_cache(const std::string &url, const document &doc);

	int run();

private:
	std::unique_ptr<engine_data> m_data;
};

}}

#endif // WOOKIE_ENGINE_HPP
