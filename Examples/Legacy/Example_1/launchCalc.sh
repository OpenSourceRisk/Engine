#!/bin/bash

/Applications/LibreOffice.app/Contents/MacOS/soffice \
--calc \
--accept="socket,host=localhost,port=2002;urp;StarOffice.ServiceManager" \
run.ods &
