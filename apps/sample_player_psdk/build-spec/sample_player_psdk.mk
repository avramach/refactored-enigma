#*******************************************************************************
# ADOBE SYSTEMS INCORPORATED
# Copyright 2008 Adobe Systems Incorporated
# All Rights Reserved.
#
# NOTICE:  Adobe permits you to use, modify, and distribute this file in
# accordance with the  terms of the Adobe license agreement accompanying it. If
# you have received this file from a source other than Adobe, then your use,
# modification, or distribution of it requires the prior written permission of
# Adobe.
#******************************************************************************/

###############################################################################
# STEP 1 - REQUIRED: Specify what kind of targets to build
$(warning "Karthick in sample_player_sdk")
DH_MODULE_BUILD_SHARED_LIB	:= no
DH_MODULE_BUILD_STATIC_LIB	:= no
DH_MODULE_BUILD_EXECUTABLE	:= yes

###############################################################################
# STEP 2 - REQUIRED: Specify module source dir and relative source file list
DH_MODULE_SOURCE_DIR		:= $(DH_PLATFORM_MAKEFILE_DIR)/apps/sample_player_psdk/
DH_MODULE_SOURCE_FILES		:=					\
								source/sample_player_psdk.cpp \

###############################################################################
# STEP 3 - OPTIONAL: Specify additional target obj sub-dirs that need to be made
#                    (any subdirs referenced in DH_MODULE_SOURCE_FILES)
#
DH_ADDITIONAL_MODULE_OBJ_SUBDIRS := source 

###############################################################################
# STEP 4 - OPTIONAL: Modify flags using (:=, +=, patsubst, etc)
#
# DH_CFLAGS_GENERIC	+=
# DH_CFLAGS_DEBUG	+=
# DH_CFLAGS_RELEASE	+=
DH_CXXFLAGS_GENERIC	+= $(DH_DH_GTEST_INCLUDE_DIRECTIVE) 
DH_CXXFLAGS_GENERIC	+= $(DH_LIBCURL_CLIENT_INCLUDE_DIRECTIVE) \
                       $(DH_LIBOPENSSL_CLIENT_INCLUDE_DIRECTIVE) -I$(DH_SOURCE_DIR_AVE)/psdk/include -I$(DH_SOURCE_DIR_AVE)/psdkutils/include -I$(DH_SOURCE_DIR_AVE)/auditude/include 

DH_CXXFLAGS_GST = `pkg-config --cflags gstreamer-1.0 glib-2.0` -fpermissive
DH_CXXFLAGS_GENERIC	+= $(DH_CXXFLAGS_GST) 
DH_CXXFLAGS_GENERIC := $(subst -Werror,,$(DH_CXXFLAGS_GENERIC))
DH_LDFLAGS_GST = `pkg-config --libs gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0 glib-2.0` 
# DH_CXXFLAGS_DEBUG	+=
# DH_CXXFLAGS_RELEASE	+=
# DH_LDFLAGS_SHAREDLIB	+=
DH_LDFLAGS_EXECUTABLE   := -L $(DH_TARGET_LIB_DIR) -lpsdk -lmedia -lportingkit -ldrm -ltext -lcts -lfilesystem -lnet -lkernel -lSafeAPIs -lpthread -lrt -lcrypto -lcryptoModule -lxmlreader -ltinyxml2 -lauditude -lpsdkutils -lzlib $(DH_LIBCURL_CLIENT_LINK_DIRECTIVE) $(DH_LIBOPENSSL_CLIENT_LINK_DIRECTIVE) $(DH_LDFLAGS_EXECUTABLE) $(DH_LDFLAGS_GST) \

# DH_ARFLAGS_STATICLIB	+=


###############################################################################
# STEP 5 - REQUIRED: include DH_MODULE_RULES_MAKEFILE to execute the module build


include $(DH_MODULE_RULES_MAKEFILE)

###############################################################################
#   LOG
###############################################################################
#   05-Nov-12	angela  	created

