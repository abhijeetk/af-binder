#!/bin/sh

$R/bin/afb-client -s -e $WSURL <<EOC
monitor get {"verbosity":true}
monitor get {"apis":true}
monitor set {"verbosity":"debug"}
monitor session {"refresh-token":false}
monitor session {"refresh-token":true}
EOC

