ServerTimeSync.js
=================

Use the server time at the client side.

How does it work?
-----------------

Include the `sync.js` into your html, and setup the `timeURL` to the time server.

```html
<script>
var timeURL = "http://time.server.com/";
 (function(w,s){
    var f = d.getElementsByTagName(s)[0],
      j = d.createElement(s),
    j.async = true;
    j.src = '/sync.js';
    f.parentNode.insertBefore(j,f);
  })(window,document,'script');
</script>
```

The time server is a very lightweight server, capable of handling thousands of requests per second, and returns a json with time.

The script `sync.js` will override `Date` class, and it will contain the server time; to use the client time you must use `LocalDate` class.

The time synchronization is asynchronous and the first time could take a few seconds, but we store the offset in localStorage to avoid resynchronizing for the next hour.

```html
<script>
var timeURL = "http://37.153.159.15:9999";
var timeSynced = false;
var retry = setInterval(function() {
    if (timeSynced) {
        clearInterval(retry);
        alert("Time is sinced, there is " + (Date.now() - LocalDate.now()) + "ms of difference");
    }
}, 100);
</script>
<script src="sync.js"></script>
```

Server
------

The server is very simple, it is written in NodeJS. I'll push a C server which performs much better.

License
-------

```text
Copyright (c) 2014, César Rodas
All rights reserved.

Redistribution and use in source and binary forms are permitted
provided that the above copyright notice and this paragraph are
duplicated in all such forms and that any documentation,
advertising materials, and other materials related to such
distribution and use acknowledge that the software was developed
by the <organization>. The name of the
<organization> may not be used to endorse or promote products derived
from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
```
