#!/bin/sh

$R/bin/afb-client -k $WSURL <<EOC &
monitor trace {"add":{"tag":"fun","api":"*","request":"*","event":"*","session":"*","global":"*"}}
monitor trace {"add":{"tag":"T","api":"*","request":"*","event":"*","session":"*","global":"*"}}
monitor trace {"drop":{"tag":"fun"}}
EOC
