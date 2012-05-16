Python Jansson Bind Readme
==========================

This library jansson bind implementation for Python.
Library jansson https://github.com/akheron/jansson.

In some cases, you can replace that became a standard library of simplejson.
The module is written in pure C and works a little bit which is interpreted simplejson python.

For example:
------------

  try:
      import pyjansson as simplejson
  except ImportError:
      import simplejson
  
  print simplejson.dumps({'title': 'Hyperion', 'autor': 'Dan Simmons'})
