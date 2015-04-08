Colophon
========

- za3k.com is written in a combination of HTML and markdown, using vim.
- za3k.com is managed with the version control system, git, and uses a git server hook to check out commits to the correct place to be served.
- I use ruby's redcarpet gem to process markdown into HTML.
- The server runs on one of prgmr.com's xen servers as a guest, using Debian. The server uses nginx to figure out what virtual server is needed and to strip out HTTPS authentication. za3k.com and blog.za3k.com proper run off apache2.
- blog.za3k.com runs on Wordpress, using the default theme. I use a postgres database. I installed the plugins 'Force SSL URL Scheme', 'Highlight Source Pro', and 'Askismet' for comment spam.
