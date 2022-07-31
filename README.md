# cweb
my terrible web server in c.

HTTP 1.0

Serves:

  text:sendtext()
  
  files:sendFileWeb()
  
  headers:sendHrader()
  
dont use sendheader unless youre implementing other function
  
Proscesses HTTP Request into:

  Struct webRequest:
  
    method, uri, version, body
    
    array of Headers(key, value)
    
Implements a key value store called MON(Mystery Object Notation):

  Add a key value pair to file: monWrite()
  
  Read all key-value pairs from file: monRead()
  
  Delete a key value pait from file: monDelete()
  
and not much else...

this took me an embarrasingly long time

if anyone want to implement getting a users uploaded file, be my guest.
