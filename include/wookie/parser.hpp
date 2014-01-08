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

#ifndef __WOOKIE_PARSER_HPP
#define __WOOKIE_PARSER_HPP

#include <libxml/xmlstring.h>
#include <libxml/HTMLparser.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>
#include <utility>

namespace ioremap { namespace wookie {

class parser {
	public:
		parser() {}

		void parse(const std::string &page, const std::string &encoding) {
			htmlParserCtxtPtr ctx;

			ctx = htmlNewParserCtxt();
			if (!ctx)
				throw std::runtime_error("could not allocate new parser context");

			int options = HTML_PARSE_RECOVER | HTML_PARSE_NOBLANKS |
				HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING |
				HTML_PARSE_NONET | HTML_PARSE_COMPACT;
			if (encoding.size())
				options |= HTML_PARSE_IGNORE_ENC;

			htmlDocPtr doc = htmlCtxtReadMemory(ctx, page.c_str(), page.size(), "url", encoding.c_str(), options);

			if (doc == NULL) {
				htmlFreeParserCtxt(ctx);
				throw std::runtime_error("could not parse page");
			}

			htmlNodePtr root = xmlDocGetRootElement(doc);
			traverse_tree(root);

			xmlFreeDoc(doc);
		}

		const std::vector<std::string> &urls(void) const {
			return m_urls;
		}

		const std::vector<std::string> &tokens(void) const {
			return m_tokens;
		}

		std::string text(void) const {
			std::ostringstream ss;

			std::copy(m_tokens.begin(), m_tokens.end(), std::ostream_iterator<std::string>(ss, "|"));
			return std::move(ss.str());
		}

		void reset(void) {
			m_urls.clear();
			m_tokens.clear();
		}

	private:
		std::vector<std::string> m_urls;
		std::vector<std::string> m_tokens;

		void traverse_tree(htmlNodePtr start) {
			for (htmlNodePtr node = start; node; node = node->next) {
				if (node->type == XML_TEXT_NODE) {
					m_tokens.push_back((char *)node->content);
				}

				if (node->type == XML_ELEMENT_NODE) {
					if (!xmlStrcmp(node->name, (xmlChar *)"a")) {
						xmlChar *data = xmlGetProp(node, (xmlChar *)"href");
						if (data) {
							m_urls.push_back((char *)data);
							break;
						}
					}
				}

				traverse_tree(node->children);
			}
		}

};

}};

#endif /* __WOOKIE_PARSER_HPP */
