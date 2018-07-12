#!/bin/sh


curl -s -o /dev/null $URL/index.html
curl -s -o /dev/null $URL/marrus-orthocanna.jpg
curl -s -o /dev/null $URL/test.js
curl -s -o /dev/null $URL/icons/marrus-orthocanna.jpg

curl -s -o /dev/null $URL/fake-file.html

curl -s "$URL/api/salut/ping?arg1=null&arg1=%22a+string%22"
curl -s "$URL/api/hello/ping" \
	-F image=@$R/www/marrus-orthocanna.jpg \
	-F name=test
curl -s -X POST "$URL/api/hello/ping" \
	--header 'content-type: application/json' \
	--data-binary '[null,3,{"hello":false,"salut":4.5},true]'

curl -s "$URL/api/hello/get?name=something&something=nothing"

curl -s -F name=file -F file=@$R/www/marrus-orthocanna.jpg "$URL/api/hello/get"

curl -s -X HEAD -o /dev/null $URL/index.html
#curl -s -X CONNECT -o /dev/null $URL/index.html
curl -s -X DELETE -o /dev/null $URL/index.html
curl -s -X OPTIONS -o /dev/null $URL/index.html
curl -s -X PATCH -o /dev/null $URL/index.html
curl -s -X PUT -o /dev/null $URL/index.html
curl -s -X TRACE -o /dev/null $URL/index.html
