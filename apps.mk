#############################################################################
# Makefile.config - generic configuration makefile
#
# Copyright 2008, Adobe Systems Incorporated.  All rights reserved.
#
###############################################################################

sample_player_psdk : psdk $(dep-all)
sample_player_psdk_deps := psdk $(dep-all)
simple_player_psdk : psdk $(dep-all)
simple_player_psdk_deps := psdk $(dep-all)

.PHONY: all
all : sample_player_psdk simple_player_psdk

.PHONY: mkbuilddirs
mkbuilddirs: sample_player_psdk simple_player_psdk
 
DH_X86_APPS		:= sample_player_psdk simple_player_psdk

$(warning "DH_DDK_MODULES:" $(DH_DDK_MODULES))
$(warning "DH_AVE_WEBKIT_MODULES:" $(DH_AVE_WEBKIT_MODULES))

DH_DDK_MODULES		:= $(DH_DDK_MODULES) 
DH_AVE_WEBKIT_MODULES	:= $(DH_AVE_WEBKIT_MODULES)

define APP_MAKE
$(eval TARGET :=$(1))
$(eval PKG_DIR := $(DH_TARGET_BIN_DIR)/apps/$(TARGET)_pkg)

$(1): mkdirs
		@echo "=======================> Making $(TARGET)" 
		@DH_MODULE=$$@ DH_MODULE_DEPS="$($(TARGET)_deps)" $$(MAKE) --no-print-directory --f $(DH_PLATFORM_MAKEFILE_DIR)/apps/$(TARGET)/build-spec/$(TARGET).mk $(MAKECMDGOALS);

clean-$(TARGET): mkdirs
		@echo "=======================> Cleaning $(TARGET)" 
		@DH_MODULE=$(TARGET) DH_MODULE_DEPS="$($(TARGET)_deps)" $$(MAKE) --no-print-directory --f $(DH_PLATFORM_MAKEFILE_DIR)/apps/$(TARGET)/build-spec/$(TARGET).mk clean; 

rebuild-$(TARGET): mkdirs
		@echo "=======================> Rebuilding $(TARGET)" 
		@DH_MODULE=$(TARGET) DH_MODULE_DEPS="$($(TARGET)_deps)" $$(MAKE) --no-print-directory --f $(DH_PLATFORM_MAKEFILE_DIR)/apps/$(TARGET)/build-spec/$(TARGET).mk clean all; 

build-$(TARGET): mkdirs
		@echo "=======================> Building $(TARGET)" 
		@DH_MODULE=$(TARGET) DH_MODULE_DEPS="$($(TARGET)_deps)" $$(MAKE) --no-print-directory --f $(DH_PLATFORM_MAKEFILE_DIR)/apps/$(TARGET)/build-spec/$(TARGET).mk all; 

endef

# 
#FOREACH module in MODULES
# EVAL 
#	CALL INVOKE_MAKE TARGET 
$(warning "Building components for sample_player_app" $(DH_X86_APPS))
$(foreach app, $(DH_X86_APPS), $(eval $(call APP_MAKE, $(app))) )
$(warning "Completed Building components for sample_player_app")


###############################################################################
#  LOG
###############################################################################
#  05-Nov-12   angela  created
