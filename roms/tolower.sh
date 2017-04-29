#!/bin/sh
find . -depth -exec rename -v 's/(.*)\/([^\/]*)/$1\/\L$2/' {} \;
