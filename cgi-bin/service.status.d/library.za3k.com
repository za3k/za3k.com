./up? "https://public:babelfish@library.za3k.com/" || exit 1
curl "https://public:babelfish@library.za3k.com/" | grep -q "reading list"
