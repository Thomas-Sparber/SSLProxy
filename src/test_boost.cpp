//If you need/want to use boost asio, you need to define BOOST_ASIO before including sslproxy
#define BOOST_ASIO
#include "../include/sslproxy.hpp"

#include <iostream>
#include <cstdlib>
#include <chrono>
#include <string>
#include <thread>

using std::cout;
using std::endl;
using std::string;

int printHelp(const string &programName);

int main(int argc, char *args[])
{
	int ssl_port = 443;
	string proxy_host = "localhost";
	int proxy_port = 80;
	string cert_file = "cert.pem";
	string priv_key = "key.pem";
	string priv_password = "1234";

	if(argc > 1 && string(args[1]) == "--help")
		return printHelp(args[0]);

	//Argument parsing
	if(argc > 1)ssl_port = atoi(args[1]);
	if(argc > 2)proxy_host = args[2];
	if(argc > 3)proxy_port = atoi(args[3]);
	if(argc > 4)cert_file = args[4];
	if(argc > 5)priv_key = args[5];
	if(argc > 6)priv_password = args[6];

	//Create and start SSL proxy
	SslProxy proxy(ssl_port, proxy_host, proxy_port, cert_file, priv_key, priv_password);
	proxy.start();
	proxy.run_block();
}

int printHelp(const string &programName)
{
	cout<<"Usage:"<<endl;
	cout<<programName<<" [sslport=443] [proxy_host=localhost] [proxy_port=80] [cert_file=cert.pem] [priv_key=key.pem] [priv_password=1234]"<<endl;
	cout<<endl;
	cout<<"\tsslport:"<<endl;
	cout<<"\t\tThe port on which the ssl proxy should listen for encrypted connections"<<endl;
	cout<<endl;
	cout<<"\tproxy_host:"<<endl;
	cout<<"\t\tThe target host of the unencrypted HTTP traffic"<<endl;
	cout<<endl;
	cout<<"\tproxy_port:"<<endl;
	cout<<"\t\tThe target port on the target host of the unencrypted HTTP traffic"<<endl;
	cout<<endl;
	cout<<"\tcert_file:"<<endl;
	cout<<"\t\tSSL certificate file"<<endl;
	cout<<endl;
	cout<<"\tpriv_key:"<<endl;
	cout<<"\t\tSSL certificate private key"<<endl;
	cout<<endl;
	cout<<"\tpriv_password:"<<endl;
	cout<<"\t\tPassword of the SSL key"<<endl;

	return 0;
}
