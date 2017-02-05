Colophon
========

- za3k.com is written in a combination of [HTML](https://en.wikipedia.org/wiki/HTML) and [markdown](https://daringfireball.net/projects/markdown/syntax), using vim.
- za3k.com is managed with the version control system, [git](https://git-scm.com/book/en/v2), and uses a git [server hook](https://git-scm.com/book/en/v2/Customizing-Git-Git-Hooks#Server-Side-Hooks) to check out commits to the correct place to be served. The [source](https://github.com/za3k/za3k.com) is mirrored on github (another git server hook).
- I use ruby's [redcarpet](https://github.com/vmg/redcarpet) gem to process markdown into HTML.
- The server runs on one of [prgmr.com](http://prgmr.com)'s xen servers as a guest, using [Debian](https://www.debian.org/). The server is [nginx](http://nginx.org/). The [source](https://github.com/za3k/devops) for my server setup in managed in [fabric](http://www.fabfile.org/).
- blog.za3k.com runs on [Wordpress](https://wordpress.org/), using the default theme. I use a [postgres](https://www.postgresql.org/) database. I installed the plugins ['Force SSL URL Scheme'](https://gist.github.com/webaware/4688802), and ['Highlight Source Pro'](https://wordpress.org/plugins/highlight-source-pro/).
