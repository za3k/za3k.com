#!/usr/bin/ruby
require 'redcarpet'
print "Content-type: text/html\n\n"

markdown = Redcarpet::Markdown.new(
    Redcarpet::Render::HTML,
    :autolink => true,
    :fenced_code_blocks => true,
    :footnotes => true,
    :highlight => true,
    :lax_spacing => true,
    :no_intra_emphasis => true,
    :tables => true,
)

content = markdown.render(File.read(ENV['PATH_TRANSLATED']))

puts %Q{
<html>
<head>
<link rel="stylesheet" type="text/css" href="/markdown.css">
</head>
<body>
#{content}
</body>
</html>
}
