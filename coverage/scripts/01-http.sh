#!/bin/sh


curl $URL/index.html
curl $URL/marrus-orthocanna.jpg
curl $URL/test.js
curl $URL/icons/marrus-orthocanna.jpg

curl $URL/fake-file.html

curl "$URL/api/salut/ping?arg1=null&arg1=%22a+string%22"
curl "$URL/api/hello/ping" \
	-F image=@$R/www/marrus-orthocanna.jpg \
	-F name=test
curl -X POST "$URL/api/hello/ping" \
	--header 'content-type: application/json' \
	--data-binary '[null,3,{"hello":false,"salut":4.5},true]'
