###########################################################################
# Copyright 2015, 2016, 2017 IoT.bzh
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

