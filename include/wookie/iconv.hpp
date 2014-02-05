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

#ifndef __WOOKIE_ICONV_HPP
#define __WOOKIE_ICONV_HPP

#include <string>
#include <stdexcept>
#include <sstream>

#include <iconv.h>
#include <string.h>

namespace ioremap { namespace wookie {

class charset_convert {
	public:
		charset_convert(const char *to, const char *from) {
			m_tmp.resize(128);

			m_iconv = iconv_open(to, from);
			if (m_iconv == (iconv_t)-1) {
				int err = errno;
				std::ostringstream ss;
				ss << "invalid conversion: " <<
					from << " -> " << to << " : " << strerror(err) << " [" << err << "]";
				throw std::runtime_error(ss.str());
			}
		}

		~charset_convert() {
			iconv_close(m_iconv);
		}

		void reset(void) {
			::iconv(m_iconv, NULL, NULL, NULL, NULL);
		}

		std::string convert(const std::string &in) {
			char *src = const_cast<char *>(&in[0]);
			size_t inleft = in.size();

			std::ostringstream out;

			while (inleft > 0) {
				char *dst = const_cast<char *>(&m_tmp[0]);
				size_t outleft = m_tmp.size();

				size_t size = ::iconv(m_iconv, &src, &inleft, &dst, &outleft);

				if (size == (size_t)-1) {
					if (errno == EINVAL)
						break;
					if (errno == E2BIG)
						continue;
					if (errno == EILSEQ) {
						src++;
						inleft--;
						continue;
					}
				}

				out.write(m_tmp.c_str(), m_tmp.size() - outleft);
			}

			return out.str();
		}

	private:
		iconv_t m_iconv;
		std::string m_tmp;
};

}} //namespace ioremap::wookie

#endif /* __WOOKIE_ICONV_HPP */
