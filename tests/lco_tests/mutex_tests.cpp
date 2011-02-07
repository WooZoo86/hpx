//  Copyright (C) 2001-2003 William E. Kempf
//  Copyright (c) 2007-2011 Hartmut Kaiser
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying 
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/hpx.hpp>
#include <hpx/lcos/mutex.hpp>
#include <hpx/lcos/recursive_mutex.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/serialization/export.hpp>

using namespace hpx;
namespace po = boost::program_options;

///////////////////////////////////////////////////////////////////////////////
template <typename M>
struct test_lock
{
    typedef M mutex_type;
    typedef typename M::scoped_lock lock_type;

    void operator()()
    {
        mutex_type mutex;

        // Test the lock's constructors.
        {
            lock_type lock(mutex, boost::defer_lock);
            BOOST_TEST(!lock);
        }

        lock_type lock(mutex);
        BOOST_TEST(lock ? true : false);

//         // Construct and initialize an xtime for a fast time out.
//         boost::condition condition;
//         boost::xtime xt = delay(0, 100);
// 
//         // Test the lock and the mutex with condition variables.
//         // No one is going to notify this condition variable.  We expect to
//         // time out.
//         BOOST_TEST(!condition.timed_wait(lock, xt));
//         BOOST_TEST(lock ? true : false);

        // Test the lock and unlock methods.
        lock.unlock();
        BOOST_TEST(!lock);
        lock.lock();
        BOOST_TEST(lock ? true : false);
    }
};

template <typename M>
struct test_trylock
{
    typedef M mutex_type;
    typedef typename M::scoped_try_lock try_lock_type;

    void operator()()
    {
        mutex_type mutex;

        // Test the lock's constructors.
        {
            try_lock_type lock(mutex);
            BOOST_TEST(lock ? true : false);
        }
        {
            try_lock_type lock(mutex, boost::defer_lock);
            BOOST_TEST(!lock);
        }
        try_lock_type lock(mutex);
        BOOST_TEST(lock ? true : false);

//         // Construct and initialize an xtime for a fast time out.
//         boost::condition condition;
//         boost::xtime xt = delay(0, 100);
// 
//         // Test the lock and the mutex with condition variables.
//         // No one is going to notify this condition variable.  We expect to
//         // time out.
//         BOOST_TEST(!condition.timed_wait(lock, xt));
//         BOOST_TEST(lock ? true : false);

        // Test the lock, unlock and trylock methods.
        lock.unlock();
        BOOST_TEST(!lock);
        lock.lock();
        BOOST_TEST(lock ? true : false);
        lock.unlock();
        BOOST_TEST(!lock);
        BOOST_TEST(lock.try_lock());
        BOOST_TEST(lock ? true : false);
    }
};

template<typename Mutex>
struct test_lock_times_out_if_other_thread_has_lock 
  : components::detail::simple_component_tag
{
    typedef boost::unique_lock<Mutex> Lock;

    Mutex m;
    hpx::lcos::mutex done_mutex;
    bool done;
    bool locked;

    test_lock_times_out_if_other_thread_has_lock():
        done(false), locked(false)
    {}

    void locking_thread(boost::uint64_t id)
    {
        Lock lock (m, boost::defer_lock);
        lock.timed_lock(boost::posix_time::milliseconds(5));

        boost::lock_guard<hpx::lcos::mutex> lk(done_mutex);
        locked = lock.owns_lock();
        done = true;
        threads::set_thread_state(threads::thread_id_type(id));
    }

    void locking_thread_through_constructor(boost::uint64_t id)
    {
        Lock lock(m, boost::posix_time::milliseconds(5));

        boost::lock_guard<hpx::lcos::mutex> lk(done_mutex);
        locked = lock.owns_lock();
        done = true;
        threads::set_thread_state(threads::thread_id_type(id));
    }

    typedef test_lock_times_out_if_other_thread_has_lock<Mutex> this_type;

    template <typename Action>
    void do_test()
    {
        Lock lock(m);

        locked = false;
        done = false;

        naming::gid_type this_(applier::get_applier().get_runtime_support_raw_gid());
        this_.set_lsb(this);

        threads::thread_self& self = threads::get_self();
        threads::thread_id_type id = self.get_thread_id();
        hpx::lcos::eager_future<Action> f(this_, boost::uint64_t(id));

        try
        {
            {
                // create timeout thread
                threads::set_thread_state(id, boost::posix_time::seconds(3));

                // suspend this thread waiting for test to happen
                BOOST_TEST(threads::wait_timeout != self.yield(threads::suspended));

                BOOST_TEST(done);
                BOOST_TEST(!locked);
            }

            lock.unlock();
            f.get();        // synchronize with spawned function
        }
        catch(...)
        {
            lock.unlock();
            f.get();        // synchronize with spawned function
            throw;
        }
    }

    typedef actions::action1<
        this_type, 0, boost::uint64_t, &this_type::locking_thread
    > locking_thread_action;

    typedef actions::action1<
        this_type, 1, boost::uint64_t,
        &this_type::locking_thread_through_constructor
    > locking_thread_through_constructor_action;

    void operator()()
    {
        do_test<locking_thread_action>();
        do_test<locking_thread_through_constructor_action>();
    }
};

HPX_DEFINE_GET_COMPONENT_TYPE_STATIC(
    test_lock_times_out_if_other_thread_has_lock<lcos::timed_mutex>,
    hpx::components::component_invalid);
HPX_REGISTER_ACTION_EX(
    test_lock_times_out_if_other_thread_has_lock<lcos::timed_mutex>::locking_thread_action,
    test_lock_times_out_if_other_thread_has_lock_time_mutex_locking_thread_action);
HPX_REGISTER_ACTION_EX(
    test_lock_times_out_if_other_thread_has_lock<lcos::timed_mutex>::locking_thread_through_constructor_action,
    test_lock_times_out_if_other_thread_has_lock_time_mutex_locking_thread_through_constructor_action);

HPX_DEFINE_GET_COMPONENT_TYPE_STATIC(
    test_lock_times_out_if_other_thread_has_lock<lcos::recursive_timed_mutex>,
    hpx::components::component_invalid);
HPX_REGISTER_ACTION_EX(
    test_lock_times_out_if_other_thread_has_lock<lcos::recursive_timed_mutex>::locking_thread_action,
    test_lock_times_out_if_other_thread_has_lock_recursive_time_mutex_locking_thread_action);
HPX_REGISTER_ACTION_EX(
    test_lock_times_out_if_other_thread_has_lock<lcos::recursive_timed_mutex>::locking_thread_through_constructor_action,
    test_lock_times_out_if_other_thread_has_lock_recursive_time_mutex_locking_thread_through_constructor_action);

template <typename M>
struct test_timedlock
{
    typedef M mutex_type;
    typedef typename M::scoped_timed_lock timed_lock_type;

    static bool fake_predicate()
    {
        return false;
    }

    void operator()()
    {
        test_lock_times_out_if_other_thread_has_lock<mutex_type>()();

        mutex_type mutex;

        // Test the lock's constructors.
        {
            // Construct and initialize an xtime for a fast time out.
            boost::system_time xt = boost::get_system_time() + 
                boost::posix_time::milliseconds(100);

            timed_lock_type lock(mutex, xt);
            BOOST_TEST(lock ? true : false);
        }
        {
            timed_lock_type lock(mutex, boost::defer_lock);
            BOOST_TEST(!lock);
        }
        timed_lock_type lock(mutex);
        BOOST_TEST(lock ? true : false);

//         // Construct and initialize an xtime for a fast time out.
//         boost::system_time timeout = boost::get_system_time()+boost::posix_time::milliseconds(100);
// 
//         // Test the lock and the mutex with condition variables.
//         // No one is going to notify this condition variable.  We expect to
//         // time out.
//         boost::condition condition;
//         BOOST_TEST(!condition.timed_wait(lock, timeout, fake_predicate));
//         BOOST_TEST(lock ? true : false);
// 
//         boost::system_time now=boost::get_system_time();
//         boost::posix_time::milliseconds const timeout_resolution(20);
//         BOOST_TEST((timeout-timeout_resolution)<now);

        // Test the lock, unlock and timedlock methods.
        lock.unlock();
        BOOST_TEST(!lock);
        lock.lock();
        BOOST_TEST(lock ? true : false);
        lock.unlock();
        BOOST_TEST(!lock);
        boost::system_time target = boost::get_system_time()+boost::posix_time::milliseconds(100);
        BOOST_TEST(lock.timed_lock(target));
        BOOST_TEST(lock ? true : false);
        lock.unlock();
        BOOST_TEST(!lock);

        BOOST_TEST(mutex.timed_lock(boost::posix_time::milliseconds(100)));
        mutex.unlock();

        BOOST_TEST(lock.timed_lock(boost::posix_time::milliseconds(100)));
        BOOST_TEST(lock ? true : false);
        lock.unlock();
        BOOST_TEST(!lock);
    }
};

template <typename M>
struct test_recursive_lock
{
    typedef M mutex_type;
    typedef typename M::scoped_lock lock_type;

    void operator()()
    {
        mutex_type mx;
        lock_type lock1(mx);
        lock_type lock2(mx);
    }
};

void do_test_mutex(int num_tests)
{
    test_lock<hpx::lcos::mutex> t;
    for (int i = 0; i < num_tests; ++i)
    {
        applier::register_work_nullary(t, "do_test_mutex_t");
    }
}

void do_test_try_mutex(int num_tests)
{
    test_lock<hpx::lcos::try_mutex> t1;
    test_trylock<hpx::lcos::try_mutex> t2;
    for (int i = 0; i < num_tests; ++i)
    {
        applier::register_work_nullary(t1, "do_test_try_mutex_t1");
        applier::register_work_nullary(t2, "do_test_try_mutex_t2");
    }
}

void do_test_timed_mutex(int num_tests)
{
    test_lock<hpx::lcos::timed_mutex> t1;
    test_trylock<hpx::lcos::timed_mutex> t2;
    test_timedlock<hpx::lcos::timed_mutex> t3;
    for (int i = 0; i < num_tests; ++i)
    {
        applier::register_work_nullary(t1, "do_test_timed_mutex_t1");
        applier::register_work_nullary(t2, "do_test_timed_mutex_t2");
        applier::register_work_nullary(t3, "do_test_timed_mutex_t3");
    }
}

void do_test_recursive_mutex(int num_tests)
{
    test_lock<hpx::lcos::recursive_mutex> t1;
    test_recursive_lock<hpx::lcos::recursive_mutex> t2;
    for (int i = 0; i < num_tests; ++i)
    {
        applier::register_work_nullary(t1, "do_test_recursive_mutex_t1");
        applier::register_work_nullary(t2, "do_test_recursive_mutex_t2");
    }
}

void do_test_recursive_try_mutex(int num_tests)
{
    test_lock<hpx::lcos::recursive_try_mutex> t1;
    test_trylock<hpx::lcos::recursive_try_mutex> t2;
    test_recursive_lock<hpx::lcos::recursive_try_mutex> t3;
    for (int i = 0; i < num_tests; ++i)
    {
        applier::register_work_nullary(t1, "do_test_recursive_try_mutex_t1");
        applier::register_work_nullary(t2, "do_test_recursive_try_mutex_t2");
        applier::register_work_nullary(t3, "do_test_recursive_try_mutex_t3");
    }
}

void do_test_recursive_timed_mutex(int num_tests)
{
    test_lock<hpx::lcos::recursive_timed_mutex> t1;
    test_trylock<hpx::lcos::recursive_timed_mutex> t2;
    test_timedlock<hpx::lcos::recursive_timed_mutex> t3;
    test_recursive_lock<hpx::lcos::recursive_timed_mutex> t4;
    for (int i = 0; i < num_tests; ++i)
    {
        applier::register_work_nullary(t1, "do_test_recursive_timed_mutex_t1");
        applier::register_work_nullary(t2, "do_test_recursive_timed_mutex_t2");
        applier::register_work_nullary(t3, "do_test_recursive_timed_mutex_t3");
        applier::register_work_nullary(t4, "do_test_recursive_timed_mutex_t4");
    }
}

///////////////////////////////////////////////////////////////////////////////
int hpx_main(int num_threads, int num_tests)
{
//     applier::register_work_nullary(
//         boost::bind(&do_test_mutex, num_tests), "do_test_mutex");
//     applier::register_work_nullary(
//         boost::bind(&do_test_try_mutex, num_tests), "do_test_try_mutex");
    if (num_threads > 1) {
        applier::register_work_nullary(
            boost::bind(&do_test_timed_mutex, num_tests), "do_test_timed_mutex");
    }
// 
//     applier::register_work_nullary(
//         boost::bind(&do_test_recursive_mutex, num_tests), 
//             "do_test_recursive_mutex");
//     applier::register_work_nullary(
//         boost::bind(&do_test_recursive_try_mutex, num_tests), 
//             "do_test_recursive_try_mutex");
//     if (num_threads > 1) {
//         applier::register_work_nullary(
//             boost::bind(&do_test_recursive_timed_mutex, num_tests), 
//                 "do_test_recursive_timed_mutex");
//     }

    // initiate shutdown of the runtime system
    components::stubs::runtime_support::shutdown_all();
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
bool parse_commandline(char const* name, int argc, char *argv[], 
    po::variables_map& vm)
{
    try {
        std::string usage("Usage: ");
        usage += name;
        usage += " [options]";

        po::options_description desc_cmdline (usage);
        desc_cmdline.add_options()
            ("help,h", "print out program usage (this message)")
            ("run_agas_server,r", "run AGAS server as part of this runtime instance")
            ("worker,w", "run this instance in worker (non-console) mode")
            ("agas", po::value<std::string>(), 
                "the IP address the AGAS server is running on (default taken "
                "from hpx.ini), expected format: 192.168.1.1:7912")
            ("hpx", po::value<std::string>(), 
                "the IP address the HPX parcelport is listening on (default "
                "is localhost:7910), expected format: 192.168.1.1:7913")
            ("threads,t", po::value<int>(), 
                "the number of operating system threads to spawn for this"
                "HPX locality")
            ("concurrency,c", po::value<int>(), 
                "the number of concurrent tests to run (default: 1)")
            ("num_tests,n", po::value<int>(), 
                "the number of times to repeat the test (default: 1)")
            ("queueing,q", po::value<std::string>(),
                "the queue scheduling policy to use, options are 'global' "
                " and 'local' (default is 'global')")
        ;

        po::store(po::command_line_parser(argc, argv)
            .options(desc_cmdline).run(), vm);
        po::notify(vm);

        // print help screen
        if (vm.count("help")) {
            std::cout << desc_cmdline;
            return false;
        }
    }
    catch (std::exception const& e) {
        std::cerr << "hpx_runtime: exception caught: " << e.what() << std::endl;
        return false;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// helper class for DGAS server initialization
class agas_server_helper
{
public:
    agas_server_helper(std::string host, boost::uint16_t port)
      : agas_pool_(2, "agas_server_pool"), agas_(agas_pool_, host, port)
    {
        agas_.run(false);
    }
    ~agas_server_helper()
    {
        agas_.stop();
    }

private:
    hpx::util::io_service_pool agas_pool_; 
    hpx::naming::resolver_server agas_;
};

///////////////////////////////////////////////////////////////////////////////
inline void 
split_ip_address(std::string const& v, std::string& addr, boost::uint16_t& port)
{
    try {
        std::string::size_type p = v.find_first_of(":");
        if (p != std::string::npos) {
            addr = v.substr(0, p);
            port = boost::lexical_cast<boost::uint16_t>(v.substr(p+1));
        }
        else {
            addr = v;
        }
    }
    catch (boost::bad_lexical_cast const& /*e*/) {
        ;   // ignore bad_cast exceptions
    }
}

///////////////////////////////////////////////////////////////////////////////
void run_global(std::string hpx_host, boost::uint16_t hpx_port, 
    std::string dgas_host, boost::uint16_t dgas_port, hpx::runtime::mode mode,
    int num_threads, int num_tests, int concurrency)
{
    typedef hpx::threads::policies::global_queue_scheduler queue_policy;
    typedef hpx::runtime_impl<queue_policy> runtime_type;

    if (0 == num_threads) {
        int num_of_cores = boost::thread::hardware_concurrency();
        runtime_type rt(hpx_host, hpx_port, dgas_host, dgas_port, mode);
        for (int t = 0; t < num_tests; ++t) {
            for (int i = 1; i <= 2*num_of_cores; ++i) { 
                rt.run(boost::bind(&hpx_main, i, concurrency), i);
                std::cerr << ".";
            }
        }
        std::cerr << "\n";
    }
    else {
        runtime_type rt(hpx_host, hpx_port, dgas_host, dgas_port, mode);
        for (int t = 0; t < num_tests; ++t) {
            rt.run(boost::bind(&hpx_main, num_threads, concurrency), num_threads);
            std::cerr << ".";
        }
        std::cerr << "\n";
    }
}

void run_local(std::string hpx_host, boost::uint16_t hpx_port, 
    std::string dgas_host, boost::uint16_t dgas_port, hpx::runtime::mode mode,
    int num_threads, int num_tests, int concurrency)
{
    typedef hpx::threads::policies::local_queue_scheduler queue_policy;
    typedef hpx::runtime_impl<queue_policy> runtime_type;

    if (0 == num_threads) {
        int num_of_cores = boost::thread::hardware_concurrency();
        for (int i = 1; i <= 2*num_of_cores; ++i) { 
            queue_policy::init_parameter_type init(i);
            runtime_type rt(hpx_host, hpx_port, dgas_host, dgas_port, mode, init);
            for (int t = 0; t < num_tests; ++t) {
                rt.run(boost::bind(&hpx_main, i, concurrency), i);
                std::cerr << ".";
            }
        }
        std::cerr << "\n";
    }
    else {
        queue_policy::init_parameter_type init(num_threads);
        runtime_type rt(hpx_host, hpx_port, dgas_host, dgas_port, mode, init);
        for (int t = 0; t < num_tests; ++t) {
            rt.run(boost::bind(&hpx_main, num_threads, concurrency), num_threads);
            std::cerr << ".";
        }
        std::cerr << "\n";
    }
}

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    try {
        // analyze the command line
        po::variables_map vm;
        if (!parse_commandline("hpx_runtime", argc, argv, vm))
            return -1;

        // Check command line arguments.
        std::string hpx_host("localhost"), dgas_host;
        boost::uint16_t hpx_port = HPX_PORT, dgas_port = 0;
        hpx::runtime::mode mode = hpx::runtime::console;    // default is console mode
        int num_threads = 0;
        int num_tests = 1;
        int concurrency = 1;
        std::string queueing("global");

        // extract IP address/port arguments
        if (vm.count("agas")) 
            split_ip_address(vm["agas"].as<std::string>(), dgas_host, dgas_port);

        if (vm.count("hpx")) 
            split_ip_address(vm["hpx"].as<std::string>(), hpx_host, hpx_port);

        if (vm.count("worker"))
            mode = hpx::runtime::worker;

        if (vm.count("threads"))
            num_threads = vm["threads"].as<int>();

        if (vm.count("num_tests"))
            num_tests = vm["num_tests"].as<int>();

        if (vm.count("concurrency"))
            concurrency = vm["concurrency"].as<int>();

        if (vm.count("queueing"))
            queueing = vm["queueing"].as<std::string>();

        // initialize and run the DGAS service, if appropriate
        std::auto_ptr<agas_server_helper> dgas_server;
        if (vm.count("run_agas_server"))  // run the AGAS server instance here
            dgas_server.reset(new agas_server_helper(dgas_host, dgas_port));

        // start the HPX runtime using different numbers of threads
        if (queueing == "global") {
            run_global(hpx_host, hpx_port, dgas_host, dgas_port, mode,
                num_threads, num_tests, concurrency);
        }
        else if (queueing == "local") {
            run_local(hpx_host, hpx_port, dgas_host, dgas_port, mode,
                num_threads, num_tests, concurrency);
        }
        else {
            throw std::logic_error("bad value for parameter --queuing/-q");
        }
    }
    catch (std::exception& e) {
        BOOST_TEST(false);
        std::cerr << "std::exception caught: " << e.what() << "\n";
    }
    catch (...) {
        BOOST_TEST(false);
        std::cerr << "unexpected exception caught\n";
    }
    return boost::report_errors();
}
