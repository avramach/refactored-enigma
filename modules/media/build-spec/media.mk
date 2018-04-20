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
DH_MODULE_BUILD_SHARED_LIB	:= yes
DH_MODULE_BUILD_STATIC_LIB	:= yes
DH_MODULE_BUILD_EXECUTABLE	:= no

###############################################################################
# STEP 2 - REQUIRED: Specify module source dir and relative source file list
DH_MODULE_SOURCE_DIR		:= $(DH_SOURCE_DIR_AVE)/media/source
DH_MODULE_SOURCE_FILES		:= \
								AsyncAVDecoder.cpp \
								AudioMixer.cpp \
								ColorConverterSoftware.cpp \
								DecodedFrameQueue.cpp \
								DecodedSampleQueue.cpp \
								FileWriter.cpp \
								FlashAccessAdapter.cpp \
								HTTPFileReader.cpp \
								LocalFileReader.cpp \
								MediaComponentFactory.cpp \
								RGBAPlane.cpp \
								VideoDataBuffer.cpp \
								VideoOutputSW.cpp \
								VideoPresenter.cpp \
								VideoPresenterQueue.cpp \
								MediaUtils.cpp \
								YUVPlane.cpp \
								\
								filters/AudioSpeed.cpp \
                                filters/Rate.cpp \
                                \
								adapters/H264Utils.cpp \
								adapters/sndcodec.cpp \
								adapters/SWAudioCommonAdapter.cpp \
								adapters/SyncVideoAdapterBase.cpp \
								\
								parsers/F4FParser.cpp \
								parsers/F4V.cpp \
								parsers/FLVParser.cpp \
								parsers/FragmentedHTTPStreamer.cpp \
								parsers/HlsParser.cpp \
								parsers/M2TS.cpp \
								parsers/Mp4.cpp \
								parsers/MP4Parser.cpp \
								parsers/MPDParser.cpp \
								parsers/ManifestCommon.cpp \
								parsers/UTFConversion.cpp \
								parsers/xmlparser.cpp \
								parsers/CEA_608_708.cpp \
								parsers/URLParser.cpp \

DH_PLATFORM_SOURCE_DIR		:= $(DH_PLATFORM_MAKEFILE_DIR)/modules/media/source
DH_PLATFORM_SOURCE_FILES		:= \
                    RDKAACUtils.cpp \
                    RDKGstAVDecoder.cpp \
                    RdkVideoOutput.cpp \
					RdkVideoCompositor.cpp \
					AudioOutputRdk.cpp \


###############################################################################
# STEP 3 - OPTIONAL: Specify additional target obj sub-dirs that need to be made
#                    (any subdirs referenced in DH_MODULE_SOURCE_FILES)
#
DH_ADDITIONAL_MODULE_OBJ_SUBDIRS := adapters parsers platform/stagecraft filters

###############################################################################
# STEP 4 - OPTIONAL: Modify flags using (:=, +=, patsubst, etc)
#
# DH_CFLAGS_GENERIC	+=
# DH_CFLAGS_DEBUG	+=
# DH_CFLAGS_RELEASE	+=
DH_CXXFLAGS_GENERIC := $(subst -Werror,, $(DH_CXXFLAGS_GENERIC)) \
			-D PLATFORM_HARDWAREAV_AVAILABLE \
			-D PLATFORM_AVOUT_AVAILABLE \
			-D DRM_MODULE_AVAILABLE \
			-D TEXT_MODULE_AVAILABLE \
			-D _DH


DH_LDFLAGS_GST = `pkg-config --libs gstreamer-1.0 gstreamer-app-1.0 glib-2.0 gstreamer-video-1.0` 
# DH_CXXFLAGS_DEBUG	+=
# DH_CXXFLAGS_RELEASE	+=
DH_LDFLAGS_SHAREDLIB	+= $(DH_LIBCURL_CLIENT_LINK_DIRECTIVE) \
                           $(DH_LDFLAGS_GST) \
                           $(DH_LIBOPENSSL_CLIENT_LINK_DIRECTIVE)                            

# DH_LDFLAGS_EXECUTABLE	+=
# DH_ARFLAGS_STATICLIB	+=

DH_CXXFLAGS_GST = `pkg-config --cflags gstreamer-1.0 glib-2.0` -fpermissive
DH_CXXFLAGS_GENERIC += $(DH_WEBKIT_INCLUDES)
DH_CXXFLAGS_GENERIC	+= $(DH_CXXFLAGS_GST) 

DH_CXXFLAGS_GENERIC += 	\
					-I $(DH_SOURCE_DIR_AVE)/net/include \
					-I $(DH_SOURCE_DIR_AVE)/drm/include \
					-I $(DH_SOURCE_DIR_AVE)/text/include \
					-I $(DH_SOURCE_DIR_AVE)/media/include \
					-I $(DH_SOURCE_DIR_AVE)/crypto/include \
					-I $(DH_THIRDPARTY_SOURCE_DIR)/cts \
					-I $(DH_THIRDPARTY_SOURCE_DIR)/cef \
					$(DH_LIBOPENSSL_CLIENT_INCLUDE_DIRECTIVE)

DH_CXXFLAGS_GENERIC += -DAPOLLO -DFLASH_STAGECRAFT
###############################################################################
# STEP 5 - REQUIRED: include DH_MODULE_RULES_MAKEFILE to execute the module build

include $(DH_MODULE_RULES_MAKEFILE)

###############################################################################
#   LOG
###############################################################################
#   15-May-12	aldrin created

