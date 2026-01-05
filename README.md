# SSLProxy
Minimal C++ header-only library to add SSL security to an HTTP server

# Purpose
I created this library because I was using CrowCpp and wanted to have it listen on an HTTP and HTTPS port.
Initially I copied the Crow::App and made one listen on the HTTP port and the other on the HTTPS port.
Finally this lead to some problems, basically because Crow was not ment for such a scenario

 - I had to duplicate all the handler functions
 - finally this led to some very strange Crow errors, e.g. route was accessible an HTTP but not on HTTPS etc

This motivated me to create this tiny neat header-only library to solve this problem

# Features
Actually, this is very simple. It "just"

 - loads an SSL certificate + key from file
 - opens and listens an HTTPS port
 - accepts encrypted traffic
 - forwards the traffic to the target host and port
 - reads the answer from the target host and sends it back to the client.
 - It also handles redirects e.g. HTTP 301 - it replaces the http with https

All of this is done using libasio and openssl

# Usage / Examples
Please check the src folders for some examples how this SSL proxy can be used

#Certificate
Please don't use the provided example certificate for production use :-)

You can create a self signed certificate using OpenSSL, e.g.

    openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -sha256 -days 365
