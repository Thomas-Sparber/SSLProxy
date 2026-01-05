#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <asio.hpp>
#include <asio/ssl.hpp>

namespace net = asio;
namespace ssl = net::ssl;
using net::ip::tcp;

class ProxySession : public std::enable_shared_from_this<ProxySession>
{

public:
	enum
	{
		max_length = 8192
	};

	ProxySession(net::io_context& io_context, tcp::endpoint target_endpoint, std::unique_ptr<ssl::stream<tcp::socket>> client_socket) :
		client_socket_(std::move(client_socket)),
		target_socket_(io_context),
		target_endpoint_(std::move(target_endpoint)),
		target_response_started_(false),
		header_buffer_()
	{}

	void start() {
		auto self = shared_from_this();
		//std::cout << "DEBUG: [Session] ProxySession started. Connecting to target." << std::endl;

		target_socket_.async_connect(target_endpoint_, [this, self](const net::error_code& ec)
		{
			if (!ec)
			{
				//std::cout << "DEBUG: [Session] Target connected. Starting read/write cycles." << std::endl;
				self->start_read_from_client();
				self->start_read_from_target();
			}
			else
			{
				//std::cerr << "ProxySession: Target connect error: " << ec.message() << std::endl;
				self->close_all_resources();
			}
		});
	}

private:
	std::unique_ptr<ssl::stream<tcp::socket>> client_socket_;
	tcp::socket target_socket_;
	tcp::endpoint target_endpoint_;
	char client_data_[max_length];
	char target_data_[max_length];
	bool target_response_started_;
	std::string header_buffer_;
	std::string header_end_delimiter_ = "\r\n\r\n";

	void start_read_from_client()
	{
		auto self = shared_from_this();
		//std::cout << "DEBUG: [ClientRead] Waiting for data from client (Browser)." << std::endl;

		if (!client_socket_)
		{
			close_sockets_only_target();
			return;
		}

		client_socket_->async_read_some(
			net::buffer(client_data_, max_length),
			[this, self](const net::error_code& ec, std::size_t length)
			{
				if (!ec)
				{
					//std::cout << "DEBUG: Read " << length << " bytes from client (Encrypted)." << std::endl;
					net::async_write(self->target_socket_, net::buffer(self->client_data_, length), [this, self](const net::error_code& write_ec, std::size_t /*written*/)
					{
						if(!write_ec)
						{
							self->start_read_from_client();
						}
						else
						{
							std::cerr << "ProxySession: Write to target error: " << write_ec.message() << std::endl;
							self->do_shutdown();
						}
					});
				}
				else if (ec == net::error::eof)
				{
					//std::cout << "DEBUG: Client closed connection. Starting shutdown." << std::endl;
					self->do_shutdown();
				}
				else if (ec != net::error::operation_aborted)
				{
					//std::cerr << "ProxySession: Read from client error: " << ec.message() << std::endl;
					self->do_shutdown();
				}
			}
		);
	}

	void start_read_from_target()
	{
		auto self = shared_from_this();

		if(!client_socket_ || !target_socket_.is_open())
		{
			self->do_shutdown();
			return;
		}

		target_socket_.async_read_some(
			net::buffer(target_data_, max_length),
			[this, self](const net::error_code& ec, std::size_t length)
			{
				if(!ec)
				{
					if(self->target_response_started_)
					{
						//std::cout << "DEBUG: [TargetRead] Tunneling " << length << " bytes." << std::endl;

						net::async_write(*self->client_socket_, net::buffer(self->target_data_, length), [this, self, length](const net::error_code& write_ec, std::size_t /*written*/)
						{
							if(!write_ec)
							{
								self->start_read_from_target();
							}
							else
							{
								self->do_shutdown();
							}
						});
					}
					else
					{
						self->header_buffer_.append(self->target_data_, length);

						std::size_t header_end_pos = self->header_buffer_.find(self->header_end_delimiter_);

						if(header_end_pos != std::string::npos)
						{
							//std::cout << "DEBUG: [TargetRead] Header end found." << std::endl;
							self->target_response_started_ = true;

							if(self->header_buffer_.length() >= 10 && self->header_buffer_.substr(0, 9) == "HTTP/1.1 " && self->header_buffer_.substr(9, 1) == "3")
							{
								std::string search_location = "Location:";
								std::string search_http = "http://";
								std::string replace_https = "https://";

								std::size_t location_start = self->header_buffer_.find(search_location, 0);


								if(location_start != std::string::npos && location_start < header_end_pos)
								{
									std::size_t search_start = location_start + search_location.length();

									std::size_t http_pos = self->header_buffer_.find(search_http, search_start);

									if(http_pos != std::string::npos && http_pos < header_end_pos)
									{
										self->header_buffer_.replace(http_pos, search_http.length(), replace_https);
										//std::cout << "DEBUG: [TargetRead] Location: HTTP found and replaced with HTTPS." << std::endl;
									}
									else
									{
										//std::cout << "DEBUG: [TargetRead] Location header found, but 'http://' not found or not absolute. No replacement." << std::endl;
									}
								}
								else
								{
									//std::cout << "DEBUG: [TargetRead] Redirect (3xx) found, but 'Location:' header not found. No replacement." << std::endl;
								}
							}

							auto buffer_ptr = std::make_shared<std::string>(std::move(self->header_buffer_));

							net::async_write(
								*self->client_socket_,
								net::buffer(*buffer_ptr),
								[self, buffer_ptr](const net::error_code& write_ec, std::size_t /*written*/)
								{
									if(!write_ec)
									{
										self->start_read_from_target();
									}
									else
									{
										self->do_shutdown();
									}
								}
							);
						}
						else
						{
							self->start_read_from_target();
						}
					}
				}
				else if(ec == net::error::eof || ec == net::error::connection_reset)
				{
					self->do_shutdown();
				}
				else if (ec != net::error::operation_aborted)
				{
					//std::cerr << "ProxySession: Read from target error: " << ec.message() << std::endl;
					self->do_shutdown();
				}
			}
		);
	}

	void do_shutdown()
	{
		std::unique_ptr<ssl::stream<tcp::socket>> client_socket_moved = std::move(client_socket_);

		if (!client_socket_moved)
		{
			//std::cerr << "DEBUG: Attempted SSL shutdown on null socket (after move)." << std::endl;
			close_sockets_only_target();
			return;
		}

		auto self = shared_from_this();

		client_socket_moved->async_shutdown(
			[self, client_socket_moved = std::move(client_socket_moved)](const net::error_code& ec)
			{
				if(ec)
				{
					//std::cerr << "DEBUG: SSL Shutdown error (ignored): " << ec.message() << std::endl;
				}
				else
				{
					//std::cout << "DEBUG: SSL Shutdown complete." << std::endl;
				}

				self->close_sockets_only_target(); 
			}
		);
	}

	void close_sockets_only_target()
	{
		net::error_code ec;
		if (target_socket_.is_open())
		{
			target_socket_.shutdown(tcp::socket::shutdown_both, ec);
			target_socket_.close(ec);
			//std::cout << "DEBUG: Target socket closed." << std::endl;
		}

		//if (client_socket_)
		//{
		//	client_socket_.reset();
		//	//std::cout << "DEBUG: Client SSL stream destroyed." << std::endl;
		//}
	}

	void close_all_resources()
	{
		close_sockets_only_target();

		if(client_socket_)
		{
			client_socket_.reset();
			//std::cout << "DEBUG: Client SSL stream destroyed (by direct close)." << std::endl;
		}
	}

}; //end class ProxySession

class SslProxy
{

public:
	SslProxy(unsigned short source_port, const std::string& target_host, unsigned short target_port, const std::string& cert_file, const std::string& key_file, const std::string &key_password="") :
		io_context_ptr_(std::make_unique<net::io_context>()),
		io_context_(*io_context_ptr_),
		ssl_context_(ssl::context::sslv23_server),
		acceptor_(io_context_, tcp::endpoint(tcp::v4(), source_port)),
		target_endpoint_(),
		proxy_thread_(),
		certificate_file(cert_file),
		private_key_file(key_file),
		private_key_password(key_password)
	{
		try
		{
			tcp::resolver resolver(io_context_);
			auto endpoints = resolver.resolve(target_host, std::to_string(target_port));
			target_endpoint_ = *endpoints.begin();

			std::cout << "Proxy configured: SSL Port " << source_port
				<< " -> Target " << target_host << " ("
				<< target_endpoint_.address().to_string() << ":" << target_port << ")" << std::endl;

		}
		catch (const std::exception& e)
		{
			std::cerr << "FATAL: Could not resolve target host '" << target_host << "': " << e.what() << std::endl;
			throw;
		}

		load_certificates();

		ssl_context_.set_options(
			net::ssl::context::default_workarounds |
			net::ssl::context::no_sslv2 |
			net::ssl::context::no_sslv3 |
			net::ssl::context::single_dh_use
		);
	}

	~SslProxy()
	{
		stop();
		join_thread();
	}

	void start()
	{
		//std::cout << "SSL Proxy listening on port " << acceptor_.local_endpoint().port() << "..." << std::endl;
		do_accept();
	}

	void restart_context()
	{
		io_context_.restart();
		//std::cout << "DEBUG: [Proxy] io_context restarted." << std::endl;
		do_accept();
	}

	void start_thread()
	{
		if(proxy_thread_.joinable()) return;

		proxy_thread_ = std::thread([this] {
			//std::cout << "DEBUG: [Proxy Thread] Starting io_context::run()." << std::endl;
			this->run_block();
			//std::cout << "DEBUG: [Proxy Thread] Finished." << std::endl;
		});
	}

	void join_thread()
	{
		if(proxy_thread_.joinable())
		{
			proxy_thread_.join();
		}
	}

	void run_block()
        {
                io_context_.run();
        }

	void stop()
	{
		io_context_.stop();
	}

	net::io_context& get_context()
	{
		return io_context_;
	}

private:
	std::unique_ptr<net::io_context> io_context_ptr_;
	net::io_context& io_context_;
	ssl::context ssl_context_;
	tcp::acceptor acceptor_;
	tcp::endpoint target_endpoint_;
	std::thread proxy_thread_;
	std::string certificate_file;
	std::string private_key_file;
	std::string private_key_password;

	void load_certificates()
	{
		try
		{
			ssl_context_.set_password_callback(
				std::bind(&SslProxy::on_password_needed, this,
				std::placeholders::_1, std::placeholders::_2)
			);

			ssl_context_.use_certificate_chain_file(certificate_file);
			ssl_context_.use_private_key_file(private_key_file, ssl::context::pem);

			//std::cout << "Certificates loaded successfully." << std::endl;
		}
		catch (const std::exception& e)
		{
			std::cerr << "FATAL: Failed to load SSL files: " << e.what() << std::endl;
			throw;
		}
	}

	std::string on_password_needed(std::size_t max_length, net::ssl::context_base::password_purpose purpose)
	{
		(void)purpose;
		(void)max_length;

		return private_key_password;
	}

	void do_accept()
	{
		auto socket_ptr = std::make_shared<tcp::socket>(io_context_);

		//std::cout << "DEBUG: [SslProxy] Listening for new connection." << std::endl;

		acceptor_.async_accept(*socket_ptr, [this, socket_ptr](const net::error_code& ec)
		{
			if(!ec)
			{
				//std::cout << "DEBUG: [SslProxy] TCP connection accepted. Starting handshake." << std::endl;

				handle_handshake(std::make_unique<tcp::socket>(std::move(*socket_ptr)));
			}
			else
			{
				std::cerr << "Accept error: " << ec.message() << std::endl;
			}

			do_accept();
		});
	}

	void handle_handshake(std::unique_ptr<tcp::socket> tcp_socket)
	{
		auto ssl_stream_ptr = std::make_unique<ssl::stream<tcp::socket>>(std::move(*tcp_socket), ssl_context_);

		//std::cout << "DEBUG: [Handshake] async_handshake initiated." << std::endl;
		ssl_stream_ptr->async_handshake(
			ssl::stream_base::server,
			[this, ssl_stream_ptr = std::move(ssl_stream_ptr)](const net::error_code& ec) mutable
			{
				if (!ec)
				{
					//std::cout << "DEBUG: [Handshake] SSL Handshake completed successfully. Starting ProxySession." << std::endl;
					std::make_shared<ProxySession>(
						io_context_,
						target_endpoint_,
						std::move(ssl_stream_ptr)
					)->start();
				}
				else
				{
					std::cerr << "SSL Handshake error: " << ec.message() << std::endl;
				}
			}
		);
	}

}; //end class SslProxy
