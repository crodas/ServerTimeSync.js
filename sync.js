/*
  +---------------------------------------------------------------------------------+
  | Copyright (c) 2014 César Rodas                                                  |
  +---------------------------------------------------------------------------------+
  | Redistribution and use in source and binary forms, with or without              |
  | modification, are permitted provided that the following conditions are met:     |
  | 1. Redistributions of source code must retain the above copyright               |
  |    notice, this list of conditions and the following disclaimer.                |
  |                                                                                 |
  | 2. Redistributions in binary form must reproduce the above copyright            |
  |    notice, this list of conditions and the following disclaimer in the          |
  |    documentation and/or other materials provided with the distribution.         |
  |                                                                                 |
  | 3. All advertising materials mentioning features or use of this software        |
  |    must display the following acknowledgement:                                  |
  |    This product includes software developed by César D. Rodas.                  |
  |                                                                                 |
  | 4. Neither the name of the César D. Rodas nor the                               |
  |    names of its contributors may be used to endorse or promote products         |
  |    derived from this software without specific prior written permission.        |
  |                                                                                 |
  | THIS SOFTWARE IS PROVIDED BY CÉSAR D. RODAS ''AS IS'' AND ANY                   |
  | EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED       |
  | WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE          |
  | DISCLAIMED. IN NO EVENT SHALL CÉSAR D. RODAS BE LIABLE FOR ANY                  |
  | DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES      |
  | (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;    |
  | LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND     |
  | ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT      |
  | (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS   |
  | SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE                     |
  +---------------------------------------------------------------------------------+
  | Authors: César Rodas <crodas@php.net>                                           |
  +---------------------------------------------------------------------------------+
*/
(function(window, URL, LocalDate, localStorage) {
    var samples = []
        , offset = 0
    if (!URL) return;

    window.timeSynced = false;

    var persist = {}
    try {
        if (localStorage.time_offset) persist = JSON.parse(localStorage.time_offset);
    } catch (e) {}

    if (persist[URL] && persist[URL][0] > LocalDate.now()) {
        offset = persist[URL][1]
        override();
        return;
    }

    function override() {
        // http://stackoverflow.com/questions/13839318/javascript-override-date-prototype-constructor
        var bind = Function.bind;
        var unbind = bind.bind(bind);

        function instantiate(constructor, args) {
            return new (unbind(constructor, null).apply(null, args));
        }

        window.timeSynced = true;
        window.LocalDate = LocalDate;
        window.Date = function (Date) {
            return function () {
                var date = instantiate(Date, arguments);
                var args = arguments.length;
                var arg = arguments[0];
                if ((args === 3 || args == 6) && arg < 100 && arg >= 0) {
                    date.setFullYear(arg);
                }
                date.setTime(date.getTime() + offset);
                return date;
            }
        }(LocalDate);

        window.Date.now = function() {
            return LocalDate.now() + offset;
        };

        if (console.log) console.log([new window.Date, new window.LocalDate])
    }

    function process() {
        offset = samples.reduce(function(a, b) { return (+a)+b}, 0) / samples.length;
        persist[URL] = [LocalDate.now() + (3600*1000), offset];
        localStorage.time_offset = JSON.stringify(persist);
        override();
    }

    function pingServer() {
        var request = new XMLHttpRequest()
            , req, res
            , valid

        request.open("GET", URL + "/ping");
        request.onreadystatechange = function() {
            if (this.readyState == this.HEADERS_RECEIVED) {
                valid = this.status == 200
                res = LocalDate.now()
            }
        };
        request.onload = function() {
            if (!valid) return pingServer();
            var data = JSON.parse(this.response)
            samples.push((data.time + ((res-req)/1)) - res)
            if (samples.length < 10) pingServer();
            else process();
        };
        req = LocalDate.now();
        request.send();
    }
    pingServer();
})(window, timeURL, Date, localStorage);

