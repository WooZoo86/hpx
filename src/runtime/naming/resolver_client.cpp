//  Copyright (c) 2007-2009 Hartmut Kaiser
//
//  Parts of this code were taken from the Boost.Asio library
//  Copyright (c) 2003-2007 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// 
//  Distributed under the Boost Software License, Version 1.0. (See accompanying 
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <vector>

#include <hpx/hpx_fwd.hpp>

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/cstdint.hpp>
#include <boost/assert.hpp>
#include <boost/throw_exception.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/integer/endian.hpp>

#include <hpx/config.hpp>
#include <hpx/exception.hpp>
#include <hpx/runtime/naming/locality.hpp>
#include <hpx/runtime/naming/address.hpp>
#include <hpx/runtime/naming/resolver_client.hpp>
#include <hpx/runtime/naming/server/reply.hpp>
#include <hpx/runtime/naming/server/request.hpp>
#include <hpx/util/portable_binary_oarchive.hpp>
#include <hpx/util/portable_binary_iarchive.hpp>
#include <hpx/util/container_device.hpp>
#include <hpx/util/asio_util.hpp>
#include <hpx/util/util.hpp>
#include <hpx/util/block_profiler.hpp>

#define HPX_USE_AGAS_CACHE 1

///////////////////////////////////////////////////////////////////////////////
namespace hpx { namespace naming 
{
    resolver_client::resolver_client(util::io_service_pool& io_service_pool, 
            locality l, bool isconsole) 
      : there_(l), io_service_pool_(io_service_pool), 
        connection_cache_(HPX_MAX_AGAS_CONNECTION_CACHE_SIZE, "[AGAS] "),
        agas_cache_(HPX_INITIAL_AGAS_CACHE_SIZE), isconsole_(isconsole)
    {
        // start the io service pool
        io_service_pool.run(false);
    }

    ///////////////////////////////////////////////////////////////////////////
    void resolver_client::get_prefix(locality const& l, id_type& prefix, 
        error_code& ec) const
    {
        // send request
        server::request req (server::command_getprefix, l, isconsole_);
        server::reply rep;
        execute(req, rep);

        hpx::error s = (hpx::error) rep.get_status();
        ec = make_error_code(s, rep.get_error(), hpx::rethrow);
        if (s != success && s != repeated_request) 
            return;

        prefix = rep.get_prefix();
    }

    bool resolver_client::get_prefix(locality const& l, id_type& prefix) const
    {
        error_code ec;
        get_prefix(l, prefix, ec);
        if (ec.value() != success && ec.value() != repeated_request)
        {
            HPX_RETHROW_EXCEPTION(ec.value(), "resolver_client::get_prefix", 
                ec.get_message());
            return false;
        }
        return ec.value() == success;
    }

    ///////////////////////////////////////////////////////////////////////////
    bool resolver_client::get_console_prefix(id_type& prefix) const
    {
        // send request
        server::request req (server::command_getconsoleprefix);
        server::reply rep;
        execute(req, rep);

        hpx::error s = (hpx::error) rep.get_status();
        if (s != success && s != no_registered_console)
        {
            HPX_RETHROW_EXCEPTION(s, "resolver_client::get_console_prefix", 
                rep.get_error());
            return false;
        }
        prefix = rep.get_prefix();
        return s == success;
    }

    ///////////////////////////////////////////////////////////////////////////
    bool resolver_client::get_prefixes(std::vector<id_type>& prefixes,
        components::component_type type) const
    {
        // send request
        server::request req (server::command_getprefixes, type);
        server::reply rep;
        execute(req, rep);

        hpx::error s = (hpx::error) rep.get_status();
        if (s != success && s != no_success)
        {
            HPX_RETHROW_EXCEPTION(s, "resolver_client::get_prefixes", 
                rep.get_error());
            return false;
        }
        typedef std::vector<boost::uint32_t>::const_iterator iterator;
        iterator end = rep.get_prefixes().end();
        for (iterator it = rep.get_prefixes().begin(); it != end; ++it)
            prefixes.push_back(get_id_from_prefix(*it));

        return s == success;
    }

    ///////////////////////////////////////////////////////////////////////////
    components::component_type resolver_client::get_component_id(
        std::string const& componentname) const
    {
        // send request
        server::request req (server::command_get_component_id, componentname);
        server::reply rep;
        execute(req, rep);

        hpx::error s = (hpx::error) rep.get_status();
        if (s != success && s != no_success)
        {
            HPX_RETHROW_EXCEPTION(s, "resolver_client::get_component_id", 
                rep.get_error());
            return components::component_invalid;
        }
        return rep.get_component_id();
    }

    ///////////////////////////////////////////////////////////////////////////
    components::component_type resolver_client::register_factory(
        id_type const& prefix, std::string const& name) const
    {
        // send request
        server::request req (server::command_register_factory, name, prefix);
        server::reply rep;
        execute(req, rep);

        hpx::error s = (hpx::error) rep.get_status();
        if (s != success && s != no_success)
        {
            HPX_RETHROW_EXCEPTION(s, "resolver_client::register_factory", 
                rep.get_error());
            return components::component_invalid;
        }
        return rep.get_component_id();
    }

    ///////////////////////////////////////////////////////////////////////////
    bool resolver_client::get_id_range(locality const& l, std::size_t count, 
        id_type& lower_bound, id_type& upper_bound) const
    {
        // send request
        server::request req (server::command_getidrange, l, count);
        server::reply rep;
        execute(req, rep);

        hpx::error s = (hpx::error) rep.get_status();
        if (s != success && s != repeated_request)
        {
            HPX_RETHROW_EXCEPTION(s, "resolver_client::get_id_range", 
                rep.get_error());
            return false;
        }
        lower_bound = rep.get_lower_bound();
        upper_bound = rep.get_upper_bound();
        return s == success;
    }

    ///////////////////////////////////////////////////////////////////////////
    bool resolver_client::bind_range(id_type const& id, std::size_t count, 
        address const& addr, std::ptrdiff_t offset) const
    {
        // send request
        server::request req (server::command_bind_range, id, count, addr, offset);
        server::reply rep;
        execute(req, rep);

        hpx::error s = (hpx::error) rep.get_status();
        if (s != success && s != no_success)
        {
            HPX_RETHROW_EXCEPTION(s, "resolver_client::bind_range", 
                rep.get_error());
            return false;
        }

#if defined(HPX_USE_AGAS_CACHE)
        // add the new range to the local cache
        boost::mutex::scoped_lock lock(mtx_);
        cache_key k(id, count);
        agas_cache_.insert(k, std::make_pair(addr, offset));
#endif

        return s == success;
    }

    ///////////////////////////////////////////////////////////////////////////
    struct erase_policy
    {
        erase_policy(id_type const& id, std::size_t count)
          : entry_(id, count)
        {}

        typedef 
            std::pair<resolver_client::cache_key, resolver_client::entry_type>
        cache_storage_entry_type;

        bool operator()(cache_storage_entry_type const& p) const
        {
            return p.first == entry_;
        }

        resolver_client::cache_key entry_;
    };

    void resolver_client::unbind_range(id_type const& id, std::size_t count, 
        address& addr, error_code& ec) const
    {
        // send request
        server::request req (server::command_unbind_range, id, count);
        server::reply rep;
        execute(req, rep);

        hpx::error s = (hpx::error) rep.get_status();
        ec = make_error_code(s, rep.get_error(), hpx::rethrow);
        if (s != success && s != no_success)
            return;

        addr = rep.get_address();

#if defined(HPX_USE_AGAS_CACHE)
        // remove this entry from the cache
        boost::mutex::scoped_lock lock(mtx_);
        erase_policy ep(id, count);
        agas_cache_.erase(ep);
#endif
    }

    bool resolver_client::unbind_range(id_type const& id, std::size_t count, 
        address& addr) const
    {
        error_code ec;
        unbind_range(id, count, addr, ec);
        if (ec.value() != success && ec.value() != no_success)
        {
            HPX_THROW_EXCEPTION(ec.value(), 
                "resolver_client::unbind_range", ec.get_message());
        }
        return ec.value() == success;
    }

    ///////////////////////////////////////////////////////////////////////////
    struct resolve_tag {};
    bool resolver_client::resolve(id_type const& id, address& addr) const
    {
        util::block_profiler<resolve_tag> bp("resolver_client::resolve");

#if defined(HPX_USE_AGAS_CACHE)
        // first look up the requested item in the cache
        cache_key k(id);
        {
            boost::mutex::scoped_lock lock(mtx_);
            cache_key realkey;
            cache_type::entry_type e;
            if (agas_cache_.get_entry(k, realkey, e)) {
                // This entry is currently in the cache
                BOOST_ASSERT(id.get_msb() == realkey.id_.get_msb());
                addr = e.get().first;
                addr.address_ += (id.get_lsb() - realkey.id_.get_lsb()) * e.get().second;
                return true;
            }
        }
#endif

        // send request
        server::request req (server::command_resolve, id);
        server::reply rep;
        execute(req, rep);

        hpx::error s = (hpx::error) rep.get_status();
        if (s != success && s != no_success)
        {
            HPX_RETHROW_EXCEPTION(s, "resolver_client::resolve", 
                rep.get_error());
            return false;
        }

        addr = rep.get_address();

#if defined(HPX_USE_AGAS_CACHE)
        // add the requested item to the cache
        boost::mutex::scoped_lock lock(mtx_);
        agas_cache_.insert(k, std::make_pair(addr, 1));
#endif

        return s == success;
    }

    ///////////////////////////////////////////////////////////////////////////
    bool resolver_client::registerid(std::string const& ns_name, 
        id_type const& id) const
    {
        // send request
        server::request req (server::command_registerid, ns_name, id);
        server::reply rep;
        execute(req, rep);

        hpx::error s = (hpx::error) rep.get_status();
        if (s != success && s != no_success)
        {
            HPX_RETHROW_EXCEPTION(s, "resolver_client::registerid", 
                rep.get_error());
            return false;
        }
        return s == success;
    }

    ///////////////////////////////////////////////////////////////////////////
    bool resolver_client::unregisterid(std::string const& ns_name) const
    {
        // send request
        server::request req (server::command_unregisterid, ns_name);
        server::reply rep;
        execute(req, rep);

        hpx::error s = (hpx::error) rep.get_status();
        if (s != success && s != no_success)
        {
            HPX_RETHROW_EXCEPTION(s, "resolver_client::unregisterid", 
                rep.get_error());
            return false;
        }
        return s == success;
    }

    ///////////////////////////////////////////////////////////////////////////
    bool resolver_client::queryid(std::string const& ns_name, id_type& id) const
    {
        // send request
        server::request req (server::command_queryid, ns_name);
        server::reply rep;
        execute(req, rep);

        hpx::error s = (hpx::error) rep.get_status();
        if (s != success && s != no_success)
        {
            HPX_RETHROW_EXCEPTION(s, "resolver_client::queryid", 
                rep.get_error());
            return false;
        }
        id = rep.get_id();
        return s == success;
    }

    ///////////////////////////////////////////////////////////////////////////
    bool resolver_client::get_statistics_count(std::vector<std::size_t>& counts) const
    {
        // send request
        server::request req (server::command_statistics_count);
        server::reply rep;
        execute(req, rep);

        hpx::error s = (hpx::error) rep.get_status();
        if (s != success && s != no_success)
        {
            HPX_RETHROW_EXCEPTION(s, "resolver_client::get_statistics_count", 
                rep.get_error());
            return false;
        }

        counts.clear();
        for (std::size_t i = 0; i < server::command_lastcommand; ++i)
            counts.push_back(std::size_t(rep.get_statictics(i)));

        return s == success;
    }

    ///////////////////////////////////////////////////////////////////////////
    bool resolver_client::get_statistics_mean(std::vector<double>& timings) const
    {
        // send request
        server::request req (server::command_statistics_mean);
        server::reply rep;
        execute(req, rep);

        hpx::error s = (hpx::error) rep.get_status();
        if (s != success && s != no_success)
        {
            HPX_RETHROW_EXCEPTION(s, "resolver_client::get_statistics_mean", 
                rep.get_error());
            return false;
        }

        std::swap(timings, rep.get_statictics());
        return s == success;
    }

    ///////////////////////////////////////////////////////////////////////////
    bool resolver_client::get_statistics_moment2(std::vector<double>& timings) const
    {
        // send request
        server::request req (server::command_statistics_moment2);
        server::reply rep;
        execute(req, rep);

        hpx::error s = (hpx::error) rep.get_status();
        if (s != success && s != no_success) 
        {
            HPX_RETHROW_EXCEPTION(s, "resolver_client::get_statistics_moment2", 
                rep.get_error());
            return false;
        }

        std::swap(timings, rep.get_statictics());
        return s == success;
    }

    ///////////////////////////////////////////////////////////////////////////
    bool resolver_client::read_completed(boost::system::error_code const& err, 
        std::size_t bytes_transferred, boost::uint32_t size)
    {
        return bytes_transferred >= size;
    }

    bool resolver_client::write_completed(boost::system::error_code const& err, 
        std::size_t bytes_transferred, boost::uint32_t size)
    {
        return bytes_transferred >= size;
    }

    ///////////////////////////////////////////////////////////////////////////
    boost::shared_ptr<resolver_client_connection> 
    resolver_client::get_client_connection() const
    {
        boost::shared_ptr<resolver_client_connection> client_connection(
            connection_cache_.get(there_));

        if (!client_connection) {
            // establish a new connection
            boost::system::error_code error = boost::asio::error::fault;
            for (int i = 0; i < HPX_MAX_NETWORK_RETRIES; ++i)
            {
                try {
                    client_connection.reset(new resolver_client_connection(
                            io_service_pool_.get_io_service())); 

                    locality::iterator_type end = there_.connect_end();
                    for (locality::iterator_type it = 
                            there_.connect_begin(io_service_pool_.get_io_service()); 
                         it != end; ++it)
                    {
                        client_connection->socket().close();
                        client_connection->socket().connect(*it, error);
                        if (!error) 
                            break;
                    }
                    if (!error)
                        break;

                    boost::this_thread::sleep(boost::get_system_time() + 
                        boost::posix_time::milliseconds(HPX_NETWORK_RETRIES_SLEEP));
                }
                catch (boost::system::error_code const& e) {
                    HPX_THROW_EXCEPTION(network_error, 
                        "resolver_client::get_client_connection", e.message());
                }
            }

            if (error) {
                client_connection->socket().close();

                HPX_OSSTREAM strm;
                strm << error.message() << " (while trying to connect to: " 
                     << there_ << ")";
                HPX_THROW_EXCEPTION(network_error, 
                    "resolver_client::get_client_connection", 
                    HPX_OSSTREAM_GETSTRING(strm));
            }
        }
        return client_connection;
    }

    ///////////////////////////////////////////////////////////////////////////
    void resolver_client::execute(server::request const &req, 
        server::reply& rep) const
    {
        typedef util::container_device<std::vector<char> > io_device_type;

        try {
            // connect socket
            std::vector<char> buffer;
            {
                // serialize the request
                boost::iostreams::stream<io_device_type> io(buffer);
#if HPX_USE_PORTABLE_ARCHIVES != 0
                util::portable_binary_oarchive archive(io);
#else
                boost::archive::binary_oarchive archive(io);
#endif
                archive << req;
            }

            // get existing connection to AGAS server or establish a new one
            boost::system::error_code err = boost::asio::error::try_again;

            boost::shared_ptr<resolver_client_connection> client_connection =
                get_client_connection();

            // send the data
            boost::integer::ulittle32_t size = (boost::integer::ulittle32_t)buffer.size();
            std::vector<boost::asio::const_buffer> buffers;
            buffers.push_back(boost::asio::buffer(&size, sizeof(size)));
            buffers.push_back(boost::asio::buffer(buffer));
            std::size_t written_bytes = boost::asio::write(
                client_connection->socket(), buffers,
                boost::bind(&resolver_client::write_completed, _1, _2, 
                    buffer.size() + sizeof(size)),
                err);
            if (err) {
                HPX_THROW_EXCEPTION(network_error, 
                    "resolver_client::execute", err.message());
            }
            if (buffer.size() + sizeof(size) != written_bytes) {
                HPX_THROW_EXCEPTION(network_error, 
                    "resolver_client::execute", "network write failed");
            }

            // wait for response
            // first read the size of the message 
            std::size_t reply_length = boost::asio::read(
                client_connection->socket(),
                boost::asio::buffer(&size, sizeof(size)),
                boost::bind(&resolver_client::read_completed, _1, _2, sizeof(size)),
                err);
            if (err) {
                HPX_THROW_EXCEPTION(network_error, 
                    "resolver_client::execute", err.message());
            }
            if (reply_length != sizeof(size)) {
                HPX_THROW_EXCEPTION(network_error, 
                    "resolver_client::execute", "network read failed");
            }

            // now read the rest of the message
            boost::uint32_t native_size = size;
            buffer.resize(native_size);
            reply_length = boost::asio::read(client_connection->socket(), 
                boost::asio::buffer(buffer), 
                boost::bind(&resolver_client::read_completed, _1, _2, native_size), 
                err);

            if (err) {
                HPX_THROW_EXCEPTION(network_error, 
                    "resolver_client::execute", err.message());
            }
            if (reply_length != native_size) {
                HPX_THROW_EXCEPTION(network_error, 
                    "resolver_client::execute", "network read failed");
            }

            // return the connection to the cache
            connection_cache_.add(there_, client_connection);

            // De-serialize the data
            {
                boost::iostreams::stream<io_device_type> io(buffer);
#if HPX_USE_PORTABLE_ARCHIVES != 0
                util::portable_binary_iarchive archive(io);
#else
                boost::archive::binary_iarchive archive(io);
#endif
                archive >> rep;
            }
        }
        catch (boost::system::system_error const& e) {
            HPX_THROW_EXCEPTION(network_error, 
                "resolver_client::execute", e.what());
        }        
        catch (std::exception const& e) {
            HPX_THROW_EXCEPTION(network_error, 
                "resolver_client::execute", e.what());
        }
        catch(...) {
            HPX_THROW_EXCEPTION(no_success, 
                "resolver_client::execute", "unexpected error");
        }
    }

///////////////////////////////////////////////////////////////////////////////
}}  // namespace hpx::naming

