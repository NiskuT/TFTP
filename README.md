# TFTP
Implemetation of a TFTP client

How to use puttftp:
./puttftp ip file
./puttftp ip port file
./puttftp ip file -b xxxx
./puttftp ip port file -b xxxx

With xxxx the value of blocksize option

How to use gettftp:
./gettftp ip file
./gettftp ip port file
./gettftp ip file -b xxxx
./gettftp ip port file -b xxxx

With xxxx the value of blocksize option

For more informations about TFTP Protocol, see:
      -RFC 1350 for TFTP v2
      -RFC 2347 for TFTP options
      -RFC 2348 for blocksize option
      -RFC 2349 for timeout & tsize options (not implemented here)
      -RFC 7440 for TFTP Windowsize Option (not implemented here)
