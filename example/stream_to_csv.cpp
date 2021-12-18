#include <shadowmocap.hpp>

#include <asio/co_spawn.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <asio/io_context.hpp>
#include <asio/stream_file.hpp>

#include <chrono>
#include <iostream>
#include <string>

namespace net = shadowmocap::net;
using net::ip::tcp;

// Handler function for asio::co_spawn to propagate exceptions to the caller
void rethrow_exception_ptr(std::exception_ptr ptr)
{
  if (ptr) {
    std::rethrow_exception(ptr);
  }
}

net::awaitable<void> read_shadowmocap_datastream_frames(
  shadowmocap::datastream<tcp> &stream, net::stream_file &file)
{
  auto start = std::chrono::steady_clock::now();

  std::size_t num_bytes = 0;
  for (int i = 0; i < 100; ++i) {
    stream.deadline = std::max(
      stream.deadline,
      std::chrono::steady_clock::now() + std::chrono::seconds(1));

    auto message = co_await read_message<std::string>(stream);
    num_bytes += std::size(message);

    co_await net::async_write(file, net::buffer(message), net::use_awaitable);
  }

  auto end = std::chrono::steady_clock::now();

  std::cout << "read 100 samples (" << num_bytes << " bytes) in "
            << std::chrono::duration<double>(end - start).count() << "\n";
}

net::awaitable<void> read_shadowmocap_datastream(const tcp::endpoint &endpoint)
{
  using namespace shadowmocap;
  using namespace net::experimental::awaitable_operators;

  auto stream = co_await open_connection(endpoint);

  const std::string xml = "<configurable><Lq/><c/></configurable>";
  co_await write_message(stream, xml);

  net::stream_file file(
    co_await net::this_coro::executor, "out.csv",
    asio::stream_file::write_only | net::stream_file::create |
      net::stream_file::truncate);

  co_await(
    read_shadowmocap_datastream_frames(stream, file) ||
    watchdog(stream.deadline));
}

bool run()
{
  try {
    net::io_context ctx;

    const std::string host = "127.0.0.1";
    const std::string service = "32076";

    auto endpoint = *shadowmocap::tcp::resolver(ctx).resolve(host, service);

    co_spawn(ctx, read_shadowmocap_datastream(endpoint), rethrow_exception_ptr);

    ctx.run();

    return true;
  } catch (std::exception &e) {
    std::cerr << e.what() << "\n";
  }

  return false;
}

/**
  Utility class to store all of the options to run our Motion SDK data stream.
*/
class command_line_options {
public:
  command_line_options();

  /**
    Read the command line tokens and load them into this state. Returns 0 if
    successful, -1 if the command line options are invalid, or 1 if the help
    message should be printed out.
  */
  int parse(int argc, char **argv);

  /**
    Print command line usage help for this program. Returns 1 which is intended
    to be the main return code as well.
  */
  int print_help(std::ostream *out, char *program_name);

  /**
    Store an error or informational message about why the parse phase failed.
    This will be shown to the user with additional help so they can correct the
    input parameters.
  */
  std::string message;

  /**
    Stream the formatted data to a file, defaults to empty which writes to
    std::cout.
  */
  std::string filename;

  /**
    Read N frames and then stop sampling, defaults to 0 which indicates no limit
    and to keep streaming for as long as possible.
  */
  int frames;

  /** IP address to connect to, defaults to "127.0.0.1". */
  std::string address;

  /** Port to connect to, defaults to 32076. */
  unsigned port;

  /** Print this string in between every column, defaults to ",". */
  std::string separator;

  /** Print this string in between every row, defaults to "\n". */
  std::string newline;

  /**
    Set to true to print out string channel names in the 0th row, defaults to
    false.
  */
  bool header;
}; // class command_line_options

int main(int argc, char **argv)
{
  command_line_options options;
  if (options.parse(argc, argv) != 0) {
    return options.print_help(&std::cerr, *argv);
  }

#if 0

  // Stream frames to a CSV spreadsheet file.
  std::ofstream fout;
  if (!options.filename.empty()) {
    fout.open(options.filename, std::ios_base::out | std::ios_base::binary);
  }

  // Capture error messages so we do not interfere with the CSV output stream.
  std::ostringstream err;

  const int result =
    stream_data_to_csv(fout.is_open() ? &fout : &std::cout, &err, options);
  if (result != 0) {
    std::cerr << err.str() << options.newline;
  }

#endif

  if (!run()) {
    return -1;
  }

  return 0;
}

command_line_options::command_line_options()
  : message(), filename(), frames(), address("127.0.0.1"), port(32076),
    separator(","), newline("\n"), header(false)
{
}

int command_line_options::parse(int argc, char **argv)
{
  for (int i = 1; i < argc; ++i) {
    const std::string arg(argv[i]);
    if ("--file" == arg) {
      ++i;
      if (i < argc) {
        filename = argv[i];
      } else {
        message = "Missing required argument for --file";
        return -1;
      }
    } else if ("--frames" == arg) {
      ++i;
      if (i < argc) {
        frames = std::stoi(argv[i]);
      } else {
        message = "Missing required argument for --frames";
        return -1;
      }
    } else if ("--header" == arg) {
      header = true;
    } else if ("--help" == arg) {
      return 1;
    } else {
      message = "Unrecognized option \"" + arg + "\"";
      return -1;
    }
  }

  return 0;
}

int command_line_options::print_help(std::ostream *out, char *program_name)
{
  if (!message.empty()) {
    *out << message << newline << newline;
  }

  *out << "Usage: " << program_name << " [options...]" << newline << newline
       << "Allowed options:" << newline << "  --help         show help message"
       << newline << "  --file arg     output file" << newline
       << "  --frames N     read N frames" << newline
       << "  --header       show channel names in the first row" << newline
       << newline;

  return 1;
}