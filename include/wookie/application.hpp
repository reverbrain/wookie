#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <cocaine/framework/dispatch.hpp>
#include <cocaine/framework/services/app.hpp>
#include <cocaine/framework/services/storage.hpp>

namespace ioremap { namespace wookie {

/*!
 * \brief The meta_info_t class provides common API for transfering
 * meta information between pipeline applications.
 */
class meta_info_t
{
public:
	meta_info_t()
	{
	}

	meta_info_t(meta_info_t &&other) : m_data(std::move(other.m_data))
	{
	}

	meta_info_t &operator =(meta_info_t &&other)
	{
		m_data = std::move(other.m_data);
		return *this;
	}

	meta_info_t(const meta_info_t &other) = delete;
	meta_info_t &operator =(const meta_info_t &other) = delete;

	/*!
	 * \brief Set original body of the document
	 */
	void set_body(const std::string &body)
	{
		set_value<std::string>("body", body);
	}

	/*!
	 * \brief Returns original body of the document
	 */
	std::string body() const
	{
		return value<std::string>("body");
	}

	/*!
	 * \brief Set url of the document
	 */
	void set_url(const std::string &url)
	{
		set_value<std::string>("url", url);
	}

	/*!
	 * \brief Returns url of the document
	 */
	std::string url() const
	{
		return value<std::string>("url");
	}

	/*!
	 * \brief Set custom \a value property by the \a name.
	 *
	 * \a value must be convertable to raw data by cocaine::io::type_traits
	 */
	template <typename T>
	void set_value(const std::string &name, const T &value)
	{
		msgpack::sbuffer buffer;
                msgpack::packer<msgpack::sbuffer> packer(buffer);

                cocaine::io::type_traits<T>::pack(packer, value);

		m_data.insert(std::make_pair(name, std::string(buffer.data(), buffer.size())));
	}

	/*!
	 * \brief Returns custom property by the \a name.
	 *
	 * Return type must be convertable to raw data by cocaine::io::type_traits
	 */
	template <typename T>
	T value(const std::string &name) const
	{
		auto it = m_data.find(name);
		if (it == m_data.end())
			return T();

		T result;
		msgpack::unpacked msg;
	        msgpack::unpack(&msg, it->second.data(), it->second.size());
	        cocaine::io::type_traits<T>::unpack(msg.get(), result);
		return result;
	}

private:
	template <typename T>
	friend struct cocaine::io::type_traits;
	std::map<std::string, std::string> m_data;
};

}}

namespace cocaine { namespace io {

/*!
 * \brief Add support of meta_info_t to cocaine::io::type_traits.
 */
template<>
struct type_traits<ioremap::wookie::meta_info_t> {
	template<class Stream>
	static inline
	void
	pack(msgpack::packer<Stream> &packer, const ioremap::wookie::meta_info_t &source) {
		packer << source.m_data;
	}

	static inline
	void
	unpack(const msgpack::object &unpacked, ioremap::wookie::meta_info_t &target) {
		unpacked >> target.m_data;
	}
};

} }

namespace ioremap { namespace wookie {

/*!
 * \brief The processor_t class provides API to push meta_info_t to next application in pipeline
 */
class processor_t : public cocaine::framework::app_service_t
{
public:
	typedef typename cocaine::framework::service_traits<cocaine::io::app::enqueue>::future_type enqueue_generator;

	processor_t(std::shared_ptr<cocaine::framework::service_connection_t> connection,
		const std::shared_ptr<cocaine::framework::storage_service_t> &storage) :
		app_service_t(connection), m_storage(storage), m_tags(1, "documents")
	{
	}

	/*!
	 * \brief Sends \a chunk to next processor
	 *
	 * In addition it writes data to secondary index of next application, so it may be
	 * restored after failure without data loose
	 */
	template<class T>
	cocaine::framework::service_traits<cocaine::io::storage::write>::future_type
	push(const std::string& url, const T& chunk) {
		msgpack::sbuffer buffer;
		msgpack::packer<msgpack::sbuffer> packer(buffer);

		cocaine::io::type_traits<T>::pack(packer, chunk);

		cocaine::io::literal data = { buffer.data(), buffer.size() };

		auto result = m_storage->write(this->name(), url, data, m_tags);
		push_raw(data);
		return result;
 	}

	/*!
	 * \internal
	 * \brief Pushes data to next worker without adding url to secondary index
	 */
	cocaine::framework::service_traits<cocaine::io::app::enqueue>::future_type
	push_raw(const cocaine::io::literal &chunk) {
		return call<cocaine::io::app::enqueue>("process", chunk);
 	}

private:
	std::shared_ptr<cocaine::framework::storage_service_t> m_storage;
	std::vector<std::string> m_tags;
};

/*!
 * \brief The meta_info_pipeline_t class provides common API for working with pipeline
 *
 * It makes connections to logger and storage in addition to next application in pipeline
 */
class meta_info_pipeline_t
{
public:
	/*!
	 * \brief Constructs meta_info_pipeline_t with dispatch \a d, \a current application name and \a next application name
	 */
	meta_info_pipeline_t(cocaine::framework::dispatch_t &d, const std::string &current, const std::string &next) : m_tags(1, "documents")
	{
		m_current = current;
		m_logger = d.service_manager()->get_system_logger();
		m_storage = d.service_manager()->get_service<cocaine::framework::storage_service_t>("storage");

		// Last processor in pipeline won't have next processor
		if (!next.empty()) {
			m_next = d.service_manager()->get_service<processor_t>(next, m_storage);
		}
	}

	const std::shared_ptr<processor_t> &next() const
	{
		return m_next;
	}

	const std::shared_ptr<cocaine::framework::storage_service_t> &storage() const
	{
		return m_storage;
	}

	const std::shared_ptr<cocaine::framework::logger_t> &logger() const
	{
		return m_logger;
	}

	/*!
	 * \brief Pushes meta \a info to next application, \a that is shared pointer to your application
	 *
	 * Once request will be finally processed by next application this method will automatically
	 * send reply to reply_stream by calling finish method.
	 *
	 * \sa finish
	 */
	template <typename T>
	void push(const T &that, const meta_info_t &info)
	{
		std::string url = info.url();

		next()->push(url, info).then(
			[this, that, url] (cocaine::framework::generator<void> &future) {
			try {
				future.next();
			} catch (std::exception &e) {
				COCAINE_LOG_ERROR(logger(), "Failed to send to next processor, url: %s, error: %s", url, e.what());

				that->response()->error(100, "Failed to send to next processor");
				return;
			}

			finish(that, url);
		});
	}

	/*!
	 * \brief Removes \a url from this application's seconday index, \a that is shared pointer to your application
	 *
	 * Sends reply to reply_stream.
	 *
	 * \attention This method is automatically called by push method.
	 *
	 * \sa push
	 */
	template <typename T>
	void finish(const T &that, const std::string &url)
	{
		storage()->remove(m_current, url).then(
			[this, that, url] (cocaine::framework::generator<void> &future) {
			try {
				future.next();
			} catch (std::exception &e) {
				COCAINE_LOG_ERROR(logger(), "Failed to remove itself from the list, url: %s, error: %s", url, e.what());
			}

			that->response()->write(cocaine::io::literal { "ok", 2 });
			that->response()->close();
		});
	}

	/*!
	 * \internal
	 */
	template <typename T>
	void restore_states(const T &that)
	{
		storage()->find(m_current, m_tags).then([this, that] (cocaine::framework::generator<std::vector<std::string>> &future) {
			try {
				for (const std::string &url : future.next()) {
					that->process(url);
				}
			} catch (std::exception &e) {
				COCAINE_LOG_ERROR(logger(), "Failed to retrieve the list of actions to restore, error: %s", e.what());
			}
		});
	}

private:
	std::string m_current;
	std::shared_ptr<processor_t> m_next;
	std::shared_ptr<cocaine::framework::storage_service_t> m_storage;
	std::shared_ptr<cocaine::framework::logger_t> m_logger;
	std::vector<std::string> m_tags;
};

template<class App>
class pipeline_process_handler : public cocaine::framework::handler<App>
{
public:
	pipeline_process_handler(App &a) : cocaine::framework::handler<App>(a), m_pipeline(a.pipeline())
        {
        }

	virtual void on_request(meta_info_t &&info) = 0;

	void on_chunk(const char *chunk, size_t size)
	{
		on_request(cocaine::framework::unpack<meta_info_t>(chunk, size));
	}

	meta_info_pipeline_t &pipeline()
	{
		return m_pipeline;
	}

protected:
	meta_info_pipeline_t m_pipeline;
};

}} // namespace ioremap::wookie

#endif // APPLICATION_HPP
