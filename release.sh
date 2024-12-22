#!/bin/bash
rm -rf release
mkdir -p release

cp -rf LTC *.{hpp,cpp,txt,json} LICENSE release/

mv release score-addon-ltc
7z a score-addon-ltc.zip score-addon-ltc
