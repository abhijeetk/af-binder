#!/bin/sh

$R/bin/afb-client -k $WSURL monitor trace '{"add":{"api":"*","request":"*","event":"*","session":"*","global":"*"}}' &

