Colophon
========

- za3k.com is written in a combination of [HTML](https://en.wikipedia.org/wiki/HTML) and [markdown](https://daringfireball.net/projects/markdown/syntax), using vim.
- za3k.com is managed with the version control system, [git](https://git-scm.com/book/en/v2), and uses a git [server hook](https://git-scm.com/book/en/v2/Customizing-Git-Git-Hooks#Server-Side-Hooks) to check out commits to the correct place to be served.
- I use ruby's [redcarpet](https://github.com/vmg/redcarpet) gem to process markdown into HTML.
- The server runs on one of [prgmr.com](http://prgmr.com)'s xen servers as a guest, using [Debian](https://www.debian.org/). The server uses [nginx](http://nginx.org/) to figure out what virtual server is needed and to strip out HTTPS authentication. za3k.com and blog.za3k.com proper run off [apache2](https://httpd.apache.org/).
- blog.za3k.com runs on [Wordpress](https://wordpress.org/), using the default theme. I use a [postgres](https://www.postgresql.org/) database. I installed the plugins ['Force SSL URL Scheme'](https://gist.github.com/webaware/4688802), and ['Highlight Source Pro'](https://wordpress.org/plugins/highlight-source-pro/).
