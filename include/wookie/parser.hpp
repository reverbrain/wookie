#ifndef __WOOKIE_PARSER_HPP
#define __WOOKIE_PARSER_HPP

#include <libxml/HTMLparser.h>

namespace ioremap { namespace wookie {

class parser {
	public:
		parser() : m_process_flag(0) {}

		void parse(const std::string &page) {
			htmlParserCtxtPtr ctxt;

			htmlSAXHandler handler;
			memset(&handler, 0, sizeof(handler));

			handler.startElement = static_parser_start_element;
			handler.endElement = static_parser_end_element;
			handler.characters = static_parser_characters;

			ctxt = htmlCreatePushParserCtxt(&handler, this, "", 0, "", XML_CHAR_ENCODING_NONE);

			htmlParseChunk(ctxt, page.c_str(), page.size(), 0);
			htmlParseChunk(ctxt, "", 0, 1);

			htmlFreeParserCtxt(ctxt);
		}

		std::vector<std::string> &urls(void) {
			return m_urls;
		}

		const std::string text(void) const {
			return m_text.str();
		}


		void parser_start_element(const xmlChar *tag_name, const xmlChar **attributes) {
			const char *tag = reinterpret_cast<const char*>(tag_name);

			if (strcasecmp(tag, "a") == 0)
				return parse_a(attributes);

			update_process_flag(tag, +1);
		}

		void parser_characters(const xmlChar *ch, int len) {
			if (m_process_flag <= 0)
				return;

			m_text.write(reinterpret_cast<const char *>(ch), len);
		}

		void parser_end_element(const xmlChar *tag_name) {
			const char *tag = reinterpret_cast<const char*>(tag_name);
			update_process_flag(tag, -1);
		}

	private:
		std::vector<std::string> m_urls;
		std::ostringstream m_text;
		int m_process_flag;

		void update_process_flag(const char *tag, int offset) {
			static std::string bad_elements[] = {"script", "style"};
			static std::string good_elements[] = {"body"};

			for (auto && bad : bad_elements) {
				if (strcasecmp(tag, bad.c_str()) == 0) {
					m_process_flag -= offset;
					return;
				}
			}

			for (auto && good : good_elements) {
				if (strcasecmp(tag, good.c_str()) == 0) {
					m_process_flag += offset;
					return;
				}
			}
		}

		static void static_parser_start_element(void *ctx,
						 const xmlChar *tag_name,
						 const xmlChar **attributes) {
			parser *context = reinterpret_cast<parser *>(ctx);
			context->parser_start_element(tag_name, attributes);
		}

		static void static_parser_end_element(void *ctx, const xmlChar *tag_name) {
			parser *context = reinterpret_cast<parser *>(ctx);
			context->parser_end_element(tag_name);
		}

		static void static_parser_characters(void *ctx, const xmlChar *ch, int len) {
			parser *context = reinterpret_cast<parser *>(ctx);
			context->parser_characters(ch, len);
		}

		void parse_a(const xmlChar **attributes) {
			if (!attributes)
				return;

			for (size_t index = 0; attributes[index]; index += 2) {
				const xmlChar *name = attributes[index];
				const xmlChar *value = attributes[index + 1];

				if (!value)
					continue;

				if (strcmp(reinterpret_cast<const char*>(name), "href") == 0) {
					m_urls.push_back(reinterpret_cast<const char*>(value));
				}
			}
		}
};

}};

#endif /* __WOOKIE_PARSER_HPP */
