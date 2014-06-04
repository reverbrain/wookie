#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <grape/elliptics_client_state.hpp>
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
 * \brief The meta_info_pipeline_t class provides common API for working with pipeline
 *
 * It makes connections to logger and queue of current and next applications in pipeline
 */
class meta_info_pipeline_t
{
public:
	/*!
	 * \brief Constructs meta_info_pipeline_t with dispatch \a d, \a current application name and \a next application name
	 */
	meta_info_pipeline_t(cocaine::framework::dispatch_t &d, const std::string &current, const std::string &next) : m_tags(1, "documents")
	{
		(void) current;
		(void) next;

		rapidjson::Document doc;
		m_state = elliptics_client_state::create("config.json", doc);

		if (doc.HasMember("next")) {
			rapidjson::Value &next = doc["next"];
			if (next.IsString()) {
				m_next_name.assign(next.GetString(), next.GetString() + next.GetStringLength());
				m_next_name += "-queue@push";
			}
		}

		if (!doc.HasMember("current") || !doc["current"].IsString()) {
			throw std::runtime_error("Field 'current' is missed or is not a string");
		}
		m_current_name = doc["current"].GetString();
		m_current_name += "-queue@ack";
		m_logger = d.service_manager()->get_system_logger();
		m_storage = d.service_manager()->get_service<cocaine::framework::storage_service_t>("storage");
	}

	meta_info_pipeline_t(const meta_info_pipeline_t &) = delete;
	meta_info_pipeline_t &operator =(const meta_info_pipeline_t &) = delete;

	const std::shared_ptr<cocaine::framework::storage_service_t> &storage() const
	{
		return m_storage;
	}

	const std::shared_ptr<cocaine::framework::logger_t> &logger() const
	{
		return m_logger;
	}

	template <typename Method>
	void push(const std::string &url, const meta_info_t &info, Method method)
	{
		push_internal(url, info).connect([this, url, method] (const elliptics::sync_exec_result &, const elliptics::error_info &error) {
			if (error) {
				COCAINE_LOG_ERROR(logger(), "Failed to send to next processor, url: %s, error: %s", url, error.message());
			}

			method(!error);
		});
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
		const std::string url = info.url();

		push_internal(url, info).connect([this, that, url] (const elliptics::sync_exec_result &, const elliptics::error_info &error) {
			if (error) {
				COCAINE_LOG_ERROR(logger(), "Failed to sent to next processor, url: %s, error: %s", url, error.message());

				that->response()->error(100, "Failed to sent to next processor");
				return;
			}

			finish(that, url);
		});
	}

	/*!
	 * \brief Removes \a url from this application's queue, \a that is shared pointer to your application
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
		elliptics::session sess = m_state.create_session();
		sess.exec(that->context(), m_current_name, elliptics::data_pointer()).connect(
			[this, url, that] (const elliptics::sync_exec_result &, const elliptics::error_info &error) {
			if (error) {
				COCAINE_LOG_ERROR(logger(), "Failed to sent to next processor, url: %s, error: %s", url, error.message());
			}
		});

		that->response()->write(cocaine::io::literal { "ok", 2 });
		that->response()->close();
	}

private:
	elliptics::async_exec_result exec(const std::string &url, const std::string &event, const elliptics::argument_data &data)
	{
		elliptics::session sess = m_state.create_session();
		elliptics::key id = url;
		sess.transform(id);
		dnet_id tmp_id = id.id();

		return sess.exec(&tmp_id, event, data);
	}

	elliptics::async_exec_result push_internal(const std::string &url, const meta_info_t &info)
	{
		msgpack::sbuffer buffer;
		msgpack::packer<msgpack::sbuffer> packer(buffer);
		cocaine::io::type_traits<meta_info_t>::pack(packer, info);

		COCAINE_LOG_INFO(logger(), "Send to next processor, url: %s, event: %s", url, m_next_name);

		elliptics::session sess = m_state.create_session();
		elliptics::key id = url;
		sess.transform(id);
		dnet_id tmp_id = id.id();

		auto data = elliptics::data_pointer::from_raw(buffer.data(), buffer.size());
		return sess.exec(&tmp_id, m_next_name, data);
	}

	elliptics_client_state m_state;
	std::string m_current_name;
	std::string m_next_name;
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
		elliptics::error_info error;
		m_context = elliptics::exec_context::parse(elliptics::data_pointer::copy(chunk, size), &error);
		if (error) {
			error.throw_error();
		}

		const auto data = m_context.data();

		COCAINE_LOG_INFO(this->parent().pipeline().logger(), "on_chunk: %s, data size: %ull",
			m_context.event(), static_cast<unsigned long long>(data.size()));

		on_request(cocaine::framework::unpack<meta_info_t>(data.data<char>(), data.size()));
	}

	meta_info_pipeline_t &pipeline()
	{
		return m_pipeline;
	}

	const elliptics::exec_context &context() const
	{
		return m_context;
	}

protected:
	meta_info_pipeline_t &m_pipeline;
	elliptics::exec_context m_context;
};

}} // namespace ioremap::wookie

#endif // APPLICATION_HPP
