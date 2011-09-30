#!/bin/sh

# Gen key
openssl genrsa -out key.pem 1024

# Gen request
openssl req -new -key key.pem -out req.pem

# Gen cert
openssl req -x509 -key key.pem -in req.pem -out cert.pem -days 999999

