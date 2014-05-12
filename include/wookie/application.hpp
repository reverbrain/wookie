#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <cocaine/framework/dispatch.hpp>
#include <cocaine/framework/services/app.hpp>
#include <cocaine/framework/services/storage.hpp>

namespace ioremap { namespace wookie {

typedef std::map<std::string, std::string> meta_info_t;

class processor_t : public cocaine::framework::app_service_t
{
public:
	typedef typename cocaine::framework::service_traits<cocaine::io::app::enqueue>::future_type enqueue_generator;

	processor_t(std::shared_ptr<cocaine::framework::service_connection_t> connection,
		const std::shared_ptr<cocaine::framework::storage_service_t> &storage) :
		app_service_t(connection), m_storage(storage)
	{
	}

	template<class T>
        cocaine::framework::service_traits<cocaine::io::storage::write>::future_type
	push(const std::string& event, const T& chunk) {
            msgpack::sbuffer buffer;
            msgpack::packer<msgpack::sbuffer> packer(buffer);

            cocaine::io::type_traits<T>::pack(packer, chunk);

	    auto result = m_storage->write(this->name(), event, cocaine::io::literal { buffer.data(), buffer.size() });
	    call<cocaine::io::app::enqueue>(event, cocaine::io::literal { buffer.data(), buffer.size() });
	    return result;
        }

private:
	std::shared_ptr<cocaine::framework::storage_service_t> m_storage;
};

}} // namespace ioremap::wookie

#endif // APPLICATION_HPP
