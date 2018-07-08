###########################################################################
# Copyright (C) 2015-2018 "IoT.bzh"
#
# author: José Bollo <jose.bollo@iot.bzh>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###########################################################################


INCLUDE_DIRECTORIES(${include_dirs})

##################################################
# AuthLogin
##################################################
ADD_LIBRARY(authLogin MODULE AuthLogin.c)
SET_TARGET_PROPERTIES(authLogin PROPERTIES
	PREFIX ""
	LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/export.map"
)
TARGET_LINK_LIBRARIES(authLogin ${link_libraries})
INSTALL(TARGETS authLogin
        LIBRARY DESTINATION ${binding_install_dir})

##################################################
# ave
##################################################
ADD_LIBRARY(ave MODULE ave.c)
SET_TARGET_PROPERTIES(ave PROPERTIES
	PREFIX ""
	LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/export.map"
)
TARGET_LINK_LIBRARIES(ave ${link_libraries})
INSTALL(TARGETS ave
        LIBRARY DESTINATION ${binding_install_dir})

##################################################
# DemoContext
##################################################
ADD_LIBRARY(demoContext MODULE DemoContext.c)
SET_TARGET_PROPERTIES(demoContext PROPERTIES
	PREFIX ""
	LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/export.map"
)
TARGET_LINK_LIBRARIES(demoContext ${link_libraries})
INSTALL(TARGETS demoContext
        LIBRARY DESTINATION ${binding_install_dir})

##################################################
# DemoPost
##################################################
ADD_LIBRARY(demoPost MODULE DemoPost.c)
SET_TARGET_PROPERTIES(demoPost PROPERTIES
	PREFIX ""
	LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/export.map"
)
TARGET_LINK_LIBRARIES(demoPost ${link_libraries})
INSTALL(TARGETS demoPost
        LIBRARY DESTINATION ${binding_install_dir})

##################################################
# HelloWorld
##################################################
ADD_LIBRARY(helloWorld MODULE HelloWorld.c)
SET_TARGET_PROPERTIES(helloWorld PROPERTIES
	PREFIX ""
	LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/export.map"
)
TARGET_LINK_LIBRARIES(helloWorld ${link_libraries})
INSTALL(TARGETS helloWorld
        LIBRARY DESTINATION ${binding_install_dir})

##################################################
# tic-tac-toe
##################################################
ADD_LIBRARY(tic-tac-toe MODULE tic-tac-toe.c)
SET_TARGET_PROPERTIES(tic-tac-toe PROPERTIES
	PREFIX ""
	LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/export.map"
)
TARGET_LINK_LIBRARIES(tic-tac-toe ${link_libraries})
INSTALL(TARGETS tic-tac-toe
        LIBRARY DESTINATION ${binding_install_dir})

##################################################
# hi3
##################################################
ADD_LIBRARY(hi3 MODULE hi3.c)
SET_TARGET_PROPERTIES(hi3 PROPERTIES
	PREFIX ""
	LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/export.map"
)
TARGET_LINK_LIBRARIES(hi3 ${link_libraries})
INSTALL(TARGETS hi3
        LIBRARY DESTINATION ${binding_install_dir})

##################################################
# hello2
##################################################
ADD_LIBRARY(hello2 MODULE hello2.c)
SET_TARGET_PROPERTIES(hello2 PROPERTIES
	PREFIX ""
	LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/export.map"
)
TARGET_LINK_LIBRARIES(hello2 ${link_libraries})
INSTALL(TARGETS hello2
        LIBRARY DESTINATION ${binding_install_dir})

##################################################
# hello3
##################################################
ADD_LIBRARY(hello3 MODULE hello3.c)
SET_TARGET_PROPERTIES(hello3 PROPERTIES
	PREFIX ""
	LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/export.map"
)
TARGET_LINK_LIBRARIES(hello3 ${link_libraries})
INSTALL(TARGETS hello3
        LIBRARY DESTINATION ${binding_install_dir})

