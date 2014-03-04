/*
 * Copyright 2014+ Evgeniy Polyakov <zbr@ioremap.net>
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

#ifndef __WOOKIE_DIR_HPP
#define __WOOKIE_DIR_HPP

#include <functional>
#include <sstream>
#include <string>

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>

namespace ioremap { namespace wookie {

	void iterate_directory(const std::string &base, const std::function<bool (const char *, const char *)> &fn) {
		int fd;
		DIR *dir;
		struct dirent64 *d;

		if (base.size() == 0)
			return;

		fd = openat(AT_FDCWD, base.c_str(), O_RDONLY);
		if (fd == -1) {
			std::ostringstream ss;
			ss << "failed to open dir '" << base << "': " << strerror(errno);
			throw std::runtime_error(ss.str());
		}

		dir = fdopendir(fd);

		try {
			while ((d = readdir64(dir)) != NULL) {
				if (d->d_name[0] == '.' && d->d_name[1] == '\0')
					continue;
				if (d->d_name[0] == '.' && d->d_name[1] == '.' && d->d_name[2] == '\0')
					continue;

				if (d->d_type != DT_DIR) {
					if (!fn((base + "/" + d->d_name).c_str(), d->d_name))
						break;
				}
			}
		} catch (const std::exception &e) {
			close(fd);
			throw;
		}

		close(fd);
	}

}} // namespace ioremap::wookie

#endif /* __WOOKIE_DIR_HPP */
