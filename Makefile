# Makefile for the sntp_example program

PROGRAM=dripirrigationcontrol.c
EXTRA_CFLAGS=-DLWIP_HTTPD_CGI=1 -DLWIP_HTTPD_SSI=1 -I./fsdata
EXTRA_COMPONENTS = extras/sntp extras/mbedtls extras/httpd extras/ds3231 extras/i2c

include ../common.mk

html:
	@echo "Generating fsdata.."
	cd fsdata && ./makefsdata


