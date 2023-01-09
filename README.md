# Enzyme

This is a really bad HTTP server that I wrote in around februrary of 2022. It's written entirely in C (not counting the tests which are written in Python), and thus extremely insecure. The server is entirely single threaded, rapidly switching between any active connections.

# DO NOT USE THIS FOR ANY SERIOUS PURPOSES WHATSOEVER

This server is probably vulnerable to a large amount of attacks (primarily buffer overflows), not only that but it suffers from numerous amounts of memory leaks. You have been warned. The only reason I wrote this server was to learn about network programming as well as C.