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

#include <buffio.h>
#include <tidy.h>
#include <tidyenum.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

namespace ioremap { namespace wookie {

class parser {
	public:
		parser() {
		}

		~parser() {
		}

		void parse(const std::string &page, const std::string &encoding) {
			reset();

			if (page.size() == 0)
				return;

			TidyBuffer errbuf;
			TidyDoc tdoc = tidyCreate();

			tidyBufInit(&errbuf);

			tidySetCharEncoding(tdoc, encoding.size() ? encoding.c_str() : "raw");
			tidyOptSetBool(tdoc, TidyXhtmlOut, yes);
			tidySetErrorBuffer(tdoc, &errbuf);

			int err = tidyParseString(tdoc, page.c_str());
			if (err < 0) {
				tidyBufFree(&errbuf);
				std::ostringstream ss;
				ss << "Failed to parse page: " << err;
				throw std::runtime_error(ss.str());
			}

			TidyNode html = tidyGetHtml(tdoc);
			traverse_tree(tdoc, html);

			tidyBufFree(&errbuf);
			tidyRelease(tdoc);
		}

		const std::vector<std::string> &urls(void) const {
			return m_urls;
		}

		const std::vector<std::string> &tokens(void) const {
			return m_tokens;
		}

		std::string text(const char *join) const {
			std::ostringstream ss;

			std::copy(m_tokens.begin(), m_tokens.end(), std::ostream_iterator<std::string>(ss, join));
			return std::move(ss.str());
		}

	private:
		std::vector<std::string> m_urls;
		std::vector<std::string> m_tokens;

		void reset(void) {
			m_urls.clear();
			m_tokens.clear();
		}

		void traverse_tree(TidyDoc tdoc, TidyNode tnode) {
			TidyNode child;

			for (child = tidyGetChild(tnode); child; child = tidyGetNext(child)) {
				TidyAttr href;
				if (tidyNodeIsLINK(child) && 0) {
					href = tidyAttrGetHREF(child);
					m_urls.push_back(tidyAttrValue(href));
				}

				if (tidyNodeIsSCRIPT(child) || tidyNodeIsSTYLE(child)) {
					continue;
				}

				if (tidyNodeIsText(child)) {
					if (tidyNodeHasText(tdoc, child)) {
						TidyBuffer buf;
						tidyBufInit(&buf);

						tidyNodeGetText(tdoc, child, &buf);
						std::string text;
						text.assign((char *)buf.bp, buf.size);
						m_tokens.emplace_back(text);

						tidyBufFree(&buf);
					}
				}

				traverse_tree(tdoc, child);
			}
		}

};

}};

#endif /* __WOOKIE_PARSER_HPP */
