// Copyright 2020 Burylov Denis <burylov01@mail.ru>
#include <header.hpp>
io_service service;

void init() {
  boost::shared_ptr<logging::core> core = logging::core::get();
  // logs to file for info
  boost::shared_ptr<sinks::text_file_backend> backend1 =
      boost::make_shared<sinks::text_file_backend>(
          keywords::file_name = "logs/file_info_%5N.log",
          keywords::rotation_size = 5 * 1024 * 1024,
          keywords::format = "[%TimeStamp%]: %Message%",
          keywords::time_based_rotation =
              sinks::file::rotation_at_time_point(12, 0, 0));

  typedef sinks::synchronous_sink<sinks::text_file_backend> sink_file;
  boost::shared_ptr<sink_file> sink1(new sink_file(backend1));
  sink1->set_filter(logging::trivial::severity >= logging::trivial::info);
  core->add_sink(sink1);

  // logs to file for trace
  boost::shared_ptr<sinks::text_file_backend> backend2 =
      boost::make_shared<sinks::text_file_backend>(
          keywords::file_name = "logs/file_trace_%5N.log",
          keywords::rotation_size = 5 * 1024 * 1024,
          keywords::format = "[%TimeStamp%]: %Message%",
          keywords::time_based_rotation =
              sinks::file::rotation_at_time_point(12, 0, 0));

  boost::shared_ptr<sink_file> sink2(new sink_file(backend2));
  sink2->set_filter(logging::trivial::severity <= logging::trivial::trace);
  core->add_sink(sink2);

  // logs to console
  boost::shared_ptr<sinks::text_ostream_backend> backend3 =
      boost::make_shared<sinks::text_ostream_backend>();
  backend3->add_stream(
      boost::shared_ptr<std::ostream>(&std::clog, boost::null_deleter()));

  typedef sinks::synchronous_sink<sinks::text_ostream_backend> sink_console;
  boost::shared_ptr<sink_console> sink3(new sink_console(backend3));
  sink3->set_filter(logging::trivial::severity >= logging::trivial::info);
  core->add_sink(sink3);
}

class talk_to_client;
std::vector<std::shared_ptr<talk_to_client>> clients;
boost::recursive_mutex mutex;

void list_changed();

class talk_to_client {
  ip::tcp::socket socket_;
  std::string username_;
  bool client_list_changed_;
  std::chrono::system_clock::time_point time_point_;

 public:
  talk_to_client() : socket_(service), client_list_changed_(false) {
    time_point_ = std::chrono::system_clock::now();
  }

  std::string username() const { return username_; }

  void answer_to_client() {
    try {
      read_request();
    } catch (boost::system::system_error&) {
      stop();
    }
    if (timed_out()) {
      stop();
      BOOST_LOG_TRIVIAL(info)
          << "disconnect " << username_ << " - timed out" << std::endl;
    }
  }
  void client_list_changed() { client_list_changed_ = true; }

  ip::tcp::socket& socket() { return socket_; }

  bool timed_out() const {
    auto now = std::chrono::system_clock::now();
    long ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - time_point_)
            .count();
    return ms > 5000;
  }

  void stop() {
    boost::system::error_code err;
    socket_.close(err);
  }

 private:
  void read_request() {
    char buff[512];
    size_t buffer_len = 0;
    if (socket_.available()) buffer_len = socket_.read_some(buffer(buff, 512));
    if (!(std::find(buff, buff + buffer_len, '\n') < buff + buffer_len)) return;
    time_point_ = std::chrono::system_clock::now();
    std::string msg(buff, buffer_len - 1);
    BOOST_LOG_TRIVIAL(trace)
        << "get request from " << username() << ": " << msg << std::endl;

    if (msg.find("login ") == 0)
      login(msg);
    else if (msg.find("ping") == 0)
      ping();
    else if (msg.find("client_list") == 0)
      client_list();
    else
      BOOST_LOG_TRIVIAL(error) << "invalid msg " << msg << std::endl;
  }

  void login(const std::string& msg) {
    std::istringstream in(msg);
    in >> username_ >> username_;
    BOOST_LOG_TRIVIAL(info) << username_ << " logged in" << std::endl;
    write("login ok\n");
    BOOST_LOG_TRIVIAL(trace) << "send answer to " << username() << ": "
                             << "login ok\n";
    list_changed();
  }

  void ping() {
    if (client_list_changed_) {
      write("ping client_list_changed\n");
      BOOST_LOG_TRIVIAL(trace) << "send answer to " << username() << ": "
                               << "ping client_list_changed\n";
    } else {
      write("ping ok\n");
      BOOST_LOG_TRIVIAL(trace) << "send answer to " << username() << ": "
                               << "ping ok\n";
    }
    client_list_changed_ = false;
  }

  void client_list() {
    std::string msg;
    {
      boost::recursive_mutex::scoped_lock lock(mutex);
      for (const auto& client : clients) msg += client->username() + " ";
    }
    write("clients " + msg + "\n");
    BOOST_LOG_TRIVIAL(trace) << "send answer to " << username() << ": "
                             << "clients " + msg + "\n";
  }

  void write(const std::string& msg) { socket_.write_some(buffer(msg)); }
};

void list_changed() {
  boost::recursive_mutex::scoped_lock lock(mutex);
  for (const auto& client : clients) client->client_list_changed();
}
void accept_thread() {
  ip::tcp::acceptor acceptor(service, ip::tcp::endpoint(ip::tcp::v4(), 8001));
  while (true) {
    auto client = std::make_shared<talk_to_client>();
    acceptor.accept(client->socket());
    boost::recursive_mutex::scoped_lock lock(mutex);
    clients.push_back(client);
  }
}

void handle_clients_thread() {
  while (true) {
    boost::this_thread::sleep(boost::posix_time::millisec(1));
    boost::recursive_mutex::scoped_lock lock(mutex);
    for (auto client : clients) client->answer_to_client();
    clients.erase(std::remove_if(clients.begin(), clients.end(),
                                 boost::bind(&talk_to_client::timed_out, _1)),
                  clients.end());
  }
}

int main() {
  init();
  boost::thread_group threads;
  threads.create_thread(accept_thread);
  threads.create_thread(handle_clients_thread);
  threads.join_all();
}
