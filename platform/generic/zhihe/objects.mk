#
# SPDX-License-Identifier: BSD-2-Clause
#

carray-platform_override_modules-$(CONFIG_PLATFORM_ZHIHE_P100) += zhihe_p100
platform-objs-$(CONFIG_PLATFORM_ZHIHE_P100) += zhihe/p100.o
